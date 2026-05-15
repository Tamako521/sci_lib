#include "indexed/common/string_pool.hpp"

#include "index/index_format.hpp"

#include <stdexcept>

namespace indexed {

std::uint32_t StringPool::intern(const std::string& value)
{
    auto it = ids_.find(value);
    if (it != ids_.end()) {
        return it->second;
    }
    const auto id = static_cast<std::uint32_t>(strings_.size());
    strings_.push_back(value);
    ids_.emplace(strings_.back(), id);
    return id;
}

void StringPool::add_loaded(std::string value)
{
    const auto id = static_cast<std::uint32_t>(strings_.size());
    strings_.push_back(std::move(value));
    ids_[strings_.back()] = id;
}

const std::string& StringPool::get(std::uint32_t id) const
{
    static const std::string missing = "<missing_string>";
    if (id == format::invalid_id || id >= strings_.size()) {
        return missing;
    }
    return strings_[id];
}

std::uint32_t StringPool::find(const std::string& value) const
{
    auto it = ids_.find(value);
    return it == ids_.end() ? format::invalid_id : it->second;
}

std::size_t StringPool::size() const
{
    return strings_.size();
}

const std::vector<std::string>& StringPool::all_strings() const
{
    return strings_;
}

void StringPool::reserve(std::size_t count)
{
    strings_.reserve(count);
    ids_.reserve(count);
}

} // namespace indexed
