#include "index/index_builder.hpp"
#include "common/text_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <unordered_set>
#include <functional>

namespace indexed {

namespace {
constexpr std::size_t chunk_size = 8 * 1024 * 1024;
// 需要统计的完全子图最大阶数
constexpr int max_clique_order = 7;

std::uint64_t edge_key(std::uint32_t a, std::uint32_t b)
{
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<std::uint64_t>(a) << 32) | b;
}


} // anonymous namespace

// ---- BigCount 128 位精确整数实现 ----

// 128 位除以 10，返回余数（修改自身为商）
static std::uint64_t div10_128(std::uint64_t& hi, std::uint64_t& lo)
{
    std::uint64_t rem = 0;
    // 从最高位逐位处理 hi
    std::uint64_t q_hi = 0;
    for (int i = 63; i >= 0; --i) {
        rem = (rem << 1) | ((hi >> i) & 1);
        if (rem >= 10) {
            rem -= 10;
            q_hi |= (1ULL << i);
        }
    }
    // 处理 lo
    std::uint64_t q_lo = 0;
    for (int i = 63; i >= 0; --i) {
        rem = (rem << 1) | ((lo >> i) & 1);
        if (rem >= 10) {
            rem -= 10;
            q_lo |= (1ULL << i);
        }
    }
    hi = q_hi;
    lo = q_lo;
    return rem;
}

// 128 位 × 10 + digit（digit < 10），32位分片防止中间溢出
static void mul10_add_128(std::uint64_t& hi, std::uint64_t& lo, std::uint64_t digit)
{
    const std::uint64_t lo_low  = lo & 0xFFFFFFFFULL;
    const std::uint64_t lo_high = lo >> 32;
    const std::uint64_t hi_low  = hi & 0xFFFFFFFFULL;
    const std::uint64_t hi_high = hi >> 32;

    const std::uint64_t p0 = lo_low  * 10 + digit;       // < 2^36
    const std::uint64_t p1 = lo_high * 10 + (p0 >> 32);  // < 2^36
    const std::uint64_t p2 = hi_low  * 10 + (p1 >> 32);  // < 2^36
    const std::uint64_t p3 = hi_high * 10 + (p2 >> 32);  // < 2^36

    lo = (p0 & 0xFFFFFFFFULL) | ((p1 & 0xFFFFFFFFULL) << 32);
    hi = (p2 & 0xFFFFFFFFULL) | ((p3 & 0xFFFFFFFFULL) << 32);
}

std::string BigCount::to_string() const
{
    if (is_zero()) return "0";

    // 128 位转十进制，最多 39 位（2^128 ≈ 3.4e38）
    char buf[40];
    int pos = 39;
    buf[pos] = '\0';

    std::uint64_t h = hi, l = lo;
    while (h != 0 || l != 0) {
        std::uint64_t rem = div10_128(h, l);
        buf[--pos] = static_cast<char>('0' + rem);
    }

    return std::string(buf + pos);
}

BigCount BigCount::from_string(const std::string& s)
{
    if (s.empty() || s == "0") return BigCount{};

    // 科学计数法格式（兼容旧数据）：解析为 double，再转换为 128 位近似值
    auto e_pos = s.find('e');
    {
        const auto E_pos = s.find('E');
        if (e_pos == std::string::npos) e_pos = E_pos;
        else if (E_pos != std::string::npos && E_pos < e_pos) e_pos = E_pos;
    }
    if (e_pos == std::string::npos) {
        // 纯十进制格式
        std::uint64_t h = 0, l = 0;
        for (char c : s) {
            if (c < '0' || c > '9') continue;
            mul10_add_128(h, l, static_cast<std::uint64_t>(c - '0'));
        }
        return BigCount{h, l};
    }

    // 科学计数法：mantissa e exponent → double → 128 位近似
    try {
        const std::string mant_str = s.substr(0, e_pos);
        const std::string exp_str = s.substr(e_pos + 1);
        double val = std::stod(mant_str) * std::pow(10.0, std::stoi(exp_str));
        if (val < 1.0) return BigCount{};
        if (val >= std::pow(2.0, 128.0)) return BigCount{UINT64_MAX, UINT64_MAX};

        std::uint64_t h = static_cast<std::uint64_t>(val / std::pow(2.0, 64.0));
        std::uint64_t l = static_cast<std::uint64_t>(std::fmod(val, std::pow(2.0, 64.0)));
        return BigCount{h, l};
    } catch (...) {
        return BigCount{};
    }
}

BigCount BigCount::combination(int n, int k)
{
    if (k < 0 || k > n) return BigCount{};
    if (k == 0 || k == n) return BigCount{1ULL};
    if (k > n - k) k = n - k;

    // n ≤ 50 → C(n,k) ≤ C(50,25) ≈ 1.26e14，安全使用 uint64_t
    std::uint64_t result = 1;
    for (int i = 1; i <= k; ++i) {
        result = result * static_cast<std::uint64_t>(n - k + i)
               / static_cast<std::uint64_t>(i);
    }
    return BigCount{result};
}

