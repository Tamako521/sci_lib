#include "index/index_builder.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <unordered_set>
#include <functional>

namespace indexed {

namespace {
constexpr std::size_t chunk_size = 8 * 1024 * 1024;
constexpr std::size_t max_clique_order = 7;

std::uint64_t edge_key(std::uint32_t a, std::uint32_t b)
{
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<std::uint64_t>(a) << 32) | b;
}

void saturating_add(std::uint64_t& target, std::uint64_t value)
{
    if (UINT64_MAX - target < value) {
        target = UINT64_MAX;
        return;
    }
    target += value;
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
    if (!writer_.open(index_dir_)) {
        std::cerr << "无法写入 articles.dat\n";
        return false;
    }
    if (!parse_xml(xml_path)) {
        return false;
    }
    writer_.close_articles();
    std::cout << "开始统计聚团...\n";
    clique_counts_ = count_cliques_by_order();
    std::uint64_t total_cliques = 0;
    std::size_t max_order = 0;
    for (std::size_t order = 1; order < clique_counts_.size(); ++order) {
        saturating_add(total_cliques, clique_counts_[order]);
        if (clique_counts_[order] > 0) {
            max_order = order;
        }
    }
    std::cout << "聚团统计完成: max_order=" << max_order
              << ", max_order_limit=" << max_clique_order
              << ", total=" << total_cliques << "\n";
    if (!writer_.write_indexes(pool_,
                               key_entries_,
                               author_string_ids_,
                               author_postings_,
                               title_exact_,
                               word_string_ids_,
                               word_postings_,
                               year_postings_,
                               journal_postings_,
                               volume_postings_,
                               author_counts_,
                               yearly_word_counts_,
                               edge_weights_,
                               clique_counts_)) {
        return false;
    }
    std::ofstream manifest(index_dir_ / "manifest.bin", std::ios::binary);
    const std::uint64_t record_count = writer_.offsets().size();
    format::write_pod(manifest, record_count);
    std::cout << "索引构建完成: records=" << writer_.offsets().size()
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
    writer_.write_article(article);
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

std::vector<std::uint64_t> IndexBuilder::count_cliques_by_order() const
{
    const int node_count = static_cast<int>(author_string_ids_.size());
    std::vector<std::uint64_t> counts(max_clique_order + 1, 0);
    counts[1] = static_cast<std::uint64_t>(node_count);
    if (node_count == 0) {
        return counts;
    }

    std::vector<std::vector<int>> neighbors(static_cast<std::size_t>(node_count));
    for (const auto& [key, weight] : edge_weights_) {
        (void)weight;
        const auto a = static_cast<int>(key >> 32);
        const auto b = static_cast<int>(key & 0xffffffffu);
        if (a >= 0 && b >= 0 && a < node_count && b < node_count) {
            neighbors[static_cast<std::size_t>(a)].push_back(b);
            neighbors[static_cast<std::size_t>(b)].push_back(a);
        }
    }
    for (auto& list : neighbors) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }

    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(node_count));
    std::vector<int> degree(static_cast<std::size_t>(node_count), 0);
    std::vector<bool> removed(static_cast<std::size_t>(node_count), false);
    std::priority_queue<
        std::pair<int, int>,
        std::vector<std::pair<int, int>>,
        std::greater<std::pair<int, int>>> queue;
    for (int index = 0; index < node_count; ++index) {
        degree[static_cast<std::size_t>(index)] = static_cast<int>(neighbors[static_cast<std::size_t>(index)].size());
        queue.push({ degree[static_cast<std::size_t>(index)], index });
    }

    while (!queue.empty()) {
        const auto [current_degree, node] = queue.top();
        queue.pop();
        if (removed[static_cast<std::size_t>(node)] || current_degree != degree[static_cast<std::size_t>(node)]) {
            continue;
        }
        removed[static_cast<std::size_t>(node)] = true;
        order.push_back(node);
        for (int neighbor : neighbors[static_cast<std::size_t>(node)]) {
            if (!removed[static_cast<std::size_t>(neighbor)]) {
                --degree[static_cast<std::size_t>(neighbor)];
                queue.push({ degree[static_cast<std::size_t>(neighbor)], neighbor });
            }
        }
    }

