#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace indexed {

// 大数：128 位精确整数，用于完全子图统计（最高约 3.4e38）
// 若溢出则饱和为最大值（对 DBLP 规模不会发生）
struct BigCount {
    std::uint64_t lo = 0;  // 低64位
    std::uint64_t hi = 0;  // 高64位

    BigCount() = default;
    explicit BigCount(std::uint64_t v) : lo(v), hi(0) {}
    BigCount(std::uint64_t h, std::uint64_t l) : lo(l), hi(h) {}

    bool is_zero() const { return lo == 0 && hi == 0; }

    std::string to_string() const;
    static BigCount from_string(const std::string& s);
    static BigCount combination(int n, int k);
    BigCount& operator+=(const BigCount& other);
};

namespace format {

constexpr std::uint32_t invalid_id = UINT32_MAX;

struct ArticleOffset {
    std::uint64_t offset = 0;
    std::uint32_t length = 0;
};

struct PostingDirEntry {
    std::uint32_t id = 0;
    std::uint64_t offset = 0;
    std::uint32_t count = 0;
};

struct TitleHashDirEntry {
    std::uint64_t hash = 0;
    std::uint64_t offset = 0;
    std::uint32_t count = 0;
};

struct TitleExactEntry {
    std::uint32_t title_id = invalid_id;
    std::uint32_t record_id = invalid_id;
};

struct WeightedNeighbor {
    std::uint32_t author_id = invalid_id;
    std::uint32_t weight = 0;
};

inline std::filesystem::path index_root()
{
    return std::filesystem::path("data") / "index";
}

inline std::filesystem::path xml_path()
{
    return std::filesystem::path("data") / "dblp.xml";
}

template <typename T>
void write_pod(std::ofstream& out, const T& value)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool read_pod(std::ifstream& in, T& value)
{
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

inline void write_string(std::ofstream& out, const std::string& value)
{
    const auto length = static_cast<std::uint32_t>(value.size());
    write_pod(out, length);
    out.write(value.data(), length);
}

inline bool read_string(std::ifstream& in, std::string& value)
{
    std::uint32_t length = 0;
    if (!read_pod(in, length)) {
        return false;
    }
    value.assign(length, '\0');
    if (length > 0) {
        in.read(value.data(), length);
    }
    return static_cast<bool>(in);
}

template <typename T>
void write_vector_file(const std::filesystem::path& path, const std::vector<T>& values)
{
    std::ofstream out(path, std::ios::binary);
    const auto count = static_cast<std::uint64_t>(values.size());
    write_pod(out, count);
    if (!values.empty()) {
        out.write(reinterpret_cast<const char*>(values.data()),
                  static_cast<std::streamsize>(values.size() * sizeof(T)));
    }
}

template <typename T>
bool read_vector_file(const std::filesystem::path& path, std::vector<T>& values)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::uint64_t count = 0;
    if (!read_pod(in, count)) {
        return false;
    }
    values.resize(static_cast<std::size_t>(count));
    if (!values.empty()) {
        in.read(reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(T)));
    }
    return static_cast<bool>(in);
}

} // namespace indexed::format

} // namespace indexed
