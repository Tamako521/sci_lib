#include "common\database.hpp"
#include "common\xml_parser.hpp"
#include "common\serializer.hpp"
#include "common\process_resporter.hpp"
#include <filesystem>
#include <algorithm>
#include <cctype>

ParseResult Database::load(const std::string& xml_path){
    ParseResult ret;
    const std::string cache_path = std::filesystem::path(xml_path).parent_path().string() + "/articles.dat";
    Serializer ser;
    if(!std::filesystem::exists(cache_path)){//还未解析并建立数据文件
        string_pool_=StringPool{};
        records_.clear();
        XmlParser parser;
        if((ret=parser.parse(xml_path,string_pool_,records_,this))!=ParseResult::OK){
            ERROR("解析失败: result=" << parse_result_name(ret) << ", path=" << xml_path);
            return ret;
        }
        rebuild_key_index();
        if((ret=ser.save(cache_path,string_pool_,records_))!=ParseResult::OK){
            ERROR("保存缓存失败: result=" << parse_result_name(ret) << ", cache=" << cache_path);
            return ret;
        }
        return ParseResult::OK;
    }
    else{//若已建立数据文件，则加载
        if((ret=ser.load(cache_path,string_pool_,records_,this))!=ParseResult::OK){
            ERROR("load失败: result=" << parse_result_name(ret) << ", cache=" << cache_path);
            return ret;
        }
        rebuild_key_index();
    }
    return ParseResult::OK;
}

void Database::rebuild_key_index(){
    key_index_.clear();
    clear_lazy_indices();

    ProcessReporter reporter("建立key索引", records_.size());
    for(size_t i=0;i<records_.size();i++){
        key_index_.insert({records_[i].key(),i});
        reporter.report(i + 1);
    }
}

void Database::clear_lazy_indices(){
    author_index_.clear();
    year_index_.clear();
    author_index_ready_ = false;
    year_index_ready_ = false;
}

void Database::ensure_author_index() const{
    if(author_index_ready_)
        return;

    author_index_.clear();
    ProcessReporter reporter("建立作者索引", records_.size());
    for(size_t i=0;i<records_.size();i++){
        for (const std::string& a : records_[i].authors())
            author_index_[a].push_back(i);
        reporter.report(i + 1);
    }
    author_index_ready_ = true;
}

void Database::ensure_year_index() const{
    if(year_index_ready_)
        return;

    year_index_.clear();
    ProcessReporter reporter("建立年份索引", records_.size());
    for(size_t i=0;i<records_.size();i++){
        year_index_[records_[i].year()].push_back(i);
        reporter.report(i + 1);
    }
    year_index_ready_ = true;
}

const std::vector<XmlValue>& Database::all() const{return records_;}
size_t Database::size() const{return records_.size();}

const XmlValue* Database::find_by_key(const std::string &key) const{
    auto it = key_index_.find(key);
    if(it == key_index_.end())
        return nullptr;
    return &records_.at(it->second);
}

//精确查询
std::vector<const XmlValue *> Database::find_by_author(const std::string &author) const{
    ensure_author_index();

    auto it=author_index_.find(author);
    if(it==author_index_.end())
        return {};
    std::vector<const XmlValue*> result;
    result.reserve(it->second.size());
    for(size_t idx : it->second)
        result.push_back(&records_.at(idx));
    return result;
}

std::vector<const XmlValue *> Database::find_by_year(const std::string& year) const{
    ensure_year_index();

    auto it=year_index_.find(year);
    if(it==year_index_.end()){
        return {};
    }
    std::vector<const XmlValue*> result;
    result.reserve(it->second.size());
    for(size_t idx:it->second)
        result.push_back(&records_.at(idx));
    return result;
}


// KMP 前缀函数，计算模式串每个位置的最长相等前后缀长度
static std::vector<int> build_prefix(const std::string& pattern) {
    int m = static_cast<int>(pattern.size());
    std::vector<int> pi(m, 0);
    for (int i = 1; i < m; ++i) {
        int j = pi[i - 1];
        while (j > 0 && pattern[i] != pattern[j])
            j = pi[j - 1];
        if (pattern[i] == pattern[j])
            ++j;
        pi[i] = j;
    }
    return pi;
}

// KMP 子串查找
static bool kmp_search(const std::string& text, const std::string& pattern) {
    int n = static_cast<int>(text.size()), m = static_cast<int>(pattern.size());
    if (m == 0) return true;
    if (n < m) return false;
    
    std::vector<int> pi = build_prefix(pattern);
    int j = 0;
    for (int i = 0; i < n; ++i) {
        while (j > 0 && text[i] != pattern[j])
            j = pi[j - 1];
        if (text[i] == pattern[j])
            ++j;
        if (j == m)
            return true;  // 找到匹配
    }
    return false;
}

// 模糊查询
std::vector<const XmlValue *> Database::find_by_title_keyword(const std::string &keyword){
    std::vector<const XmlValue*> result;
    
    std::string lower_keyword = keyword;
    std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    
    for (const XmlValue& record : records_) {
        std::string lower_title = static_cast<std::string>(record.title());
        std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (kmp_search(lower_title, lower_keyword)) {
            result.push_back(&record);
        }
    }
    
    return result;
}

const std::string &Database::get_string(uint32_t id) const
{
    return string_pool_.get(id);
}
