#ifndef SEARCH_ENGINE_HPP
#define SEARCH_ENGINE_HPP

#include <string>
#include <vector>
#include <memory>

#include "common/database.hpp"
#include "common/xml_value.hpp"

/**
 * @brief 搜索结果结构体
 */
struct SearchResult {
    const XmlValue* article;      // 论文指针
    std::string match_type;        // 匹配类型: "author", "title", "keyword"
    std::string matched_value;     // 匹配的值
    size_t relevance_score;        // 相关性得分（用于模糊搜索排序）
    
    SearchResult(const XmlValue* art, const std::string& type, 
                 const std::string& value, size_t score = 1)
        : article(art)
        , match_type(type)
        , matched_value(value)
        , relevance_score(score) {}
};

/**
 * @brief 搜索模式枚举
 */
enum class SearchMode {
    EXACT,     // 精确匹配
    FUZZY       // 模糊匹配
};

/**
 * @brief SearchEngine - 统一搜索引擎
 * 
 * 提供以下搜索功能：
 * - F1: 基本搜索（作者名/论文标题精准匹配）
 * - F5: 部分匹配搜索（关键词模糊搜索）
 */
class SearchEngine {
public:
    explicit SearchEngine(const Database* db);
    ~SearchEngine() = default;
    
    // 禁用拷贝
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
    
    // ========== F1: 基本搜索 ==========
    
    /**
     * @brief 按作者名搜索（精确匹配）
     * @param author 作者名
     * @return 搜索结果列表
     */
    std::vector<SearchResult> search_by_author(const std::string& author) const;
    
    /**
     * @brief 按论文标题搜索（精确匹配）
     * @param title 完整论文标题
     * @return 搜索结果列表
     */
    std::vector<SearchResult> search_by_title(const std::string& title) const;
    
    // ========== F5: 部分匹配搜索 ==========
    
    /**
     * @brief 按关键词搜索（模糊匹配，大小写不敏感）
     * @param keyword 关键词
     * @param mode 搜索模式（精确/模糊）
     * @return 搜索结果列表，按相关性排序
     */
    std::vector<SearchResult> search_by_keyword(const std::string& keyword, 
                                                 SearchMode mode = SearchMode::FUZZY) const;
    
    /**
     * @brief 按多个关键词搜索（模糊匹配）
     * @param keywords 关键词列表
     * @param match_all 是否要求匹配所有关键词（AND），否则匹配任意（OR）
     * @return 搜索结果列表，按相关性排序
     */
    std::vector<SearchResult> search_by_keywords(const std::vector<std::string>& keywords,
                                                  bool match_all = false) const;
    
    // ========== 统一搜索接口 ==========
    
    /**
     * @brief 智能搜索接口
     * @param query 搜索查询
     * @return 搜索结果列表
     * 
     * 自动判断搜索类型：
     * - 如果包含 "author:" 前缀，按作者搜索
     * - 如果包含 "title:" 前缀，按标题搜索
     * - 否则按关键词模糊搜索
     */
    std::vector<SearchResult> smart_search(const std::string& query) const;
    
    /**
     * @brief 格式化搜索结果为字符串
     * @param results 搜索结果
     * @param max_results 最大显示数量（0表示全部）
     * @return 格式化后的字符串
     */
    static std::string format_results(const std::vector<SearchResult>& results, 
                                       size_t max_results = 0);
    
private:
    const Database* db_;
    
    // 内部辅助方法
    std::vector<const XmlValue*> find_exact_title(const std::string& title) const;
    std::vector<const XmlValue*> find_fuzzy_keyword(const std::string& keyword) const;
    static std::string to_lowercase(const std::string& str);
};

#endif // SEARCH_ENGINE_HPP
