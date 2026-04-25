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

std::vector<std::string> StatisticsAnalyzer::split_title_keywords(const std::string &title){
    std::string str=title;
    for(size_t i=0;i<str.size();i++){
        unsigned char ch = static_cast<unsigned char>(str[i]);
        if(std::isalpha(ch)) str[i]=static_cast<char>(std::tolower(ch));
        else str[i]=' ';
    }
    std::vector<std::string> keywords;

    bool isword=false;
    size_t start=0;
    for(size_t i=0;i<str.size();i++){
        unsigned char ch = static_cast<unsigned char>(str[i]);
        if((!isword)&&std::isalpha(ch)){
           isword=true;
           start=i;
        }
        if(isword&&(!std::isalpha(ch))){
            std::string s=str.substr(start,i-start);
            if(!is_stop_word(s)) keywords.push_back(s);
            isword=false;
        }
    }
    if(isword){
        std::string s=str.substr(start);
        if(!is_stop_word(s)) keywords.push_back(s);
    }
    return keywords;
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
    return (word.size() < 2||(stop_words.find(word)!=stop_words.end()));
}


//
std::vector<AuthorStat> StatisticsAnalyzer::top_authors(const Database &db, size_t limit) const{
    if(limit==0){
        return {};
    }
    //计数
    std::unordered_map<std::string,size_t>author_counts;
    const std::vector<XmlValue>& records=db.all();
    for(const XmlValue& val:records){
        std::vector<std::string> authors=val.authors();
        for(const std::string& author:authors){
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
    for(const XmlValue& val:records){
        std::string year=val.year();
        if(is_missing_string(year)) continue;
        std::string title=val.title();
        if(is_missing_string(title)) continue;

        std::vector<std::string>keywords=split_title_keywords(title);
        for(const std::string& keyword:keywords){
            keyword_counts[year][keyword]++;
        }
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
