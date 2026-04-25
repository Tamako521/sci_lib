#pragma once

#include <iostream>
#define ERROR(msg) \
    std::cout << "[ERROR] " << __FILE__ << ":" << __LINE__ << " (" << __func__ << ") " << msg << std::endl

enum class ParseResult {
    OK = 0,                         // 成功

    ARTICLE_INCOMPLETE,
    // XML 解析错误
    FILE_NOT_FOUND,                 // 文件未找到
    FILE_OPEN_FAILED,               // 文件打开失败
    FILESTREAM_BAD,                 //file.good()=false，文件流不可用
    UNEXPECTED_EOF,                 // 意外的文件结束
    MALFORMED_XML,                  // XML 格式错误（标签未闭合、嵌套错误等）
    MISSING_REQUIRED_FIELD,         // 缺少必要字段（如 key、title）
    ENCODING_ERROR,                 // 编码错误

    // 序列化/反序列化错误
    CACHE_NOT_FOUND,                // 缓存文件不存在
    CACHE_STALE,                    // 缓存过期（xml 比缓存更新）
    CACHE_CORRUPTED,                // 缓存文件损坏（magic/校验不通过）
    READ_FAILED,                    // 读缓存失败
    WRITE_FAILED,                   // 写入缓存失败
};

inline const char* parse_result_name(ParseResult result) {
    switch (result) {
    case ParseResult::OK: return "OK";
    case ParseResult::ARTICLE_INCOMPLETE: return "ARTICLE_INCOMPLETE";
    case ParseResult::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
    case ParseResult::FILE_OPEN_FAILED: return "FILE_OPEN_FAILED";
    case ParseResult::FILESTREAM_BAD: return "FILESTREAM_BAD";
    case ParseResult::UNEXPECTED_EOF: return "UNEXPECTED_EOF";
    case ParseResult::MALFORMED_XML: return "MALFORMED_XML";
    case ParseResult::MISSING_REQUIRED_FIELD: return "MISSING_REQUIRED_FIELD";
    case ParseResult::ENCODING_ERROR: return "ENCODING_ERROR";
    case ParseResult::CACHE_NOT_FOUND: return "CACHE_NOT_FOUND";
    case ParseResult::CACHE_STALE: return "CACHE_STALE";
    case ParseResult::CACHE_CORRUPTED: return "CACHE_CORRUPTED";
    case ParseResult::READ_FAILED: return "READ_FAILED";
    case ParseResult::WRITE_FAILED: return "WRITE_FAILED";
    default: return "UNKNOWN_PARSE_RESULT";
    }
}
