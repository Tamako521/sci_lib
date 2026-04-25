#include "common\string_pool.hpp"
#include <stdexcept>
uint32_t StringPool::intern(const std::string &s){
    auto it=id_map_.find(s);
    if(it!=id_map_.end()) return it->second;
    //不存在
    uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.push_back(s);
    id_map_.emplace(s,id);
    total_bytes_+=s.length();
    return id;
}

const std::string &StringPool::get(uint32_t id) const{
    if(id>=strings_.size())
        throw std::out_of_range("id: "+std::to_string(id)+" out of range");
    return strings_[id];
}

size_t StringPool::size() const{
    return strings_.size();
}

size_t StringPool::total_bytes() const{
    return total_bytes_;
}

const std::vector<std::string> &StringPool::all_strings() const{
    return strings_;
}

void StringPool::reserve(size_t count){
    strings_.reserve(count);
}
