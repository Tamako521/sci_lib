#include "statistics_analyzer.hpp"
#include "common/database.hpp"
#include "common/xml_value.hpp"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <queue>
#include <utility>

bool StatisticsAnalyzer::is_missing_string(const std::string &value){
    return value==MISSING_STRING;
}

void StatisticsAnalyzer::count_title_keywords(const std::string &title, std::unordered_map<std::string, size_t>& counts){
    std::string word;
    word.reserve(32);

    for(char raw_ch:title){
        unsigned char ch = static_cast<unsigned char>(raw_ch);
        if(std::isalnum(ch)){
            word.push_back(static_cast<char>(std::tolower(ch)));
        }
        else if(!word.empty()){
            if(!is_stop_word(word)){
                counts[word]++;
            }
            word.clear();
        }
    }

    if(!word.empty()&&!is_stop_word(word)){
        counts[word]++;
    }
}

bool StatisticsAnalyzer::is_stop_word(const std::string &word){
    static const std::unordered_set<std::string> stop_words = {
        "an", "the",
        "of", "and", "or",
        "in", "on", "at", "to", "for", "from", "by", "with", "without",
        "as", "is", "are", "was", "were", "be", "been", "being",
        "this", "that", "these", "those",
        "it", "its",
        "into", "over", "under", "between", "among",
        "than", "then",
        "using", "use", "used",
        "based", "via"
    };
    const bool is_number = std::all_of(word.begin(), word.end(), [](unsigned char ch) {
        return std::isdigit(ch);
    });
    return (word.size() < 2||is_number||(stop_words.find(word)!=stop_words.end()));
}


//
std::vector<AuthorStat> StatisticsAnalyzer::top_authors(const Database &db, size_t limit) const{
    if(limit==0){
        return {};
    }
    //计数
    std::unordered_map<std::string,size_t>author_counts;
    const std::vector<XmlValue>& records=db.all();
    author_counts.reserve(records.size());
    for(const XmlValue& val:records){
        for(size_t i=0;i<val.author_count();i++){
            const std::string& author=val.author_at(i);
            if(!is_missing_string(author)){
                author_counts[author]++;
            }
        }
    }

    //小根堆维护
    auto cmp=[](const AuthorStat &a,const AuthorStat &b){
        if(a.paper_count!=b.paper_count)
            return a.paper_count>b.paper_count;
        return a.author<b.author;
    };
    std::priority_queue<AuthorStat,std::vector<AuthorStat>,decltype(cmp)> min_heap(cmp);

    for(const auto &[author,count]:author_counts){
        min_heap.push({author,count});
        if(min_heap.size()>limit){
            min_heap.pop();
        }
    }

    //取出结果并按论文数降序排列
    std::vector<AuthorStat> result;
    result.reserve(min_heap.size());
    while(!min_heap.empty()){
        result.push_back(min_heap.top());
        min_heap.pop();
    }
    std::sort(result.begin(),result.end(),[](const AuthorStat& a,const AuthorStat& b){
        if(a.paper_count!=b.paper_count)
            return a.paper_count>b.paper_count;
        return a.author<b.author;
    });

    return result;
}

//
YearKeywordTop StatisticsAnalyzer::yearly_hot_keywords(const Database &db, size_t limit) const{
    if(limit==0){
        return {};
    }
    //遍历
    std::unordered_map<std::string,std::unordered_map<std::string,size_t>> keyword_counts;
    const std::vector<XmlValue>& records=db.all();
    keyword_counts.reserve(256);
    for(const XmlValue& val:records){
        const std::string& year=val.year();
        if(is_missing_string(year)) continue;
        const std::string& title=val.title();
        if(is_missing_string(title)) continue;

        count_title_keywords(title, keyword_counts[year]);
    }

    YearKeywordTop result;
    for(const auto& [year,counts]:keyword_counts){
        auto cmp=[](const KeywordStat &a,const KeywordStat &b){
            if(a.count!=b.count)
                return a.count>b.count;
            return a.keyword<b.keyword;
        };
        std::priority_queue<KeywordStat,std::vector<KeywordStat>,decltype(cmp)> min_heap(cmp);

        for(const auto& [keyword,count]:counts){
            min_heap.push({keyword,count});
            if(min_heap.size()>limit){
                min_heap.pop();
            }
        }

        std::vector<KeywordStat> top_keywords;
        top_keywords.reserve(min_heap.size());
        while(!min_heap.empty()){
            top_keywords.push_back(min_heap.top());
            min_heap.pop();
        }
        std::sort(top_keywords.begin(),top_keywords.end(),cmp);

        result[year]=std::move(top_keywords);
    }
    return result;
}
