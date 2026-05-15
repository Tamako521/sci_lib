#include "index/index_builder.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace indexed {

namespace {
constexpr std::size_t chunk_size = 8 * 1024 * 1024;

std::uint64_t edge_key(std::uint32_t a, std::uint32_t b)
{
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<std::uint64_t>(a) << 32) | b;
}
}

bool IndexBuilder::build(const std::filesystem::path& xml_path, const std::filesystem::path& index_dir)
{
    if (!std::filesystem::exists(xml_path)) {
        std::cerr << "未找到 " << xml_path.string() << "\n";
        return false;
    }
    index_dir_ = index_dir;
    if (std::filesystem::exists(index_dir_)) {
        std::cout << "检测到旧索引 " << index_dir_.string() << "，是否删除并重建？(y/n): ";
        char answer = 'n';
        std::cin >> answer;
        if (answer != 'y' && answer != 'Y') {
            std::cout << "已取消重建。\n";
            return true;
        }
        std::filesystem::remove_all(index_dir_);
    }
    std::filesystem::create_directories(index_dir_);
    articles_.open(index_dir_ / "articles.dat", std::ios::binary);
    if (!articles_.is_open()) {
        std::cerr << "无法写入 articles.dat\n";
        return false;
    }
    if (!parse_xml(xml_path)) {
        return false;
    }
    articles_.close();
    if (!write_indexes()) {
        return false;
    }
    std::ofstream manifest(index_dir_ / "manifest.bin", std::ios::binary);
    const std::uint64_t record_count = offsets_.size();
    format::write_pod(manifest, record_count);
    std::cout << "索引构建完成: records=" << offsets_.size()
              << ", authors=" << author_string_ids_.size()
              << ", title_words=" << word_string_ids_.size()
              << ", edges=" << edge_weights_.size() << "\n";
    return true;
}

