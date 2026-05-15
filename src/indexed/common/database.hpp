#pragma once

#include "index/index_format.hpp"
#include "indexed/common/string_pool.hpp"
#include "indexed/common/xml_value.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace indexed {

struct AuthorStat {
    std::string author;
    std::size_t paper_count = 0;
};

struct KeywordStat {
    std::string keyword;
    std::size_t count = 0;
};

using YearKeywordTop = std::unordered_map<std::string, std::vector<KeywordStat>>;

class Database {
public:
    struct Dir {
        std::uint64_t offset = 0;
        std::uint32_t count = 0;
    };

    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::filesystem::path& index_dir = format::index_root());
    static bool has_index(const std::filesystem::path& index_dir = format::index_root());

    std::size_t size() const;
    const std::string& get_string(std::uint32_t id) const;

    std::optional<XmlValue> read_article(std::uint32_t record_id) const;
    std::vector<XmlValue> read_articles(const std::vector<std::uint32_t>& record_ids,
                                        std::size_t offset = 0,
                                        std::size_t limit = 100) const;

    std::optional<std::uint32_t> find_key_id(const std::string& key) const;
    const XmlValue* find_by_key(const std::string& key) const;

    std::vector<std::uint32_t> author_records_exact(const std::string& author) const;
    std::vector<std::uint32_t> author_records_contains(const std::string& author) const;
    std::vector<std::uint32_t> title_records_exact(const std::string& title) const;
    std::vector<std::uint32_t> title_word_records(const std::string& query, bool match_all) const;
    std::vector<std::uint32_t> field_records(const std::string& field, const std::string& value) const;

    std::vector<AuthorStat> top_authors(std::size_t limit = 100) const;
    YearKeywordTop yearly_hot_keywords(std::size_t limit = 10) const;
    std::vector<std::pair<std::string, int>> coauthors(const std::string& author) const;
    std::vector<std::uint64_t> count_cliques_by_order() const;
    std::vector<std::pair<std::string, std::size_t>> author_paper_counts() const;

private:
    bool load_string_pool();
    bool load_offsets();
    bool load_lookup_files();
    bool load_stats();
    bool load_graph();
    bool load_clique_stats();
    std::vector<std::uint32_t> read_posting(const std::string& file, Dir dir) const;
    std::optional<XmlValue> read_article_at(format::ArticleOffset offset) const;
    static std::string normalize(const std::string& value);
    static std::vector<std::string> tokenize(const std::string& text);
    static std::vector<std::uint32_t> intersect_sorted(std::vector<std::uint32_t> left,
                                                       const std::vector<std::uint32_t>& right);
    static std::vector<std::uint32_t> union_sorted(std::vector<std::uint32_t> left,
                                                   const std::vector<std::uint32_t>& right);

    std::filesystem::path index_dir_;
    StringPool string_pool_;
    std::vector<format::ArticleOffset> offsets_;

    std::unordered_map<std::string, std::uint32_t> key_lookup_;
    std::unordered_map<std::string, std::uint32_t> author_lookup_;
    std::vector<std::uint32_t> author_string_ids_;
    std::unordered_map<std::uint32_t, Dir> author_dir_;
    std::unordered_map<std::uint64_t, Dir> title_exact_dir_;
    std::unordered_map<std::string, std::uint32_t> title_word_lookup_;
    std::unordered_map<std::uint32_t, Dir> title_word_dir_;
    std::unordered_map<std::uint32_t, Dir> year_dir_;
    std::unordered_map<std::uint32_t, Dir> journal_dir_;
    std::unordered_map<std::uint32_t, Dir> volume_dir_;

    std::vector<AuthorStat> top_author_stats_;
    YearKeywordTop yearly_keywords_;
    std::unordered_map<std::uint32_t, std::vector<format::WeightedNeighbor>> graph_;
    std::vector<std::uint64_t> clique_counts_;

    mutable std::ifstream articles_;
    mutable std::optional<XmlValue> last_found_;
};

} // namespace indexed
