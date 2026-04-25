#ifndef XML_PARSER_HPP
#define XML_PARSER_HPP

#include <string>
#include <vector>
#include <fstream>

#include "xml_value.hpp"
#include "parse_result.hpp"
#include "string_pool.hpp"

class XmlParser {
public:
    XmlParser()  = default;
    ~XmlParser() = default;

    // 解析入口：流式读取 xml 文件，将所有 <article> 写入 records
    ParseResult parse(const std::string&     xml_path,
                      StringPool&            string_pool,
                      std::vector<XmlValue>& records,
                      const Database*        db);

private:
    static constexpr size_t CHUNK_SIZE = 8 * 1024 * 1024; // 8MB 分块

    std::ifstream file_;        // 文件流
    std::string   buffer_;      // 滑动窗口缓冲区
    size_t        pos_ = 0;     // 当前在 buffer_ 中的解析位置
    size_t        buffer_offset_ = 0; // buffer_ 起点在文件中的偏移量

    // ---- 流式读取 ----

    // 打开文件流
    ParseResult open_file(const std::string& xml_path);

    // 在 buffer_ 中从 pos_ 向前查找最后一个 "</article>" 的结束位置
    // 找不到则返回 npos
    size_t find_last_safe_boundary() const;

    // 丢弃 safe_boundary 之前已消费的数据，从文件追加一个 CHUNK 到 buffer_ 尾部
    // 返回 false 表示文件已读完且 buffer_ 无剩余数据
    bool refill();

    // 确保 buffer_ 中从 pos_ 起至少包含一条完整 <article>...</article>
    // 不够则自动 refill；仍不够说明文件结束
    bool ensure_complete_article();

    // ---- 解析 ----

    void skip_whitespace();                     // 跳过空白字符
    void skip_prologue();                       // 跳过 <?xml ...?> 和 <!-- ... -->
    void skip_until_article();                  // 跳到下一个 <article 起始位置

    ParseResult parse_article(StringPool&     string_pool,
                              XmlValue&       out,
                              const Database* db);  // 解析单条 <article>...</article>

    ParseResult parse_tag(std::string& tag_name,
                          std::string& text);       // 解析一对子标签 <tag>text</tag>

    ParseResult parse_attributes(XmlValue&   out,
                                 StringPool& string_pool); // 解析 mdate="..." key="..."

    // ---- 工具函数 ----

    bool eof() const;                           // buffer_ 已耗尽且文件已读完
    bool match(const std::string& prefix);      // 匹配 prefix 并推进 pos_
    size_t absolute_pos() const;                // 当前解析位置在文件中的偏移量
    std::string context(size_t pos) const;      // 输出错误附近文本
};

#endif
