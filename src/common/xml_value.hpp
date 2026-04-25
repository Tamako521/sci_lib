#ifndef XML_VALUE_HPP
#define XML_VALUE_HPP
#include <string>
#include <vector>
#include <cstdint>

#define MISSING_STRING "<missing_string>"

class Database;
class XmlParser;
class Serializer;

class XmlValue{
private:
    friend class Database;
    friend class XmlParser;
    friend class Serializer;

    uint32_t mdate_id_   = INVALID_ID;
    uint32_t key_id_     = INVALID_ID;
    std::vector<uint32_t> author_ids_;
    uint32_t title_id_   = INVALID_ID;
    uint32_t journal_id_ = INVALID_ID;
    uint32_t volume_id_  = INVALID_ID;
    uint32_t month_id_   = INVALID_ID;
    uint32_t year_id_    = INVALID_ID;
    std::vector<uint32_t> cdrom_ids_;
    std::vector<uint32_t> ee_ids_;
    
    const Database* db_ = nullptr;   // 反查字符串池用

public:
    static constexpr uint32_t INVALID_ID = UINT32_MAX;

    const std::string& mdate()   const;
    const std::string& key()     const;
    std::vector<std::string> authors() const;  // 一条 article 可能有多位作者
    const std::string& title()   const;
    const std::string& journal() const;
    const std::string& volume()  const;
    const std::string& month()   const;
    const std::string& year()    const;
    std::vector<std::string> cdroms() const;
    std::vector<std::string> ees() const;

    //setter
    void setMdate(uint32_t id);
    void setKey(uint32_t id);
    void addAuthor(uint32_t id);       // 一条 article 可能有多位作者
    void setTitle(uint32_t id);
    void setJournal(uint32_t id);
    void setVolume(uint32_t id);
    void setMonth(uint32_t id);
    void setYear(uint32_t id);
    void addCdrom(uint32_t id);
    void addEe(uint32_t id);

    //test:输出函数
    void print_val() const;
};


#endif
