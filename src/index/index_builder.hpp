#pragma once

#include "index/index_format.hpp"
#include "index/index_writer.hpp"
#include "common/string_pool.hpp"
#include "common/xml_value.hpp"

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
    using PostingMap = IndexWriter::PostingMap;
    using Article = IndexArticle;

    bool parse_xml(const std::filesystem::path& xml_path);
    void process_article(const Article& article);
    static std::string strip_inline_tags(const std::string& text);
    static std::string decode_entities(std::string text);
    static std::string normalize(const std::string& value);
    static std::vector<std::string> tokenize(const std::string& text);
    static bool is_stop_word(const std::string& word);
    static std::uint64_t stable_hash(const std::string& value);
    std::vector<std::uint64_t> count_cliques_by_order() const;
    static std::uint64_t combination(std::uint64_t n, std::uint64_t k);

    std::filesystem::path index_dir_;
    StringPool pool_;
    IndexWriter writer_;
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
    std::vector<std::uint64_t> clique_counts_;
};

} // namespace indexed
