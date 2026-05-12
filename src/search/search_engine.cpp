#include "search/search_engine.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

// ========== SearchEngine 实现 ==========

SearchEngine::SearchEngine(const Database* db) : db_(db) {
    if (db_ == nullptr) {
        throw std::invalid_argument("SearchEngine requires a valid Database pointer");
    }
}

static std::string trim_copy(const std::string& str) {
    size_t begin = 0;
    while (begin < str.size() && std::isspace(static_cast<unsigned char>(str[begin]))) {
        ++begin;
    }

    size_t end = str.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }

    return str.substr(begin, end - begin);
}

// ========== F1: 基本搜索 ==========

std::vector<SearchResult> SearchEngine::search_by_author(const std::string& author) const {
    std::vector<SearchResult> results;
    
    auto articles = db_->find_by_author(author);
    results.reserve(articles.size());
    
    for (const XmlValue* article : articles) {
        results.emplace_back(article, "author", author);
    }
    
    return results;
}

std::vector<const XmlValue*> SearchEngine::find_exact_title(const std::string& title) const {
    std::vector<const XmlValue*> matches;
    const std::string lower_title = to_lowercase(title);
    
    for (const XmlValue& article : db_->all()) {
        if (to_lowercase(article.title()) == lower_title) {
            matches.push_back(&article);
        }
    }
    return matches;
}

std::vector<SearchResult> SearchEngine::search_by_title(const std::string& title) const {
    std::vector<SearchResult> results;
    
    auto matches = find_exact_title(title);
    results.reserve(matches.size());
    
    for (const XmlValue* article : matches) {
        results.emplace_back(article, "title", title);
    }
    
    return results;
}

// ========== F5: 部分匹配搜索 ==========

