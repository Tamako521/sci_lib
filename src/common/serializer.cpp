#include "common\serializer.hpp"
#include "common\string_pool.hpp"
#include "common\process_resporter.hpp"
#include <fstream>
#include <iostream>
#include <cstdint>

ParseResult Serializer::error_read(const std::ifstream &f) const{
    if (f.fail()) {
        if (f.eof()) {
            std::cerr << "文件过早结束 (EOF)" << std::endl;
        }
        else if (f.bad()) {
            std::cerr << "流损坏/不可恢复错误" << std::endl;
        } 
        else {
            std::cerr << "读取失败" << std::endl;
        }
        std::cerr << "实际读取: " << f.gcount() << " 字节" << std::endl;

        return ParseResult::READ_FAILED;
    }
    return ParseResult::OK;
}

ParseResult Serializer::error_write(const std::ofstream &f) const {
    if (f.fail()) {
        if (f.bad()) {
            std::cerr << "流损坏/不可恢复错误" << std::endl;
        } else {
            std::cerr << "写入失败" << std::endl;
        }
        
        return ParseResult::WRITE_FAILED;
    }
    
    if (!f) {  // 检查流状态
        std::cerr << "写入操作失败" << std::endl;
        return ParseResult::WRITE_FAILED;
    }
    
    return ParseResult::OK;
}

ParseResult Serializer::read_header(std::ifstream &f){
    ParseResult ret;

    f.read(reinterpret_cast<char*>(header.get()),sizeof(Header));

    if((ret=error_read(f))!=ParseResult::OK) {
        ERROR("读取header失败");
        return ret;
    }

    return ParseResult::OK;
}

ParseResult Serializer::read_string_pool(std::ifstream &f, StringPool &pool){
    ParseResult ret;
    pool.reserve(header->string_pool_size);
    ProcessReporter reporter("读取StringPool", static_cast<size_t>(header->string_pool_size));
    for(uint64_t i=0;i<header->string_pool_size;i++){
        uint32_t str_len;

        f.read(reinterpret_cast<char*>(&str_len),sizeof(str_len));

        if((ret=error_read(f))!=ParseResult::OK){
            ERROR("读取StringPool失败：str_len读取失败");

            return ret;
        } 
        
        std::string str(str_len,'\0');

        f.read(&str[0],str_len);

        if((ret=error_read(f))!=ParseResult::OK){
            ERROR("读取StringPool失败：string读取失败");
            
            return ret;
        }

        pool.intern(str);
        reporter.report(static_cast<size_t>(i + 1));
    }
    return ParseResult::OK;
}

ParseResult Serializer::read_records(std::ifstream &f, std::vector<XmlValue> &records, const Database *db){
    ParseResult ret;
    records.reserve(header->records_size);
    ProcessReporter reporter("读取records", static_cast<size_t>(header->records_size));
    for(uint64_t i=0;i<header->records_size;i++){
        XmlValue value;
        f.read(reinterpret_cast<char*>(xml.get()),sizeof(Xml));
        if((ret=error_read(f))!=ParseResult::OK){
                ERROR("读取xml失败");
            
                return ret;
            }
        value.mdate_id_ = xml->mdate_id;
        value.key_id_ = xml->key_id_;
        value.title_id_ = xml->title_id_;
        value.journal_id_ = xml->journal_id_;
        value.volume_id_ = xml->volume_id_;
        value.month_id_ = xml->month_id_;
        value.year_id_ = xml->year_id_;
        
        uint32_t author_size = xml->author_size;
        for (uint32_t j = 0; j < author_size; j++) {
            uint32_t id;
            f.read(reinterpret_cast<char*>(&id), sizeof(uint32_t));
            if((ret=error_read(f))!=ParseResult::OK){
                ERROR("读取records失败：读取author_id失败");
            
                return ret;
            }
            value.author_ids_.push_back(id);
        }
        uint32_t cdrom_size = xml->cdrom_size;
        for (uint32_t j = 0; j < cdrom_size; j++) {
            uint32_t id;
            f.read(reinterpret_cast<char*>(&id), sizeof(uint32_t));
            if((ret=error_read(f))!=ParseResult::OK){
                ERROR("读取records失败：读取cdrom_id失败");
            
                return ret;
            }
            value.cdrom_ids_.push_back(id);
        }
        uint32_t ee_size = xml->ee_size;
        for (uint32_t j = 0; j < ee_size; j++) {
            uint32_t id;
            f.read(reinterpret_cast<char*>(&id), sizeof(uint32_t));
            if((ret=error_read(f))!=ParseResult::OK){
                ERROR("读取records失败：读取ee_id失败");
            
                return ret;
            }
            value.ee_ids_.push_back(id);
        }
        value.db_ = db;
        records.push_back(value);
        reporter.report(static_cast<size_t>(i + 1));
    }
    return ParseResult::OK;
}

ParseResult Serializer::write_header(std::ofstream &f, const uint32_t string_pool_size,const uint32_t records_size)
{
    ParseResult ret;
    header->magic[0]='D',header->magic[1]='B',header->magic[2]='L',header->magic[3]='P';
    header->records_size=records_size;
    header->string_pool_size=string_pool_size;

    f.write(reinterpret_cast<const char*>(header.get()),sizeof(Header));

    if ((ret = error_write(f)) != ParseResult::OK){
        ERROR("写入header失败");
        return ret;
    }
    
    return ParseResult::OK;
}

