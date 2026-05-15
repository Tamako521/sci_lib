#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace indexed {

class StringPool {
public:
    std::uint32_t intern(const std::string& value);
    void add_loaded(std::string value);
    const std::string& get(std::uint32_t id) const;
    std::uint32_t find(const std::string& value) const;
    std::size_t size() const;
    const std::vector<std::string>& all_strings() const;
    void reserve(std::size_t count);

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string, std::uint32_t> ids_;
};

} // namespace indexed
