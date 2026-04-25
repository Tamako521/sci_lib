#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Database;

struct AuthorStat {
    std::string author;
    size_t paper_count;
};

struct KeywordStat {
    std::string keyword;
    size_t count;
};

using YearKeywordTop = std::unordered_map<std::string, std::vector<KeywordStat>>;

class StatisticsAnalyzer {
private:
    static bool is_missing_string(const std::string& value);
    static std::vector<std::string> split_title_keywords(const std::string& title);
    static bool is_stop_word(const std::string& word);
public:
    std::vector<AuthorStat> top_authors(const Database& db, size_t limit = 100) const;//返回结果按照出现次数size降序排列
    YearKeywordTop yearly_hot_keywords(const Database& db, size_t limit = 10) const;
};
