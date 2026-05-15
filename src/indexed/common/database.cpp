#include "indexed/common/database.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <type_traits>

namespace indexed {

namespace {
std::uint64_t stable_hash(const std::string& value)
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

template <typename K>
void load_dir_map(const std::filesystem::path& path, std::unordered_map<K, Database::Dir>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return;
    }
    std::uint64_t count = 0;
    format::read_pod(in, count);
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        if constexpr (std::is_same_v<K, std::uint64_t>) {
            format::TitleHashDirEntry e;
            format::read_pod(in, e);
            out[e.hash] = { e.offset, e.count };
        } else {
            format::PostingDirEntry e;
            format::read_pod(in, e);
            out[e.id] = { e.offset, e.count };
        }
    }
}
}

Database::~Database()
{
    if (articles_.is_open()) {
        articles_.close();
    }
}

bool Database::has_index(const std::filesystem::path& index_dir)
{
    return std::filesystem::exists(index_dir / "manifest.bin");
}

bool Database::open(const std::filesystem::path& index_dir)
{
    index_dir_ = index_dir;
    if (!has_index(index_dir_)) {
        return false;
    }
    if (!load_string_pool() || !load_offsets() || !load_lookup_files()) {
        return false;
    }
    load_stats();
    load_graph();
    load_clique_stats();
    articles_.open(index_dir_ / "articles.dat", std::ios::binary);
    return articles_.is_open();
}

bool Database::load_string_pool()
{
    std::ifstream in(index_dir_ / "string_pool.dat", std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::uint64_t count = 0;
    if (!format::read_pod(in, count)) {
        return false;
    }
    string_pool_.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        std::string value;
        if (!format::read_string(in, value)) {
            return false;
        }
        string_pool_.add_loaded(std::move(value));
    }
    return true;
}

bool Database::load_offsets()
{
    return format::read_vector_file(index_dir_ / "article_offsets.dat", offsets_);
}

bool Database::load_lookup_files()
{
    std::vector<format::PostingDirEntry> key_entries;
    if (!format::read_vector_file(index_dir_ / "key_index.dat", key_entries)) {
        return false;
    }
    key_lookup_.reserve(key_entries.size());
    for (const auto& e : key_entries) {
        key_lookup_[normalize(string_pool_.get(e.id))] = static_cast<std::uint32_t>(e.offset);
    }

    std::vector<format::PostingDirEntry> author_lookup_entries;
    if (!format::read_vector_file(index_dir_ / "author_lookup.dat", author_lookup_entries)) {
        return false;
    }
    author_string_ids_.resize(author_lookup_entries.size(), format::invalid_id);
    for (const auto& e : author_lookup_entries) {
        author_lookup_[normalize(string_pool_.get(e.id))] = static_cast<std::uint32_t>(e.offset);
        if (e.offset < author_string_ids_.size()) {
            author_string_ids_[static_cast<std::size_t>(e.offset)] = e.id;
        }
    }

    load_dir_map<std::uint32_t>(index_dir_ / "author_index_dir.dat", author_dir_);
    load_dir_map<std::uint64_t>(index_dir_ / "title_exact_dir.dat", title_exact_dir_);

    std::vector<format::PostingDirEntry> word_lookup_entries;
    format::read_vector_file(index_dir_ / "title_word_lookup.dat", word_lookup_entries);
    for (const auto& e : word_lookup_entries) {
        title_word_lookup_[normalize(string_pool_.get(e.id))] = static_cast<std::uint32_t>(e.offset);
    }
    load_dir_map<std::uint32_t>(index_dir_ / "title_word_index_dir.dat", title_word_dir_);
    load_dir_map<std::uint32_t>(index_dir_ / "year_index_dir.dat", year_dir_);
    load_dir_map<std::uint32_t>(index_dir_ / "journal_index_dir.dat", journal_dir_);
    load_dir_map<std::uint32_t>(index_dir_ / "volume_index_dir.dat", volume_dir_);
    return true;
}

