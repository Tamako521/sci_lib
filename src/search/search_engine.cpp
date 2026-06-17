#include "search/search_engine.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace indexed {

namespace {
std::string trim_copy(const std::string& value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}
}

SearchEngine::SearchEngine(const Database* db) : db_(db)
{
    if (db_ == nullptr) {
        throw std::invalid_argument("SearchEngine requires Database");
    }
}

std::vector<SearchResult> SearchEngine::make_results(const std::vector<std::uint32_t>& ids,
                                                     const std::string& type,
                                                     const std::string& matched,
                                                     std::size_t limit) const
{
    std::vector<SearchResult> results;
    auto articles = db_->read_articles(ids, 0, limit);
    results.reserve(articles.size());
    for (auto& article : articles) {
        results.push_back({ std::move(article), type, matched, 1 });
    }
    return results;
}

std::vector<SearchResult> SearchEngine::search_by_author(const std::string& author,
                                                         std::size_t limit) const
{
    auto ids = db_->author_records_exact(trim_copy(author));
    return make_results(ids, "author", author, limit);
}

std::vector<SearchResult> SearchEngine::search_by_title(const std::string& title,
                                                        std::size_t limit) const
{
    auto ids = db_->title_records_exact(trim_copy(title));
    return make_results(ids, "title", title, limit);
}

std::vector<SearchResult> SearchEngine::search_by_keyword(const std::string& keyword,
                                                          SearchMode mode,
                                                          std::size_t limit) const
{
    const std::string query = trim_copy(keyword);
    if (query.empty()) {
        return {};
    }
    if (mode == SearchMode::EXACT) {
        return search_by_title(query, limit);
    }
    auto ids = db_->title_word_records(query, true);
    return make_results(ids, "keyword_fuzzy", query, limit);
}

std::vector<SearchResult> SearchEngine::search_by_keywords(const std::vector<std::string>& keywords,
                                                           bool match_all,
                                                           std::size_t limit) const
{
    std::string joined;
    for (const auto& keyword : keywords) {
        const std::string value = trim_copy(keyword);
        if (value.empty()) {
            continue;
        }
        if (!joined.empty()) {
            joined.push_back(' ');
        }
        joined += value;
    }
    return make_results(db_->title_word_records(joined, match_all),
                        match_all ? "keywords_and" : "keywords_or", joined, limit);
}

std::vector<SearchResult> SearchEngine::smart_search(const std::string& query,
                                                     std::size_t limit) const
{
    const std::string value = trim_copy(query);
    if (value.rfind("author:", 0) == 0) {
        return search_by_author(value.substr(7), limit);
    }
    if (value.rfind("title:", 0) == 0) {
        return search_by_title(value.substr(6), limit);
    }
    if (value.rfind("key:", 0) == 0) {
        std::vector<SearchResult> results;
        if (const XmlValue* article = db_->find_by_key(trim_copy(value.substr(4)))) {
            results.push_back({ *article, "key", value.substr(4), 1 });
        }
        return results;
    }
    return search_by_keyword(value, SearchMode::FUZZY, limit);
}

} // namespace indexed
