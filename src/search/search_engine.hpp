#pragma once

#include "common/database.hpp"
#include "common/xml_value.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace indexed {

struct SearchResult {
    XmlValue article;
    std::string match_type;
    std::string matched_value;
    std::size_t relevance_score = 1;
};

enum class SearchMode {
    EXACT,
    FUZZY
};

class SearchEngine {
public:
    explicit SearchEngine(const Database* db);

    std::vector<SearchResult> search_by_author(const std::string& author) const;
    std::vector<SearchResult> search_by_title(const std::string& title) const;
    std::vector<SearchResult> search_by_keyword(const std::string& keyword,
                                                SearchMode mode = SearchMode::FUZZY) const;
    std::vector<SearchResult> search_by_keywords(const std::vector<std::string>& keywords,
                                                 bool match_all = false) const;
    std::vector<SearchResult> smart_search(const std::string& query) const;

private:
    std::vector<SearchResult> make_results(const std::vector<std::uint32_t>& ids,
                                           const std::string& type,
                                           const std::string& matched) const;
    const Database* db_ = nullptr;
};

} // namespace indexed