bool Database::load_stats()
{
    std::ifstream in(index_dir_ / "stats_index.dat", std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::uint64_t top_count = 0;
    format::read_pod(in, top_count);
    for (std::uint64_t i = 0; i < top_count; ++i) {
        std::uint32_t author_id = 0;
        std::uint32_t count = 0;
        format::read_pod(in, author_id);
        format::read_pod(in, count);
        if (author_id < author_string_ids_.size()) {
            top_author_stats_.push_back({ string_pool_.get(author_string_ids_[author_id]), count });
        }
    }

    std::uint64_t year_count = 0;
    format::read_pod(in, year_count);
    for (std::uint64_t i = 0; i < year_count; ++i) {
        std::uint32_t year_id = 0;
        std::uint32_t item_count = 0;
        format::read_pod(in, year_id);
        format::read_pod(in, item_count);
        auto& items = yearly_keywords_[string_pool_.get(year_id)];
        for (std::uint32_t j = 0; j < item_count; ++j) {
            std::uint32_t word_id = 0;
            std::uint32_t count = 0;
            format::read_pod(in, word_id);
            format::read_pod(in, count);
            items.push_back({ string_pool_.get(word_id), count });
        }
    }
    return true;
}

bool Database::load_graph()
{
    std::ifstream in(index_dir_ / "coauthor_graph.dat", std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::uint64_t author_count = 0;
    format::read_pod(in, author_count);
    graph_.reserve(static_cast<std::size_t>(author_count));
    for (std::uint64_t i = 0; i < author_count; ++i) {
        std::uint32_t author_id = 0;
        std::uint32_t degree = 0;
        format::read_pod(in, author_id);
        format::read_pod(in, degree);
        auto& neighbors = graph_[author_id];
        neighbors.resize(degree);
        if (degree > 0) {
            in.read(reinterpret_cast<char*>(neighbors.data()),
                    static_cast<std::streamsize>(neighbors.size() * sizeof(format::WeightedNeighbor)));
        }
    }
    return true;
}

bool Database::load_clique_stats()
{
    return format::read_vector_file(index_dir_ / "clique_stats.dat", clique_counts_);
}

std::size_t Database::size() const
{
    return offsets_.size();
}

const std::string& Database::get_string(std::uint32_t id) const
{
    return string_pool_.get(id);
}

std::optional<XmlValue> Database::read_article(std::uint32_t record_id) const
{
    if (record_id >= offsets_.size()) {
        return std::nullopt;
    }
    return read_article_at(offsets_[record_id]);
}

std::optional<XmlValue> Database::read_article_at(format::ArticleOffset offset) const
{
    if (!articles_.is_open()) {
        return std::nullopt;
    }
    articles_.clear();
    articles_.seekg(static_cast<std::streamoff>(offset.offset), std::ios::beg);
    if (!articles_) {
        return std::nullopt;
    }

    XmlValue value;
    format::read_pod(articles_, value.mdate_id_);
    format::read_pod(articles_, value.key_id_);
    format::read_pod(articles_, value.title_id_);
    format::read_pod(articles_, value.journal_id_);
    format::read_pod(articles_, value.volume_id_);
    format::read_pod(articles_, value.month_id_);
    format::read_pod(articles_, value.year_id_);

    auto read_ids = [&](std::vector<std::uint32_t>& ids) {
        std::uint32_t count = 0;
        format::read_pod(articles_, count);
        ids.resize(count);
        if (count > 0) {
            articles_.read(reinterpret_cast<char*>(ids.data()),
                           static_cast<std::streamsize>(ids.size() * sizeof(std::uint32_t)));
        }
    };
    read_ids(value.author_ids_);
    read_ids(value.cdrom_ids_);
    read_ids(value.ee_ids_);
    value.db_ = this;
    if (!articles_) {
        return std::nullopt;
    }
    return value;
}

std::vector<XmlValue> Database::read_articles(const std::vector<std::uint32_t>& record_ids,
                                              std::size_t offset,
                                              std::size_t limit) const
{
    std::vector<XmlValue> result;
    if (offset >= record_ids.size() || limit == 0) {
        return result;
    }
    const std::size_t end = std::min(record_ids.size(), offset + limit);
    result.reserve(end - offset);
    for (std::size_t i = offset; i < end; ++i) {
        if (auto article = read_article(record_ids[i])) {
            result.push_back(std::move(*article));
        }
    }
    return result;
}

const XmlValue* Database::find_by_key(const std::string& key) const
{
    auto id = find_key_id(key);
    if (!id) {
        return nullptr;
    }
    auto article = read_article(*id);
    if (!article) {
        return nullptr;
    }
    last_found_ = std::move(*article);
    return &*last_found_;
}

std::optional<std::uint32_t> Database::find_key_id(const std::string& key) const
{
    auto it = key_lookup_.find(normalize(key));
    if (it == key_lookup_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::uint32_t> Database::read_posting(const std::string& file, Dir dir) const
{
    std::ifstream in(index_dir_ / file, std::ios::binary);
    if (!in.is_open() || dir.count == 0) {
        return {};
    }
    in.seekg(static_cast<std::streamoff>(dir.offset * sizeof(std::uint32_t)), std::ios::beg);
    std::vector<std::uint32_t> ids(dir.count);
    in.read(reinterpret_cast<char*>(ids.data()),
            static_cast<std::streamsize>(ids.size() * sizeof(std::uint32_t)));
    return ids;
}

std::vector<std::uint32_t> Database::author_records_exact(const std::string& author) const
{
    auto author_it = author_lookup_.find(normalize(author));
    if (author_it == author_lookup_.end()) {
        return {};
    }
    auto dir_it = author_dir_.find(author_it->second);
    return dir_it == author_dir_.end() ? std::vector<std::uint32_t>{}
                                       : read_posting("author_index.dat", dir_it->second);
}

std::vector<std::uint32_t> Database::author_records_contains(const std::string& author) const
{
    const std::string needle = normalize(author);
    std::vector<std::uint32_t> merged;
    if (needle.empty()) {
        return merged;
    }
    for (std::size_t author_id = 0; author_id < author_string_ids_.size(); ++author_id) {
        if (normalize(string_pool_.get(author_string_ids_[author_id])).find(needle) == std::string::npos) {
            continue;
        }
        auto dir_it = author_dir_.find(static_cast<std::uint32_t>(author_id));
        if (dir_it != author_dir_.end()) {
            merged = union_sorted(std::move(merged), read_posting("author_index.dat", dir_it->second));
        }
    }
    return merged;
}

std::vector<std::uint32_t> Database::title_records_exact(const std::string& title) const
{
    const std::string norm = normalize(title);
    auto dir_it = title_exact_dir_.find(stable_hash(norm));
    if (dir_it == title_exact_dir_.end()) {
        return {};
    }

    std::ifstream in(index_dir_ / "title_exact_index.dat", std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    in.seekg(static_cast<std::streamoff>(dir_it->second.offset * sizeof(format::TitleExactEntry)), std::ios::beg);
    std::vector<std::uint32_t> result;
    for (std::uint32_t i = 0; i < dir_it->second.count; ++i) {
        format::TitleExactEntry entry;
        format::read_pod(in, entry);
        if (normalize(string_pool_.get(entry.title_id)) == norm) {
            result.push_back(entry.record_id);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::uint32_t> Database::title_word_records(const std::string& query, bool match_all) const
{
    const auto words = tokenize(query);
    std::vector<std::uint32_t> result;
    bool initialized = false;
    for (const auto& word : words) {
        auto word_it = title_word_lookup_.find(word);
        if (word_it == title_word_lookup_.end()) {
            if (match_all) {
                return {};
            }
            continue;
        }
        auto dir_it = title_word_dir_.find(word_it->second);
        if (dir_it == title_word_dir_.end()) {
            if (match_all) {
                return {};
            }
            continue;
        }
        auto ids = read_posting("title_word_index.dat", dir_it->second);
        if (!initialized) {
            result = std::move(ids);
            initialized = true;
        } else if (match_all) {
            result = intersect_sorted(std::move(result), ids);
        } else {
            result = union_sorted(std::move(result), ids);
        }
    }
    return result;
}

std::vector<std::uint32_t> Database::field_records(const std::string& field, const std::string& value) const
{
    const std::uint32_t id = string_pool_.find(value);
    if (id == format::invalid_id) {
        return {};
    }
    const auto* dir_map = &year_dir_;
    const char* file = "year_index.dat";
    if (field == "journal") {
        dir_map = &journal_dir_;
        file = "journal_index.dat";
    } else if (field == "volume") {
        dir_map = &volume_dir_;
        file = "volume_index.dat";
    }
    auto it = dir_map->find(id);
    return it == dir_map->end() ? std::vector<std::uint32_t>{} : read_posting(file, it->second);
}

std::vector<AuthorStat> Database::top_authors(std::size_t limit) const
{
    std::vector<AuthorStat> result = top_author_stats_;
    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

YearKeywordTop Database::yearly_hot_keywords(std::size_t limit) const
{
    YearKeywordTop result = yearly_keywords_;
    for (auto& item : result) {
        if (item.second.size() > limit) {
            item.second.resize(limit);
        }
    }
    return result;
}

std::vector<std::pair<std::string, int>> Database::coauthors(const std::string& author) const
{
    auto author_it = author_lookup_.find(normalize(author));
    if (author_it == author_lookup_.end()) {
        return {};
    }
    auto graph_it = graph_.find(author_it->second);
    if (graph_it == graph_.end()) {
        return {};
    }
    std::vector<std::pair<std::string, int>> result;
    result.reserve(graph_it->second.size());
    for (const auto& n : graph_it->second) {
        if (n.author_id < author_string_ids_.size()) {
            result.push_back({ string_pool_.get(author_string_ids_[n.author_id]), static_cast<int>(n.weight) });
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });
    return result;
}

std::vector<std::pair<std::string, std::size_t>> Database::author_paper_counts() const
{
    std::vector<std::pair<std::string, std::size_t>> result;
    result.reserve(author_string_ids_.size());
    for (std::size_t id = 0; id < author_string_ids_.size(); ++id) {
        auto dir_it = author_dir_.find(static_cast<std::uint32_t>(id));
        result.push_back({ string_pool_.get(author_string_ids_[id]), dir_it == author_dir_.end() ? 0 : dir_it->second.count });
    }
    return result;
}

std::vector<std::uint64_t> Database::count_cliques_by_order() const
{
    return clique_counts_;
}

std::string Database::normalize(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    bool last_space = true;
    for (unsigned char ch : value) {
        if (std::isspace(ch)) {
            if (!last_space) {
                out.push_back(' ');
                last_space = true;
            }
        } else {
            out.push_back(static_cast<char>(std::tolower(ch)));
            last_space = false;
        }
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::vector<std::string> Database::tokenize(const std::string& text)
{
    std::vector<std::string> words;
    std::string current;
    for (unsigned char ch : text) {
        if (std::isalnum(ch)) {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        words.push_back(current);
    }
    return words;
}

std::vector<std::uint32_t> Database::intersect_sorted(std::vector<std::uint32_t> left,
                                                      const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> Database::union_sorted(std::vector<std::uint32_t> left,
                                                  const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    std::set_union(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

} // namespace indexed