// KMP 前缀函数
static std::vector<int> build_kmp_prefix(const std::string& pattern) {
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
static bool kmp_match(const std::string& text, const std::string& pattern) {
    int n = static_cast<int>(text.size()), m = static_cast<int>(pattern.size());
    if (m == 0) return true;
    if (n < m) return false;
    
    std::vector<int> pi = build_kmp_prefix(pattern);
    int j = 0;
    for (int i = 0; i < n; ++i) {
        while (j > 0 && text[i] != pattern[j])
            j = pi[j - 1];
        if (text[i] == pattern[j])
            ++j;
        if (j == m)
            return true;
    }
    return false;
}

// 朴素子串匹配（用于较短文本或关键词匹配）
static bool naive_match(const std::string& text, const std::string& pattern) {
    if (pattern.empty()) return true;
    if (text.size() < pattern.size()) return false;
    
    for (size_t i = 0; i <= text.size() - pattern.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (text[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

std::vector<const XmlValue*> SearchEngine::find_fuzzy_keyword(const std::string& keyword) const {
    std::vector<const XmlValue*> matches;
    const std::string lower_keyword = to_lowercase(keyword);

    if (lower_keyword.empty()) {
        return matches;
    }
    
    for (const XmlValue& article : db_->all()) {
        const std::string lower_title = to_lowercase(article.title());
        
        // 使用 KMP 算法进行模糊匹配
        if (kmp_match(lower_title, lower_keyword) || 
            naive_match(lower_title, lower_keyword)) {
            matches.push_back(&article);
        }
    }
    
    return matches;
}

std::vector<SearchResult> SearchEngine::search_by_keyword(const std::string& keyword, 
                                                          SearchMode mode) const {
    std::vector<SearchResult> results;
    const std::string normalized_keyword = trim_copy(keyword);

    if (normalized_keyword.empty()) {
        return results;
    }
    
    if (mode == SearchMode::EXACT) {
        // 精确匹配：完整标题匹配
        auto articles = find_exact_title(normalized_keyword);
        results.reserve(articles.size());
        for (const XmlValue* article : articles) {
            results.emplace_back(article, "keyword_exact", normalized_keyword, 2);
        }
    } else {
        // 模糊匹配
        auto matches = find_fuzzy_keyword(normalized_keyword);
        results.reserve(matches.size());
        for (const XmlValue* article : matches) {
            // 基础得分
            size_t score = 1;
            
            // 根据匹配位置计算额外得分（标题开头的匹配得分更高）
            const std::string lower_title = to_lowercase(article->title());
            const std::string lower_keyword = to_lowercase(normalized_keyword);
            size_t pos = lower_title.find(lower_keyword);
            if (pos != std::string::npos) {
                score += (pos == 0) ? 2 : 1;  // 标题开头匹配 +2，否则 +1
            }
            
            results.emplace_back(article, "keyword_fuzzy", normalized_keyword, score);
        }
    }
    
    // 按相关性排序
    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    return results;
}

std::vector<SearchResult> SearchEngine::search_by_keywords(
    const std::vector<std::string>& keywords, bool match_all) const {
    
    std::vector<std::string> normalized_keywords;
    normalized_keywords.reserve(keywords.size());
    for (const std::string& keyword : keywords) {
        std::string normalized = trim_copy(keyword);
        if (!normalized.empty()) {
            normalized_keywords.push_back(normalized);
        }
    }

    if (normalized_keywords.empty()) {
        return {};
    }
    
    std::vector<SearchResult> results;
    std::unordered_map<const XmlValue*, size_t> score_map;
    
    // 对每个关键词进行模糊搜索
    for (const std::string& keyword : normalized_keywords) {
        auto matches = find_fuzzy_keyword(keyword);
        const std::string lower_keyword = to_lowercase(keyword);
        
        for (const XmlValue* article : matches) {
            size_t keyword_score = 1;
            
            // 计算该关键词在此论文中的得分
            const std::string lower_title = to_lowercase(article->title());
            size_t pos = lower_title.find(lower_keyword);
            if (pos != std::string::npos) {
                keyword_score += (pos == 0) ? 2 : 1;
            }
            
            if (match_all) {
                // AND 模式：必须有所有关键词匹配
                score_map[article] += keyword_score;
            } else {
                // OR 模式：累加得分
                score_map[article] += keyword_score;
            }
        }
    }
    
    // 如果是 AND 模式，过滤掉不包含所有关键词的结果
    if (match_all) {
        for (const auto& pair : score_map) {
            // 获取论文标题
            const std::string lower_title = to_lowercase(pair.first->title());
            
            // 检查是否所有关键词都匹配
            bool all_match = true;
            for (const std::string& keyword : normalized_keywords) {
                if (lower_title.find(to_lowercase(keyword)) == std::string::npos) {
                    all_match = false;
                    break;
                }
            }
            
            if (all_match) {
                std::string matched_str;
                for (size_t i = 0; i < normalized_keywords.size(); ++i) {
                    matched_str += normalized_keywords[i];
                    if (i < normalized_keywords.size() - 1) matched_str += ", ";
                }
                results.emplace_back(pair.first, "keywords_and", matched_str, pair.second);
            }
        }
    } else {
        // OR 模式：直接添加所有结果
        for (const auto& pair : score_map) {
            std::string matched_str;
            for (size_t i = 0; i < normalized_keywords.size(); ++i) {
                if (to_lowercase(pair.first->title()).find(to_lowercase(normalized_keywords[i])) != std::string::npos) {
                    if (!matched_str.empty()) matched_str += ", ";
                    matched_str += normalized_keywords[i];
                }
            }
            results.emplace_back(pair.first, "keywords_or", matched_str, pair.second);
        }
    }
    
    // 按相关性排序
    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    return results;
}



// ========== 智能搜索 ==========

std::vector<SearchResult> SearchEngine::smart_search(const std::string& query) const {
    std::string trimmed = trim_copy(query);
    
    if (trimmed.empty()) {
        return {};
    }
    
    // 检查前缀
    if (trimmed.rfind("author:", 0) == 0) {
        // 按作者搜索
        std::string author = trim_copy(trimmed.substr(7));
        return search_by_author(author);
    }
    
    if (trimmed.rfind("title:", 0) == 0) {
        // 按标题搜索
        std::string title = trim_copy(trimmed.substr(6));
        return search_by_title(title);
    }
    
    if (trimmed.rfind("key:", 0) == 0) {
        // 按 key 搜索
        std::string key = trim_copy(trimmed.substr(4));
        std::vector<SearchResult> results;
        const XmlValue* article = db_->find_by_key(key);
        if (article != nullptr) {
            results.emplace_back(article, "key", key);
        }
        return results;
    }
    
    // 默认按关键词模糊搜索
    return search_by_keyword(trimmed, SearchMode::FUZZY);
}

// ========== 辅助方法 ==========

std::string SearchEngine::to_lowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

// ========== 结果格式化 ==========

std::string SearchEngine::format_results(const std::vector<SearchResult>& results, 
                                          size_t max_results) {
    if (results.empty()) {
        return "未找到匹配的论文。\n";
    }
    
    std::ostringstream oss;
    size_t count = (max_results > 0 && max_results < results.size()) 
                   ? max_results : results.size();
    
    oss << "\n" << "=" << std::string(70, '=') << "\n";
    oss << "找到 " << results.size() << " 篇匹配的论文";
    if (max_results > 0 && results.size() > max_results) {
        oss << "（显示前 " << count << " 篇）";
    }
    oss << "\n" << "=" << std::string(70, '=') << "\n\n";
    
    for (size_t i = 0; i < count; ++i) {
        const SearchResult& r = results[i];
        const XmlValue* art = r.article;
        
        oss << "[" << (i + 1) << "] ";
        
        // 论文标题
        oss << "标题: " << art->title() << "\n";
        
        // 作者列表
        oss << "    作者: ";
        for (size_t j = 0; j < art->author_count(); ++j) {
            if (j > 0) oss << ", ";
            oss << art->author_at(j);
        }
        oss << "\n";
        
        // 年份
        if (!art->year().empty() && art->year() != MISSING_STRING) {
            oss << "    年份: " << art->year() << "\n";
        }
        
        // 期刊
        if (!art->journal().empty() && art->journal() != MISSING_STRING) {
            oss << "    期刊: " << art->journal() << "\n";
        }
        
        // DBLP Key
        oss << "    Key: " << art->key() << "\n";
        
        // 电子版链接
        auto ees = art->ees();
        if (!ees.empty()) {
            oss << "    链接: " << ees[0];
            if (ees.size() > 1) {
                oss << " (等 " << ees.size() << " 个链接)";
            }
            oss << "\n";
        }
        
        // 匹配信息
        oss << "    [匹配类型: " << r.match_type;
        if (!r.matched_value.empty() && r.matched_value != art->title()) {
            oss << ", 关键词: " << r.matched_value;
        }
        oss << "]\n";
        
        oss << "\n";
    }
    
    return oss.str();
}
