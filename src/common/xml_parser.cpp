#include "common\xml_parser.hpp"
#include "common\process_resporter.hpp"

#include <algorithm>
#include <filesystem>

static std::string strip_inline_tags(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    bool in_tag = false;
    for (char ch : text) {
        if (ch == '<') {
            in_tag = true;
            continue;
        }
        if (ch == '>') {
            in_tag = false;
            continue;
        }
        if (!in_tag)
            result.push_back(ch);
    }

    return result;
}


bool XmlParser::eof() const{
    return (pos_==buffer_.size())&&(file_.rdstate() & std::ios::eofbit);
}

bool XmlParser::match(const std::string &prefix){
    if(pos_+prefix.size()<=buffer_.size()){
        if(!buffer_.compare(pos_,prefix.size(),prefix)){
            pos_+=prefix.size();
            return true;
        }
        return false;
    }
    return false;
}


ParseResult XmlParser::open_file(const std::string &xml_path)
{
    buffer_.clear();
    pos_=0;
    buffer_offset_=0;
    if(file_.is_open()) file_.close();
    file_.open(xml_path,std::ios::binary);
    if(file_.is_open()) return ParseResult::OK;
    ERROR("无法打开XML文件: " << xml_path);
    return ParseResult::FILE_OPEN_FAILED;
}

size_t XmlParser::find_last_safe_boundary() const{
    size_t res=buffer_.rfind("</article>",(pos_>0?pos_-1:0));
    if(res!=std::string::npos) return res+10;
    return res;
}

bool XmlParser::refill()
{
    size_t safe;
    if((safe=find_last_safe_boundary())==std::string::npos) safe=pos_;

    buffer_.erase(0,safe);
    pos_-=safe;
    buffer_offset_+=safe;

    static std::vector<char> buf(CHUNK_SIZE);
    file_.read(buf.data(),CHUNK_SIZE);
    buffer_.append(buf.data(),file_.gcount());

    return buffer_.size()>pos_;
}

bool XmlParser::ensure_complete_article(){
    size_t p;
    while(1){
        p=buffer_.find("</article>",pos_);
        if(p!=std::string::npos) return true;
        if(!refill()) return false;
    }
}

size_t XmlParser::absolute_pos() const {
    return buffer_offset_ + pos_;
}

std::string XmlParser::context(size_t pos) const {
    const size_t begin = (pos > 80) ? pos - 80 : 0;
    const size_t end = std::min(buffer_.size(), pos + 120);
    std::string text = buffer_.substr(begin, end - begin);
    for (char& ch : text) {
        if (ch == '\n' || ch == '\r' || ch == '\t')
            ch = ' ';
    }
    return text;
}

void XmlParser::skip_whitespace(){
    while(pos_<buffer_.size()&&(buffer_[pos_]=='\t'||buffer_[pos_]=='\n'||buffer_[pos_]==' '||buffer_[pos_]=='\r')){
        pos_++;
    }
}

void XmlParser::skip_prologue(){
    while(1){
        skip_whitespace();
        if(pos_+1 >= buffer_.size()) return;

        if(buffer_[pos_]=='<' && buffer_[pos_+1]=='?')
            pos_ = buffer_.find("?>", pos_) + 2;
        else if(buffer_[pos_]=='<' && buffer_[pos_+1]=='!')
            pos_ = buffer_.find(">", pos_) + 1;
        else
            return; 
    }
}
void XmlParser::skip_until_article(){
    size_t p = buffer_.find("<article ", pos_);
    if (p == std::string::npos) {
        pos_ = buffer_.size();   // 推到末尾，上层会触发 refill
        return;
    }
    pos_ = p;
}



ParseResult XmlParser::parse_tag(std::string &tag_name, std::string &text){
    //解析'<'
    if(pos_ >= buffer_.size() || buffer_.at(pos_)!='<') {
        ERROR("解析标签失败: 当前位置不是 '<', offset=" << absolute_pos()
            << ", context=\"" << context(pos_) << "\"");
        return ParseResult::MALFORMED_XML;
    }
    pos_++;
    //解析tag
    size_t start=pos_ , gt=buffer_.find('>',pos_);
    if(gt == std::string::npos) {
        ERROR("解析标签失败: 找不到 '>', offset=" << absolute_pos()
            << ", context=\"" << context(pos_) << "\"");
        return ParseResult::MALFORMED_XML;
    }
    tag_name=buffer_.substr(start,gt-start);
    size_t space = tag_name.find_first_of(" \t\r\n");
    tag_name = tag_name.substr(0, space);
    pos_=gt+1;
    //解析文本：字段内部可能包含 <sub>...</sub> 这类嵌套标签，必须查找当前字段自己的结束标签
    start=pos_;
    const std::string end_tag = "</" + tag_name + ">";
    gt=buffer_.find(end_tag,pos_);
    if(gt == std::string::npos) {
        ERROR("解析标签失败: 找不到当前标签结束标记, tag=" << tag_name
            << ", expect=" << end_tag
            << ", offset=" << absolute_pos() << ", context=\"" << context(pos_) << "\"");
        return ParseResult::MALFORMED_XML;
    }
    text=strip_inline_tags(buffer_.substr(start,gt-start));
    pos_=gt;
    //跳过结尾tag
    if(!match("</"+tag_name+">")) {
        ERROR("解析标签失败: 起止标签不匹配, tag=" << tag_name
            << ", offset=" << absolute_pos() << ", context=\"" << context(pos_) << "\"");
        return ParseResult::MALFORMED_XML;
    }
    return ParseResult::OK;
}