BigCount& BigCount::operator+=(const BigCount& other)
{
    if (other.is_zero()) return *this;
    if (is_zero()) {
        *this = other;
        return *this;
    }

    std::uint64_t sum_lo = lo + other.lo;
    std::uint64_t carry = (sum_lo < lo) ? 1ULL : 0;

    // 128 位加法，检测溢出
    std::uint64_t hi_sum = hi + other.hi;
    bool overflow = (hi_sum < hi);
    hi_sum += carry;
    overflow = overflow || (carry && hi_sum == 0);

    if (overflow) {
        // 饱和为最大值（DBLP 规模不会触发）
        lo = UINT64_MAX;
        hi = UINT64_MAX;
    } else {
        lo = sum_lo;
        hi = hi_sum;
    }
    return *this;
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
    std::cout << "开始统计聚团(退化排序 + Forward DFS + 批量组合数加速)...\n";
    clique_counts_ = count_cliques_by_order();
    std::size_t max_order = 0;
    for (std::size_t order = 1; order < clique_counts_.size(); ++order) {
        if (!clique_counts_[order].is_zero()) {
            max_order = order;
        }
    }
    std::cout << "聚团统计完成: max_order=" << max_order
              << " (统计上限阶数=" << max_clique_order << ")\n";
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
    return indexed::normalize(value);
}

std::vector<std::string> IndexBuilder::tokenize(const std::string& text)
{
    return indexed::tokenize(text);
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
    return indexed::stable_hash(value);
}