    std::vector<int> rank_of_node(static_cast<std::size_t>(node_count), 0);
    for (int rank = 0; rank < node_count; ++rank) {
        rank_of_node[static_cast<std::size_t>(order[static_cast<std::size_t>(rank)])] = rank;
    }

    std::vector<std::vector<int>> forward_neighbors(static_cast<std::size_t>(node_count));
    for (int rank = 0; rank < node_count; ++rank) {
        const int original_index = order[static_cast<std::size_t>(rank)];
        auto& forward = forward_neighbors[static_cast<std::size_t>(rank)];
        for (int neighbor : neighbors[static_cast<std::size_t>(original_index)]) {
            const int neighbor_rank = rank_of_node[static_cast<std::size_t>(neighbor)];
            if (rank < neighbor_rank) {
                forward.push_back(neighbor_rank);
            }
        }
        std::sort(forward.begin(), forward.end());
    }

    std::vector<std::unordered_set<int>> forward_sets(static_cast<std::size_t>(node_count));
    for (int rank = 0; rank < node_count; ++rank) {
        const auto& forward = forward_neighbors[static_cast<std::size_t>(rank)];
        forward_sets[static_cast<std::size_t>(rank)].reserve(forward.size());
        for (int neighbor : forward) {
            forward_sets[static_cast<std::size_t>(rank)].insert(neighbor);
        }
    }

    auto add_count = [&](std::size_t order_size) {
        if (order_size > max_clique_order) {
            return;
        }
        saturating_add(counts[order_size], 1);
    };

    auto add_complete_suffix_counts = [&](std::size_t current_size, std::size_t candidate_count) {
        const std::size_t max_add = std::min(candidate_count, max_clique_order - current_size);
        for (std::size_t add = 1; add <= max_add; ++add) {
            const std::size_t order_size = current_size + add;
            saturating_add(counts[order_size], combination(candidate_count, add));
        }
    };

    auto is_complete_candidates = [&](const std::vector<int>& candidates) {
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto& neighbor_set = forward_sets[static_cast<std::size_t>(candidates[i])];
            for (std::size_t j = i + 1; j < candidates.size(); ++j) {
                if (neighbor_set.find(candidates[j]) == neighbor_set.end()) {
                    return false;
                }
            }
        }
        return true;
    };

    auto intersect_forward = [](const std::vector<int>& candidates,
                                std::size_t start,
                                const std::vector<int>& neighbor_list) {
        std::vector<int> result;
        std::size_t i = start;
        std::size_t j = 0;
        while (i < candidates.size() && j < neighbor_list.size()) {
            const int candidate = candidates[i];
            const int neighbor = neighbor_list[j];
            if (candidate == neighbor) {
                result.push_back(candidate);
                ++i;
                ++j;
            } else if (candidate < neighbor) {
                ++i;
            } else {
                ++j;
            }
        }
        return result;
    };

    auto dfs = [&](auto&& self, const std::vector<int>& candidates, std::size_t current_size) -> void {
        if (candidates.empty() || current_size >= max_clique_order) {
            return;
        }
        if (is_complete_candidates(candidates)) {
            add_complete_suffix_counts(current_size, candidates.size());
            return;
        }
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const int next = candidates[i];
            add_count(current_size + 1);
            if (current_size + 1 >= max_clique_order) {
                continue;
            }

            const std::vector<int> next_candidates =
                intersect_forward(candidates, i + 1, forward_neighbors[static_cast<std::size_t>(next)]);
            if (!next_candidates.empty()) {
                self(self, next_candidates, current_size + 1);
            }
        }
    };

    for (int rank = 0; rank < node_count; ++rank) {
        dfs(dfs, forward_neighbors[static_cast<std::size_t>(rank)], 1);
    }

    return counts;
}

std::uint64_t IndexBuilder::combination(std::uint64_t n, std::uint64_t k)
{
    if (k > n) {
        return 0;
    }
    if (k > n - k) {
        k = n - k;
    }

    unsigned __int128 result = 1;
    for (std::uint64_t i = 1; i <= k; ++i) {
        result = (result * (n - k + i)) / i;
        if (result > UINT64_MAX) {
            return UINT64_MAX;
        }
    }
    return static_cast<std::uint64_t>(result);
}

} // namespace indexed
