#include "index/index_writer.hpp"

#include <algorithm>
#include <fstream>

namespace indexed {

namespace {
std::uint64_t edge_author_a(std::uint64_t key)
{
    return key >> 32;
}

std::uint64_t edge_author_b(std::uint64_t key)
{
    return key & 0xffffffffu;
}
}

bool IndexWriter::open(const std::filesystem::path& index_dir)
{
    index_dir_ = index_dir;
    offsets_.clear();
    articles_.open(index_dir_ / "articles.dat", std::ios::binary);
    return articles_.is_open();
}

void IndexWriter::close_articles()
{
    if (articles_.is_open()) {
        articles_.close();
    }
}

void IndexWriter::write_article(const IndexArticle& article)
{
    const auto offset = static_cast<std::uint64_t>(articles_.tellp());
    auto write_id = [&](std::uint32_t id) { format::write_pod(articles_, id); };
    write_id(article.mdate_id);
    write_id(article.key_id);
    write_id(article.title_id);
    write_id(article.journal_id);
    write_id(article.volume_id);
    write_id(article.month_id);
    write_id(article.year_id);
    auto write_ids = [&](const std::vector<std::uint32_t>& ids) {
        const auto count = static_cast<std::uint32_t>(ids.size());
        format::write_pod(articles_, count);
        if (!ids.empty()) {
            articles_.write(reinterpret_cast<const char*>(ids.data()),
                            static_cast<std::streamsize>(ids.size() * sizeof(std::uint32_t)));
        }
    };
    write_ids(article.author_ids);
    write_ids(article.cdrom_ids);
    write_ids(article.ee_ids);
    const auto end = static_cast<std::uint64_t>(articles_.tellp());
    offsets_.push_back({ offset, static_cast<std::uint32_t>(end - offset) });
}

bool IndexWriter::write_indexes(
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
    const std::vector<std::uint64_t>& clique_counts)
{
    {
        std::ofstream out(index_dir_ / "string_pool.dat", std::ios::binary);
        const auto count = static_cast<std::uint64_t>(pool.size());
        format::write_pod(out, count);
        for (const auto& value : pool.all_strings()) {
            format::write_string(out, value);
        }
    }
    format::write_vector_file(index_dir_ / "article_offsets.dat", offsets_);
    format::write_vector_file(index_dir_ / "key_index.dat", key_entries);

    std::vector<format::PostingDirEntry> author_lookup;
    for (std::uint32_t author_id = 0; author_id < author_string_ids.size(); ++author_id) {
        author_lookup.push_back({ author_string_ids[author_id], author_id, 0 });
    }
    write_lookup("author_lookup.dat", author_lookup);
    write_posting_pair("author_index_dir.dat", "author_index.dat", author_postings);

    {
        std::vector<format::TitleHashDirEntry> dirs;
        std::vector<format::TitleExactEntry> values;
        for (const auto& [hash, original_entries] : title_exact) {
            std::vector<format::TitleExactEntry> entries = original_entries;
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                return a.record_id < b.record_id;
            });
            dirs.push_back({ hash, static_cast<std::uint64_t>(values.size()), static_cast<std::uint32_t>(entries.size()) });
            values.insert(values.end(), entries.begin(), entries.end());
        }
        format::write_vector_file(index_dir_ / "title_exact_dir.dat", dirs);
        std::ofstream out(index_dir_ / "title_exact_index.dat", std::ios::binary);
        if (!values.empty()) {
            out.write(reinterpret_cast<const char*>(values.data()),
                      static_cast<std::streamsize>(values.size() * sizeof(format::TitleExactEntry)));
        }
    }

    std::vector<format::PostingDirEntry> word_lookup;
    for (std::uint32_t word_id = 0; word_id < word_string_ids.size(); ++word_id) {
        word_lookup.push_back({ word_string_ids[word_id], word_id, 0 });
    }
    write_lookup("title_word_lookup.dat", word_lookup);
    write_posting_pair("title_word_index_dir.dat", "title_word_index.dat", word_postings);
    write_posting_pair("year_index_dir.dat", "year_index.dat", year_postings);
    write_posting_pair("journal_index_dir.dat", "journal_index.dat", journal_postings);
    write_posting_pair("volume_index_dir.dat", "volume_index.dat", volume_postings);

    {
        std::ofstream out(index_dir_ / "stats_index.dat", std::ios::binary);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> authors(author_counts.begin(), author_counts.end());
        std::sort(authors.begin(), authors.end(), [&](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return pool.get(author_string_ids[a.first]) < pool.get(author_string_ids[b.first]);
        });
        if (authors.size() > 100) authors.resize(100);
        format::write_pod(out, static_cast<std::uint64_t>(authors.size()));
        for (const auto& [author_id, count] : authors) {
            format::write_pod(out, author_id);
            format::write_pod(out, count);
        }
        format::write_pod(out, static_cast<std::uint64_t>(yearly_word_counts.size()));
        for (const auto& [year_id, counts] : yearly_word_counts) {
            std::vector<std::pair<std::uint32_t, std::uint32_t>> words(counts.begin(), counts.end());
            std::sort(words.begin(), words.end(), [&](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second > b.second;
                return pool.get(a.first) < pool.get(b.first);
            });
            if (words.size() > 10) words.resize(10);
            format::write_pod(out, year_id);
            format::write_pod(out, static_cast<std::uint32_t>(words.size()));
            for (const auto& [word_id, count] : words) {
                format::write_pod(out, word_id);
                format::write_pod(out, count);
            }
        }
    }

    {
        std::vector<std::vector<format::WeightedNeighbor>> graph(author_string_ids.size());
        for (const auto& [key, weight] : edge_weights) {
            const auto a = static_cast<std::uint32_t>(edge_author_a(key));
            const auto b = static_cast<std::uint32_t>(edge_author_b(key));
            graph[a].push_back({ b, weight });
            graph[b].push_back({ a, weight });
        }
        std::ofstream out(index_dir_ / "coauthor_graph.dat", std::ios::binary);
        format::write_pod(out, static_cast<std::uint64_t>(graph.size()));
        for (std::uint32_t author_id = 0; author_id < graph.size(); ++author_id) {
            std::sort(graph[author_id].begin(), graph[author_id].end(), [](const auto& a, const auto& b) {
                return a.author_id < b.author_id;
            });
            format::write_pod(out, author_id);
            format::write_pod(out, static_cast<std::uint32_t>(graph[author_id].size()));
            if (!graph[author_id].empty()) {
                out.write(reinterpret_cast<const char*>(graph[author_id].data()),
                          static_cast<std::streamsize>(graph[author_id].size() * sizeof(format::WeightedNeighbor)));
            }
        }
    }
    format::write_vector_file(index_dir_ / "clique_stats.dat", clique_counts);
    return true;
}

const std::vector<format::ArticleOffset>& IndexWriter::offsets() const
{
    return offsets_;
}

void IndexWriter::write_lookup(const std::string& file_name,
                               const std::vector<format::PostingDirEntry>& entries)
{
    format::write_vector_file(index_dir_ / file_name, entries);
}

void IndexWriter::write_posting_pair(const std::string& dir_name,
                                     const std::string& index_name,
                                     const PostingMap& postings)
{
    std::vector<format::PostingDirEntry> dirs;
    std::vector<std::uint32_t> values;
    dirs.reserve(postings.size());
    for (auto [id, ids] : postings) {
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        dirs.push_back({ id, static_cast<std::uint64_t>(values.size()), static_cast<std::uint32_t>(ids.size()) });
        values.insert(values.end(), ids.begin(), ids.end());
    }
    format::write_vector_file(index_dir_ / dir_name, dirs);
    std::ofstream out(index_dir_ / index_name, std::ios::binary);
    if (!values.empty()) {
        out.write(reinterpret_cast<const char*>(values.data()),
                  static_cast<std::streamsize>(values.size() * sizeof(std::uint32_t)));
    }
}

} // namespace indexed