// ============================================================================
// 聚团分析：统计各阶完全子图个数
//
// 算法: 退化排序 + Forward-neighbor 限深 DFS + 批量组合数加速
//   1. 退化排序，顶点按 rank 0..N-1 重新编号
//   2. forward_neighbors[r] = rank > r 且在原图中相邻的顶点（按 rank 排序）
//   3. forward_sets[r]  = 同上，但是 unordered_set 用于 O(1) 邻接判断
//   4. DFS 从每个顶点出发，在 forward neighbors 中递归扩展
//      - 若候选集内所有顶点两两有 forward edge，则它们构成完全子图
//      → 直接用 C(candidate_count, add) 批量累加各阶子团数量
//      - 否则逐个顶点加入，递归继续
//   5. 每个 k-clique 恰好被其 rank 最小的顶点"拥有"，计数恰好一次
//
// 内存: 邻接表 + forward_neighbors + forward_sets（数百 MB）
// ============================================================================
std::vector<BigCount> IndexBuilder::count_cliques_by_order() const
{
    const int node_count = static_cast<int>(author_string_ids_.size());
    std::vector<BigCount> counts;
    counts.resize(static_cast<std::size_t>(max_clique_order) + 1);

    if (node_count == 0) return counts;

    // ---- 构建邻接表（排序去重）----
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

    // ---- 退化排序 (degeneracy ordering) ----
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(node_count));
    std::vector<int> degree(static_cast<std::size_t>(node_count), 0);
    std::vector<bool> removed(static_cast<std::size_t>(node_count), false);

    using DegreePair = std::pair<int, int>;
    std::priority_queue<DegreePair, std::vector<DegreePair>, std::greater<DegreePair>> queue;
    for (int i = 0; i < node_count; ++i) {
        degree[static_cast<std::size_t>(i)] = static_cast<int>(neighbors[static_cast<std::size_t>(i)].size());
        queue.push({ degree[static_cast<std::size_t>(i)], i });
    }

    while (!queue.empty()) {
        const auto [cur_deg, v] = queue.top();
        queue.pop();
        if (removed[static_cast<std::size_t>(v)] || cur_deg != degree[static_cast<std::size_t>(v)])
            continue;
        removed[static_cast<std::size_t>(v)] = true;
        order.push_back(v);
        for (int u : neighbors[static_cast<std::size_t>(v)]) {
            if (!removed[static_cast<std::size_t>(u)]) {
                --degree[static_cast<std::size_t>(u)];
                queue.push({ degree[static_cast<std::size_t>(u)], u });
            }
        }
    }

    // rank_of_node[original_id] = rank position
    std::vector<int> rank_of_node(static_cast<std::size_t>(node_count), 0);
    for (int r = 0; r < node_count; ++r) {
        rank_of_node[static_cast<std::size_t>(order[static_cast<std::size_t>(r)])] = r;
    }

    // ---- 构建 forward_neighbors (使用 rank 编号, 而非原始 ID) ----
    // 对于每个顶点 (按 rank 顺序), 记录其 rank 更大的邻居的 rank
    std::vector<std::vector<int>> forward_neighbors(static_cast<std::size_t>(node_count));
    for (int r = 0; r < node_count; ++r) {
        const int v = order[static_cast<std::size_t>(r)];
        auto& fw = forward_neighbors[static_cast<std::size_t>(r)];
        for (int u : neighbors[static_cast<std::size_t>(v)]) {
            const int ru = rank_of_node[static_cast<std::size_t>(u)];
            if (r < ru)  // 只保留 rank > 当前顶点的邻居
                fw.push_back(ru);
        }
        std::sort(fw.begin(), fw.end());
    }

    // ---- 构建 forward_sets (unordered_set, O(1) 查询) ----
    std::vector<std::unordered_set<int>> forward_sets(static_cast<std::size_t>(node_count));
    for (int r = 0; r < node_count; ++r) {
        const auto& fw = forward_neighbors[static_cast<std::size_t>(r)];
        auto& fs = forward_sets[static_cast<std::size_t>(r)];
        fs.reserve(fw.size());
        for (int neighbor_rank : fw)
            fs.insert(neighbor_rank);
    }

    // ---- 辅助函数 ----
    // 单次计数（仅 3–max_clique_order 阶）
    auto add_count = [&](std::size_t order_size) {
        if (order_size >= 3 && order_size <= max_clique_order)
            counts[order_size] += BigCount{1ULL};
    };

    // 检查候选集是否构成完全子图（所有顶点两两有 forward edge）
    auto is_complete = [&](const std::vector<int>& candidates) {
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto& nbr_set = forward_sets[static_cast<std::size_t>(candidates[i])];
            for (std::size_t j = i + 1; j < candidates.size(); ++j) {
                if (nbr_set.find(candidates[j]) == nbr_set.end())
                    return false;
            }
        }
        return true;
    };

    // 批量组合数累加：当前团大小 = prefix_size, 候选集有 candidate_count 个顶点且构成完全子图
    // 从候选集中选 add 个顶点，形成 prefix_size+add 阶的团
    auto add_complete_suffix = [&](std::size_t prefix_size, std::size_t candidate_count) {
        const std::size_t start_add = (prefix_size >= 3) ? std::size_t{1}
                                         : static_cast<std::size_t>(3 - prefix_size);
        const std::size_t max_add = std::min(candidate_count,
                                              static_cast<std::size_t>(max_clique_order) - prefix_size);
        for (std::size_t add = start_add; add <= max_add; ++add) {
            counts[prefix_size + add] += BigCount::combination(
                static_cast<int>(candidate_count), static_cast<int>(add));
        }
    };

    // 双指针求交集：candidates[start..] ∩ neighbor_list（两者都已排序）
    auto intersect_fw = [](const std::vector<int>& candidates, std::size_t start,
                           const std::vector<int>& neighbor_list) {
        std::vector<int> result;
        result.reserve(std::min(candidates.size() - start, neighbor_list.size()));
        std::size_t i = start, j = 0;
        while (i < candidates.size() && j < neighbor_list.size()) {
            if (candidates[i] == neighbor_list[j]) {
                result.push_back(candidates[i]);
                ++i; ++j;
            } else if (candidates[i] < neighbor_list[j]) {
                ++i;
            } else {
                ++j;
            }
        }
        return result;
    };

    // ---- DFS 团枚举 ----
    std::uint64_t progress = 0;
    const std::uint64_t report_every = 10'000'000;

    std::function<void(const std::vector<int>&, std::size_t)> dfs;
    dfs = [&](const std::vector<int>& candidates, std::size_t current_size) {
        if (candidates.empty() || current_size >= max_clique_order)
            return;

        // 批量加速：若候选集两两相连，直接做组合数跳过大块枚举
        if (is_complete(candidates)) {
            add_complete_suffix(current_size, candidates.size());
            return;
        }

        // 逐个扩展
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const int next = candidates[i];
            add_count(current_size + 1);
            if (++progress % report_every == 0) {
                std::cout << "  已累计 " << progress << " 次扩展 (当前团阶数="
                          << (current_size + 1) << ")...\n";
            }

            if (current_size + 1 >= max_clique_order)
                continue;

            const std::vector<int> next_candidates =
                intersect_fw(candidates, i + 1, forward_neighbors[static_cast<std::size_t>(next)]);

            if (!next_candidates.empty())
                dfs(next_candidates, current_size + 1);
        }
    };

    std::cout << "  开始聚团分析 (max_order=" << static_cast<int>(max_clique_order) << ")\n"
              << "    节点数=" << node_count
              << ", 边数=" << edge_weights_.size() << "\n";

    for (int r = 0; r < node_count; ++r) {
        dfs(forward_neighbors[static_cast<std::size_t>(r)], 1);
    }

    // 1阶/2阶 用精确值覆盖（DFS 中也会统计但这里用常量更高效）
    counts[1] = BigCount{static_cast<std::uint64_t>(node_count)};
    counts[2] = BigCount{static_cast<std::uint64_t>(edge_weights_.size())};

    std::cout << "  聚团分析完成，总扩展次数=" << progress << "\n";
    return counts;
}

} // namespace indexed
