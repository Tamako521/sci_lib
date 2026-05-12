#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <vector>
#include <memory>

#include "common\parse_result.hpp"
#include "common\xml_value.hpp"

class StringPool;
class Database;

class Serializer{
private:
    // static unsigned int version;//确定版本
    // std::string mtime_check;//格式修改时间
    struct Header{
        char magic[4];
        uint64_t string_pool_size;
        uint64_t records_size;
    };
    std::unique_ptr<Header> header;

    struct Xml{
        uint32_t mdate_id;
        uint32_t key_id_;   
        uint32_t title_id_;
        uint32_t journal_id_;
        uint32_t volume_id_;
        uint32_t month_id_;
        uint32_t year_id_;
        uint32_t author_size;
        uint32_t cdrom_size;
        uint32_t ee_size;
    };
    std::unique_ptr<Xml> xml;

    //错误处理函数
    ParseResult error_read(const std::ifstream &f) const;
    ParseResult error_write(const std::ofstream &f) const;
    //articles.dat——>XmlValue+StringPool
    ParseResult read_header(std::ifstream& f);
    ParseResult read_string_pool(std::ifstream& f,StringPool& pool);
    ParseResult read_records(std::ifstream& f,std::vector<XmlValue>& records,const Database* db);

    //StringPool+XmlValue——>articles.dat
    ParseResult write_header(std::ofstream& f,const uint32_t string_pool_size,const uint32_t records_size);
    ParseResult write_string_pool(std::ofstream& f,const StringPool& pool);
    ParseResult write_records(std::ofstream& f,const std::vector<XmlValue>& records);

public:
    //反序列化(加载):articles.dat——>XmlValue+StringPool
    ParseResult load(
        const std::string&     cache_path,
        StringPool&            out_string_pool,
        std::vector<XmlValue>& out_records,
        const Database*        db_
    );

    //序列化(存储):StringPool+XmlValue——>articles.dat
    ParseResult save(
        const std::string&           cache_path,
        const StringPool&            string_pool,
        const std::vector<XmlValue>& records
    );
    Serializer();

    ~Serializer();
};

#endif
