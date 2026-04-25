#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <filesystem>
#include <cstdint>

#include "common\parse_result.hpp"
#include "common\xml_value.hpp"
#include "common\string_pool.hpp"

class Database{
private:
    std::vector<XmlValue> records_;
    StringPool string_pool_;

    std::unordered_map<std::string, size_t>  key_index_;
    std::unordered_map<std::string, std::vector<size_t>> author_index_;
    std::unordered_map<std::string, std::vector<size_t>> year_index_;

    void rebuild_indices();

public:
    Database() =default;
    ~Database()=default;
    Database(const Database&)            =delete;
    Database& operator=(const Database&) =delete;

    ParseResult load(const std::string& xml_path);//加载

    const std::vector<XmlValue>& all() const;//获取存储所有数据的vector
    size_t size() const;//获取articles数量

    //精确查询
    const XmlValue* find_by_key(const std::string& key) const;
    std::vector<const XmlValue*> find_by_author(const std::string& author) const;
    std::vector<const XmlValue*> find_by_year(std::string& year) const;

    //模糊查询
    std::vector<const XmlValue*> find_by_title_keyword(const std::string& keyword);


    const std::string& get_string(uint32_t id) const;//id-->string

};

#endif