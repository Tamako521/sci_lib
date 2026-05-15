#pragma once

#include "index/index_format.hpp"
#include "common/string_pool.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace indexed {

struct IndexArticle {
    std::uint32_t mdate_id = format::invalid_id;
    std::uint32_t key_id = format::invalid_id;
    std::vector<std::uint32_t> author_ids;
    std::uint32_t title_id = format::invalid_id;
    std::uint32_t journal_id = format::invalid_id;
    std::uint32_t volume_id = format::invalid_id;
    std::uint32_t month_id = format::invalid_id;
    std::uint32_t year_id = format::invalid_id;
    std::vector<std::uint32_t> cdrom_ids;
    std::vector<std::uint32_t> ee_ids;
};

class IndexWriter {
public:
    using PostingMap = std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>;

    bool open(const std::filesystem::path& index_dir);
    void close_articles();
    void write_article(const IndexArticle& article);

    bool write_indexes(
        const StringPool& pool,
        const std::vector<format::PostingDirEntry>& key_entries,
        const std::vector<std::uint32_t>& author_string_ids,
        const PostingMap& author_postings,
        const std::unordered_map<std::uint64_t, std::vector<format::TitleExactEntry>>& title_exact,
        const std::vector<std::uint32_t>& word_string_ids,
        const PostingMap& word_postings,
        const PostingMap& year_postings,
        const PostingMap& journal_postings,
        const PostingMap& volume_postings,
        const std::unordered_map<std::uint32_t, std::uint32_t>& author_counts,
        const std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, std::uint32_t>>& yearly_word_counts,
        const std::unordered_map<std::uint64_t, std::uint32_t>& edge_weights,
        const std::vector<std::uint64_t>& clique_counts);

    const std::vector<format::ArticleOffset>& offsets() const;

private:
    void write_lookup(const std::string& file_name,
                      const std::vector<format::PostingDirEntry>& entries);
    void write_posting_pair(const std::string& dir_name,
                            const std::string& index_name,
                            const PostingMap& postings);

    std::filesystem::path index_dir_;
    std::ofstream articles_;
    std::vector<format::ArticleOffset> offsets_;
};

}