ParseResult Serializer::write_string_pool(std::ofstream &f, const StringPool &pool){
    ParseResult ret;

    const std::vector<std::string>& v=pool.all_strings();
    ProcessReporter reporter("写入StringPool", pool.size());

    for(size_t i=0;i<pool.size();i++){
        uint32_t str_len=static_cast<uint32_t>(v[i].size());

        f.write(reinterpret_cast<const char*>(&str_len),sizeof(str_len));

        if ((ret = error_write(f)) != ParseResult::OK){
            ERROR("写入StringPool失败：写入str_len失败");
            return ret;
        }

        f.write(v[i].data(),str_len);

        if ((ret = error_write(f)) != ParseResult::OK){
            ERROR("写入StringPool失败：写入str失败");
            return ret;
        }
        reporter.report(i + 1);
    }

    return ParseResult::OK;
}

ParseResult Serializer::write_records(std::ofstream &f, const std::vector<XmlValue> &records){
    ParseResult ret;
    ProcessReporter reporter("写入records", records.size());

    for(size_t i = 0; i < records.size(); i++){
        const XmlValue& value = records[i];
        
        xml->mdate_id = value.mdate_id_;
        xml->key_id_ = value.key_id_;
        xml->title_id_ = value.title_id_;
        xml->journal_id_ = value.journal_id_;
        xml->volume_id_ = value.volume_id_;
        xml->month_id_ = value.month_id_;
        xml->year_id_ = value.year_id_;
        xml->author_size = static_cast<uint32_t>(value.author_ids_.size());
        xml->cdrom_size = static_cast<uint32_t>(value.cdrom_ids_.size());
        xml->ee_size = static_cast<uint32_t>(value.ee_ids_.size());
        
        f.write(reinterpret_cast<const char*>(xml.get()), sizeof(Xml));

        if ((ret = error_write(f)) != ParseResult::OK){
            ERROR("写入records失败：写入xml失败");
            return ret;
        }
        
        for (uint32_t author_id : value.author_ids_) {
            f.write(reinterpret_cast<const char*>(&author_id), sizeof(uint32_t));

            if ((ret = error_write(f)) != ParseResult::OK){
                ERROR("写入records失败：写入author_id失败");
                return ret;
            }
        }
        for (uint32_t cdrom_id : value.cdrom_ids_) {
            f.write(reinterpret_cast<const char*>(&cdrom_id), sizeof(uint32_t));

            if ((ret = error_write(f)) != ParseResult::OK){
                ERROR("写入records失败：写入cdrom_id失败");
                return ret;
            }
        }
        for (uint32_t ee_id : value.ee_ids_) {
            f.write(reinterpret_cast<const char*>(&ee_id), sizeof(uint32_t));

            if ((ret = error_write(f)) != ParseResult::OK){
                ERROR("写入records失败：写入ee_id失败");
                return ret;
            }
        }
        reporter.report(i + 1);
    }

    return ParseResult::OK;
}

ParseResult Serializer::load(const std::string &cache_path, StringPool &out_string_pool, std::vector<XmlValue> &out_records, const Database *db_){
    ParseResult ret;

    std::ifstream f(cache_path, std::ios::binary);
    //检查流状态
    if (!f.is_open()) {
        ERROR("文件流未打开");
        return ParseResult::FILE_OPEN_FAILED;
    }

    if (f.bad()) {
        ERROR("流损坏");
        return ParseResult::FILESTREAM_BAD;
    }

    if((ret=read_header(f))!=ParseResult::OK) return ret;
    if((ret=read_string_pool(f,out_string_pool))!=ParseResult::OK) return ret;
    if((ret=read_records(f,out_records,db_))!=ParseResult::OK) return ret;

    f.close();
    return ParseResult::OK;
}

ParseResult Serializer::save(const std::string &cache_path, const StringPool &string_pool, const std::vector<XmlValue> &records){
    ParseResult ret;
    
    std::ofstream f(cache_path, std::ios::binary);
    //检查流状态
    if (!f.is_open()) {
        ERROR("文件流未打开: " << cache_path);
        return ParseResult::FILE_OPEN_FAILED;
    }

    if (f.bad()) {
        ERROR("流损坏");
        return ParseResult::FILESTREAM_BAD;
    }

    size_t len1 = string_pool.size();
    if (len1 > UINT32_MAX) {
        ERROR("");
        throw std::overflow_error("string_pool_size 超出 uint32_t 范围");
    }
    uint32_t string_pool_size=static_cast<uint32_t>(string_pool.size());

    size_t len2=records.size();
    if (len2 > UINT32_MAX) {
        ERROR("");
        throw std::overflow_error("records_size 超出 uint32_t 范围");
    }
    uint32_t records_size=static_cast<uint32_t>(records.size());

    if((ret=write_header(f,string_pool_size,records_size))!=ParseResult::OK) return ret;
    if((ret=write_string_pool(f,string_pool))!=ParseResult::OK) return ret;
    if((ret=write_records(f,records))!=ParseResult::OK) return ret;

    f.close();
    return ParseResult::OK;
}

Serializer::Serializer() 
    : header(std::make_unique<Header>())
    , xml(std::make_unique<Xml>())
{}

Serializer::~Serializer()=default;
