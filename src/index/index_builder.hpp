#pragma once

#include "index/index_format.hpp"
#include "indexed/common/string_pool.hpp"
#include "indexed/common/xml_value.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace indexed {

class IndexBuilder {
public:
    bool build(const std::filesystem::path& xml_path = format::xml_path(),
               const std::filesystem::path& index_dir = format::index_root());

private:
    using PostingMap = std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>;

    struct Article {
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

    bool parse_xml(const std::filesystem::path& xml_path);
    void process_article(const Article& article);
    void write_article(const Article& article);
    bool write_indexes();
    void write_posting_pair(const std::string& dir_name,
                            const std::string& index_name,
                            const PostingMap& postings);
    void write_lookup(const std::string& file_name,
                      const std::vector<format::PostingDirEntry>& entries);
    static std::string strip_inline_tags(const std::string& text);
    static std::string decode_entities(std::string text);
    static std::string normalize(const std::string& value);
    static std::vector<std::string> tokenize(const std::string& text);
    static bool is_stop_word(const std::string& word);
    static std::uint64_t stable_hash(const std::string& value);

    std::filesystem::path index_dir_;
    StringPool pool_;
    std::ofstream articles_;
    std::vector<format::ArticleOffset> offsets_;
    std::uint32_t next_record_id_ = 0;

    std::vector<format::PostingDirEntry> key_entries_;
    std::unordered_map<std::string, std::uint32_t> author_ids_;
    std::vector<std::uint32_t> author_string_ids_;
    PostingMap author_postings_;
    std::unordered_map<std::uint64_t, std::vector<format::TitleExactEntry>> title_exact_;
    std::unordered_map<std::string, std::uint32_t> word_ids_;
    std::vector<std::uint32_t> word_string_ids_;
    PostingMap word_postings_;
    PostingMap year_postings_;
    PostingMap journal_postings_;
    PostingMap volume_postings_;
    std::unordered_map<std::uint32_t, std::uint32_t> author_counts_;
    std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, std::uint32_t>> yearly_word_counts_;
    std::unordered_map<std::uint64_t, std::uint32_t> edge_weights_;
};

} // namespace indexed