bool IndexBuilder::parse_xml(const std::filesystem::path& xml_path)
{
    std::ifstream in(xml_path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    const auto file_size = std::filesystem::file_size(xml_path);
    std::string buffer;
    buffer.reserve(chunk_size * 2);
    std::vector<char> chunk(chunk_size);
    std::uint64_t consumed = 0;

    while (in || !buffer.empty()) {
        if (in) {
            in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            buffer.append(chunk.data(), static_cast<std::size_t>(in.gcount()));
        }
        std::size_t search_pos = 0;
        while (true) {
            const std::size_t begin = buffer.find("<article", search_pos);
            if (begin == std::string::npos) {
                if (buffer.size() > chunk_size) {
                    consumed += buffer.size() - 1024;
                    buffer.erase(0, buffer.size() - 1024);
                }
                break;
            }
            const std::size_t end = buffer.find("</article>", begin);
            if (end == std::string::npos) {
                if (begin > 0) {
                    consumed += begin;
                    buffer.erase(0, begin);
                }
                break;
            }
            const std::size_t close = end + 10;
            const std::string item = buffer.substr(begin, close - begin);
            Article article;

            const std::size_t start_tag_end = item.find('>');
            if (start_tag_end != std::string::npos) {
                const std::string start_tag = item.substr(0, start_tag_end);
                auto attr = [&](const std::string& name) -> std::string {
                    const std::string needle = name + "=\"";
                    const std::size_t p = start_tag.find(needle);
                    if (p == std::string::npos) {
                        return {};
                    }
                    const std::size_t value_begin = p + needle.size();
                    const std::size_t value_end = start_tag.find('"', value_begin);
                    return value_end == std::string::npos ? std::string{} : start_tag.substr(value_begin, value_end - value_begin);
                };
                const std::string mdate = attr("mdate");
                const std::string key = attr("key");
                if (!mdate.empty()) article.mdate_id = pool_.intern(decode_entities(mdate));
                if (!key.empty()) article.key_id = pool_.intern(decode_entities(key));
            }

            auto collect = [&](const std::string& tag, auto add) {
                const std::string open = "<" + tag;
                const std::string close_tag = "</" + tag + ">";
                std::size_t pos = 0;
                while ((pos = item.find(open, pos)) != std::string::npos) {
                    const std::size_t gt = item.find('>', pos);
                    const std::size_t stop = item.find(close_tag, gt == std::string::npos ? pos : gt);
                    if (gt == std::string::npos || stop == std::string::npos) {
                        break;
                    }
                    add(decode_entities(strip_inline_tags(item.substr(gt + 1, stop - gt - 1))));
                    pos = stop + close_tag.size();
                }
            };
            collect("author", [&](const std::string& v) { article.author_ids.push_back(pool_.intern(v)); });
            collect("title", [&](const std::string& v) { article.title_id = pool_.intern(v); });
            collect("journal", [&](const std::string& v) { article.journal_id = pool_.intern(v); });
            collect("volume", [&](const std::string& v) { article.volume_id = pool_.intern(v); });
            collect("month", [&](const std::string& v) { article.month_id = pool_.intern(v); });
            collect("year", [&](const std::string& v) { article.year_id = pool_.intern(v); });
            collect("cdrom", [&](const std::string& v) { article.cdrom_ids.push_back(pool_.intern(v)); });
            collect("ee", [&](const std::string& v) { article.ee_ids.push_back(pool_.intern(v)); });

            process_article(article);
            if (next_record_id_ % 100000 == 0) {
                std::cout << "records=" << next_record_id_ << ", approx "
                          << ((consumed + close) * 100 / std::max<std::uint64_t>(1, file_size)) << "%\n";
            }
            search_pos = close;
        }
        if (search_pos > 0) {
            consumed += search_pos;
            buffer.erase(0, search_pos);
        }
        if (!in && buffer.find("<article") == std::string::npos) {
            break;
        }
    }
    return true;
}

void IndexBuilder::process_article(const Article& article)
{
    const std::uint32_t record_id = next_record_id_++;
    write_article(article);
    if (article.key_id != format::invalid_id) {
        key_entries_.push_back({ article.key_id, record_id, 0 });
    }
    std::vector<std::uint32_t> authors;
    authors.reserve(article.author_ids.size());
    for (std::uint32_t string_id : article.author_ids) {
        const std::string name = normalize(pool_.get(string_id));
        if (name.empty()) {
            continue;
        }
        auto [it, inserted] = author_ids_.emplace(name, static_cast<std::uint32_t>(author_ids_.size()));
        if (inserted) {
            author_string_ids_.push_back(string_id);
        }
        authors.push_back(it->second);
        author_postings_[it->second].push_back(record_id);
        ++author_counts_[it->second];
    }
    std::sort(authors.begin(), authors.end());
    authors.erase(std::unique(authors.begin(), authors.end()), authors.end());
    for (std::size_t i = 0; i < authors.size(); ++i) {
        for (std::size_t j = i + 1; j < authors.size(); ++j) {
            ++edge_weights_[edge_key(authors[i], authors[j])];
        }
    }

    if (article.title_id != format::invalid_id) {
        const std::string title = pool_.get(article.title_id);
        title_exact_[stable_hash(normalize(title))].push_back({ article.title_id, record_id });
        std::unordered_set<std::uint32_t> seen_words;
        for (const std::string& word : tokenize(title)) {
            if (is_stop_word(word)) {
                continue;
            }
            auto [it, inserted] = word_ids_.emplace(word, static_cast<std::uint32_t>(word_ids_.size()));
            if (inserted) {
                word_string_ids_.push_back(pool_.intern(word));
            }
            if (seen_words.insert(it->second).second) {
                word_postings_[it->second].push_back(record_id);
                if (article.year_id != format::invalid_id) {
                    ++yearly_word_counts_[article.year_id][word_string_ids_[it->second]];
                }
            }
        }
    }
    if (article.year_id != format::invalid_id) year_postings_[article.year_id].push_back(record_id);
    if (article.journal_id != format::invalid_id) journal_postings_[article.journal_id].push_back(record_id);
    if (article.volume_id != format::invalid_id) volume_postings_[article.volume_id].push_back(record_id);
}

void IndexBuilder::write_article(const Article& article)
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

bool IndexBuilder::write_indexes()
{
    {
        std::ofstream out(index_dir_ / "string_pool.dat", std::ios::binary);
        const auto count = static_cast<std::uint64_t>(pool_.size());
        format::write_pod(out, count);
        for (const auto& value : pool_.all_strings()) {
            format::write_string(out, value);
        }
    }
    format::write_vector_file(index_dir_ / "article_offsets.dat", offsets_);
    format::write_vector_file(index_dir_ / "key_index.dat", key_entries_);

    std::vector<format::PostingDirEntry> author_lookup;
    for (std::uint32_t author_id = 0; author_id < author_string_ids_.size(); ++author_id) {
        author_lookup.push_back({ author_string_ids_[author_id], author_id, 0 });
    }
    write_lookup("author_lookup.dat", author_lookup);
    write_posting_pair("author_index_dir.dat", "author_index.dat", author_postings_);

    {
        std::vector<format::TitleHashDirEntry> dirs;
        std::vector<format::TitleExactEntry> values;
        for (auto& [hash, entries] : title_exact_) {
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                return a.record_id < b.record_id;
            });
            dirs.push_back({ hash, static_cast<std::uint64_t>(values.size()), static_cast<std::uint32_t>(entries.size()) });
            values.insert(values.end(), entries.begin(), entries.end());
        }
        format::write_vector_file(index_dir_ / "title_exact_dir.dat", dirs);
        std::ofstream out(index_dir_ / "title_exact_index.dat", std::ios::binary);
        if (!values.empty()) {
            out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(format::TitleExactEntry)));
        }
    }

    std::vector<format::PostingDirEntry> word_lookup;
    for (std::uint32_t word_id = 0; word_id < word_string_ids_.size(); ++word_id) {
        word_lookup.push_back({ word_string_ids_[word_id], word_id, 0 });
    }
    write_lookup("title_word_lookup.dat", word_lookup);
    write_posting_pair("title_word_index_dir.dat", "title_word_index.dat", word_postings_);
    write_posting_pair("year_index_dir.dat", "year_index.dat", year_postings_);
    write_posting_pair("journal_index_dir.dat", "journal_index.dat", journal_postings_);
    write_posting_pair("volume_index_dir.dat", "volume_index.dat", volume_postings_);

    {
        std::ofstream out(index_dir_ / "stats_index.dat", std::ios::binary);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> authors(author_counts_.begin(), author_counts_.end());
        std::sort(authors.begin(), authors.end(), [&](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return pool_.get(author_string_ids_[a.first]) < pool_.get(author_string_ids_[b.first]);
        });
        if (authors.size() > 100) authors.resize(100);
        format::write_pod(out, static_cast<std::uint64_t>(authors.size()));
        for (const auto& [author_id, count] : authors) {
            format::write_pod(out, author_id);
            format::write_pod(out, count);
        }
        format::write_pod(out, static_cast<std::uint64_t>(yearly_word_counts_.size()));
        for (auto& [year_id, counts] : yearly_word_counts_) {
            std::vector<std::pair<std::uint32_t, std::uint32_t>> words(counts.begin(), counts.end());
            std::sort(words.begin(), words.end(), [&](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second > b.second;
                return pool_.get(a.first) < pool_.get(b.first);
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
        std::vector<std::vector<format::WeightedNeighbor>> graph(author_string_ids_.size());
        for (const auto& [key, weight] : edge_weights_) {
            const auto a = static_cast<std::uint32_t>(key >> 32);
            const auto b = static_cast<std::uint32_t>(key & 0xffffffffu);
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
    return true;
}

void IndexBuilder::write_lookup(const std::string& file_name,
                                const std::vector<format::PostingDirEntry>& entries)
{
    format::write_vector_file(index_dir_ / file_name, entries);
}

void IndexBuilder::write_posting_pair(const std::string& dir_name,
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

std::string IndexBuilder::strip_inline_tags(const std::string& text)
{
    std::string result;
    bool in_tag = false;
    for (char ch : text) {
        if (ch == '<') { in_tag = true; continue; }
        if (ch == '>') { in_tag = false; continue; }
        if (!in_tag) result.push_back(ch);
    }
    return result;
}

std::string IndexBuilder::decode_entities(std::string text)
{
    const std::pair<const char*, const char*> replacements[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}
    };
    for (const auto& [from, to] : replacements) {
        std::string::size_type pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, std::char_traits<char>::length(from), to);
            pos += std::char_traits<char>::length(to);
        }
    }
    return text;
}

std::string IndexBuilder::normalize(const std::string& value)
{
    std::string out;
    bool last_space = true;
    for (unsigned char ch : value) {
        if (std::isspace(ch)) {
            if (!last_space) out.push_back(' ');
            last_space = true;
        } else {
            out.push_back(static_cast<char>(std::tolower(ch)));
            last_space = false;
        }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::vector<std::string> IndexBuilder::tokenize(const std::string& text)
{
    std::vector<std::string> words;
    std::string word;
    for (unsigned char ch : text) {
        if (std::isalnum(ch)) {
            word.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) words.push_back(word);
    return words;
}

bool IndexBuilder::is_stop_word(const std::string& word)
{
    static const std::unordered_set<std::string> stop_words = {
        "an", "the", "of", "and", "or", "in", "on", "at", "to", "for", "from", "by",
        "with", "without", "as", "is", "are", "was", "were", "be", "been", "being",
        "this", "that", "these", "those", "it", "its", "into", "over", "under",
        "between", "among", "than", "then", "using", "use", "used", "based", "via"
    };
    const bool number = std::all_of(word.begin(), word.end(), [](unsigned char ch) { return std::isdigit(ch); });
    return word.size() < 2 || number || stop_words.count(word) > 0;
}

std::uint64_t IndexBuilder::stable_hash(const std::string& value)
{
    constexpr std::uint64_t offset = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= prime;
    }
    return hash;
}

} // namespace indexed
