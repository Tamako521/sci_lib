#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace indexed {

// 字符串规范化：去重空格、转小写
inline std::string normalize(const std::string& value)
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

// 分词：按非字母数字字符分割
inline std::vector<std::string> tokenize(const std::string& text)
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

// 稳定哈希（FNV-1a 变体）
inline std::uint64_t stable_hash(const std::string& value)
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
