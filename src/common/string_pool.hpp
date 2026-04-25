#ifndef STRING_POOL_HPP
#define STRING_POOL_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

class StringPool{
private:
    std::vector<std::string>                 strings_;//ID-->字符串
    std::unordered_map<std::string,uint32_t> id_map_;//字符串-->ID
    size_t total_bytes_=0;

public:
    StringPool() =default;
    ~StringPool()=default;

    uint32_t intern(const std::string& s);//写入
    const std::string& get(uint32_t id) const;//读取

    //元信息
    size_t size() const;
    size_t total_bytes() const;

    //序列化使用
    const std::vector<std::string>& all_strings() const;//返回stringPool
    void reserve(size_t count);
};

#endif