ParseResult XmlParser::parse_attributes(XmlValue &out, StringPool &string_pool){
    while(1){
        skip_whitespace();
        if(pos_ >= buffer_.size()) {
            ERROR("解析属性失败: 意外到达缓冲区末尾, offset=" << absolute_pos());
            return ParseResult::UNEXPECTED_EOF;
        }
        if(buffer_.at(pos_)=='>') {
            pos_++;
            return ParseResult::OK;
        }
        size_t eq=buffer_.find("=",pos_);
        if(eq == std::string::npos) {
            ERROR("解析属性失败: 找不到 '=', offset=" << absolute_pos()
                << ", context=\"" << context(pos_) << "\"");
            return ParseResult::MALFORMED_XML;
        }
        std::string key=buffer_.substr(pos_,eq-pos_);
        //解析value
        pos_=eq+2;//推进到”之后
        eq=buffer_.find("\"",pos_);
        if(eq == std::string::npos) {
            ERROR("解析属性失败: 找不到属性值结尾引号, key=" << key
                << ", offset=" << absolute_pos() << ", context=\"" << context(pos_) << "\"");
            return ParseResult::MALFORMED_XML;
        }
        std::string value=buffer_.substr(pos_,eq-pos_);

        if(key=="mdate")
            out.mdate_id_=string_pool.intern(value);
        else if(key=="key")
            out.key_id_=string_pool.intern(value);

        pos_=eq+1;//推进到”之后
    }
}

ParseResult XmlParser::parse_article(StringPool& string_pool, XmlValue& out, const Database* db) {
    ParseResult ret;
    if (!match("<article")) {
        ERROR("解析article失败: 未匹配到 <article, offset=" << absolute_pos()
            << ", context=\"" << context(pos_) << "\"");
        return ParseResult::MALFORMED_XML;
    }

    if ((ret = parse_attributes(out, string_pool)) != ParseResult::OK) {
        ERROR("解析article属性失败: result=" << parse_result_name(ret)
            << ", offset=" << absolute_pos() << ", context=\"" << context(pos_) << "\"");
        return ret;
    }

    while (true) {
        skip_whitespace();

        
        if (pos_ + 1 < buffer_.size() &&
            buffer_[pos_] == '<' && buffer_[pos_+1] == '/') {
            if (!match("</article>")) {
                ERROR("解析article失败: article结束标签异常, offset=" << absolute_pos()
                    << ", context=\"" << context(pos_) << "\"");
                return ParseResult::MALFORMED_XML;
            }
            out.db_ = db;
            return ParseResult::OK;
        }

        std::string tag_name, text;
        if ((ret = parse_tag(tag_name, text)) != ParseResult::OK) {
            ERROR("解析article字段失败: result=" << parse_result_name(ret)
                << ", offset=" << absolute_pos() << ", context=\"" << context(pos_) << "\"");
            return ret;
        }

        if      (tag_name == "author")  out.author_ids_.push_back(string_pool.intern(text));
        else if (tag_name == "title")   out.title_id_   = string_pool.intern(text);
        else if (tag_name == "journal") out.journal_id_ = string_pool.intern(text);
        else if (tag_name == "volume")  out.volume_id_  = string_pool.intern(text);
        else if (tag_name == "month")   out.month_id_   = string_pool.intern(text);
        else if (tag_name == "year")    out.year_id_    = string_pool.intern(text);
        else if (tag_name == "cdrom")   out.cdrom_ids_.push_back(string_pool.intern(text));
        else if (tag_name == "ee")      out.ee_ids_.push_back(string_pool.intern(text));
        // 未知 tag 直接忽略
    }
}


ParseResult XmlParser::parse(const std::string &xml_path, StringPool &string_pool, std::vector<XmlValue> &records, const Database *db){
    ParseResult ret;
    if((ret=open_file(xml_path))!=ParseResult::OK) {
        ERROR("打开XML失败: result=" << parse_result_name(ret) << ", path=" << xml_path);
        return ret;
    }
    const size_t file_size = static_cast<size_t>(std::filesystem::file_size(xml_path));
    ProcessReporter reporter("解析XML", file_size, CHUNK_SIZE);

    refill();

    skip_prologue();
    

    while(1){
        skip_until_article();
        if (pos_ >= buffer_.size()) {
            if (!refill()) break;   // 文件读完了也没找到，正常退出
            continue;               // 重新 skip_until_article
        }

        if(!ensure_complete_article()) {
            ERROR("解析失败: article不完整, 已解析记录数=" << records.size()
                << ", offset=" << absolute_pos() << ", context=\"" << context(pos_) << "\"");
            return ParseResult::ARTICLE_INCOMPLETE;
        }

        XmlValue out;
        if((ret=parse_article(string_pool,out,db))!=ParseResult::OK) {
            ERROR("解析失败: result=" << parse_result_name(ret)
                << ", 已解析记录数=" << records.size()
                << ", offset=" << absolute_pos()
                << ", context=\"" << context(pos_) << "\"");
            return ret;
        }

        records.push_back(std::move(out));
        reporter.report(absolute_pos(), "records=" + std::to_string(records.size()));
    }
    reporter.finish("records=" + std::to_string(records.size()));
    return ParseResult::OK;
}
