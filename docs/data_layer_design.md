# 数据层设计

本文档按当前代码实现更新，描述 `src/common` 中已经存在的数据层结构、数据流、对外接口和当前注意事项。早期设计中的 `Paper`、`DataStore`、`datastore.h/.cpp` 已不再是当前实现。

---

## 1. 当前目标

数据层负责从本地 `dblp.xml` 中读取文献信息，转换为内存中的 `XmlValue` 记录，并通过 `Database` 向上层模块提供统一访问接口。

当前实现的重点是：

- 使用 C++17 和标准库实现，不依赖外部数据库或第三方 XML 库。
- 面向大文件输入，`XmlParser` 采用滑动缓冲区按块读取。
- 使用 `StringPool` 对重复字符串做驻留，`XmlValue` 内部只保存字符串 ID。
- 使用 `Serializer` 将 `StringPool` 和 `XmlValue` 序列化为二进制缓存，减少重复解析成本。
- 使用 `ParseResult` 和 `ERROR` 宏返回、输出解析和读写错误。

---

## 2. 文件组织

当前数据层集中在 `src/common`：

```text
src/common/
├── database.hpp / .cpp       对外门面，负责加载、索引和查询
├── xml_value.hpp / .cpp      单条 article 记录视图
├── string_pool.hpp / .cpp    字符串驻留池
├── xml_parser.hpp / .cpp     流式 XML 解析器
├── serializer.hpp / .cpp     articles.dat 二进制缓存读写
└── parse_result.hpp          错误码、错误名转换和 ERROR 宏
```

当前可执行入口在 `test/test1.cpp`，用于加载 DBLP 数据并打印前若干条记录。

---

## 3. 总体数据流

```text
dblp.xml
  │
  ▼
Database::load(xml_path)
  │
  ├── 如果缓存存在：Serializer::load(...)
  │       └── 读 StringPool + vector<XmlValue>
  │
  └── 如果缓存不存在：XmlParser::parse(...)
          └── 写 StringPool + vector<XmlValue>

Database::rebuild_indices()
  │
  ├── key_index_
  ├── author_index_
  └── year_index_

上层模块通过 Database 查询或遍历 records_
```

`Database` 是当前数据层的唯一推荐入口。上层模块不应直接创建 `XmlParser` 或 `Serializer`，也不应绕过 `StringPool` 手动维护重复字符串。

---

## 4. `XmlValue`

`XmlValue` 表示一条 `<article>` 记录。它不直接持有字段字符串，而是保存字段在 `StringPool` 中的 `uint32_t` ID，并通过 `db_` 反查字符串。

当前字段：

```cpp
uint32_t mdate_id_;
uint32_t key_id_;
std::vector<uint32_t> author_ids_;
uint32_t title_id_;
uint32_t journal_id_;
uint32_t volume_id_;
uint32_t month_id_;
uint32_t year_id_;
uint32_t cdrom_id_;
uint32_t ee_id_;
const Database* db_;
```

当前对外 getter：

```cpp
const std::string& mdate() const;
const std::string& key() const;
std::vector<std::string> authors() const;
const std::string& title() const;
const std::string& journal() const;
const std::string& volume() const;
const std::string& month() const;
const std::string& year() const;
const std::string& cdrom() const;
const std::string& ee() const;
```

注意事项：

- `authors()` 当前返回 `std::vector<std::string>`，会复制作者字符串；如果频繁调用，后续可以考虑返回轻量视图或提供迭代接口。
- `year()` 当前返回字符串，不再使用早期设计中的 `unsigned int year`。
- `XmlValue` 依赖 `db_` 访问 `StringPool`，因此记录必须由 `Database`、`XmlParser` 或 `Serializer` 正确回填 `db_`。

---

## 5. `Database`

`Database` 是数据层门面，也是数据和索引的拥有者。

当前成员：

```cpp
std::vector<XmlValue> records_;
StringPool string_pool_;

std::unordered_map<std::string, size_t> key_index_;
std::unordered_map<std::string, std::vector<size_t>> author_index_;
std::unordered_map<std::string, std::vector<size_t>> year_index_;
```

当前对外接口：

```cpp
ParseResult load(const std::string& xml_path);

const std::vector<XmlValue>& all() const;
size_t size() const;

const XmlValue* find_by_key(const std::string& key) const;
std::vector<const XmlValue*> find_by_author(const std::string& author) const;
std::vector<const XmlValue*> find_by_year(std::string& year) const;
std::vector<const XmlValue*> find_by_title_keyword(const std::string& keyword);

const std::string& get_string(uint32_t id) const;
```

加载流程：

```text
Database::load(xml_path)
  1. 计算 cache_path = xml_path 所在目录 + "/articles.dat"
  2. 如果 articles.dat 不存在：
     - 清空 string_pool_ 和 records_
     - 调用 XmlParser::parse(...)
     - 调用 rebuild_indices()
     - 调用 Serializer::save(...)
  3. 如果 articles.dat 存在：
     - 调用 Serializer::load(...)
     - 调用 rebuild_indices()
```

查询实现：

- `find_by_key()` 通过 `key_index_` 精确查找。
- `find_by_author()` 通过 `author_index_` 返回作者对应的记录指针列表。
- `find_by_year()` 通过字符串形式的 `year_index_` 返回年份对应的记录指针列表。
- `find_by_title_keyword()` 当前使用 KMP 在标题中做大小写不敏感的子串匹配。

当前注意事项：

- `find_by_year(std::string& year)` 参数不是 `const std::string&`，后续建议改成只读引用。
- `find_by_title_keyword()` 当前仍是全量扫描，不依赖标题索引。
- `rebuild_indices()` 当前不会先清空索引；如果同一个 `Database` 对象多次调用 `load()`，后续应先清空 `key_index_`、`author_index_` 和 `year_index_`。
- `Database::load()` 当前意图使用 `articles.dat` 作为缓存文件，但 `Serializer::save()` 的参数名和实际打开路径仍需要进一步核对，避免误写到 XML 路径。

---

## 6. `StringPool`

`StringPool` 负责字符串驻留，避免在大量记录中重复保存相同字符串。

当前成员：

```cpp
std::vector<std::string> strings_;
std::unordered_map<std::string, uint32_t> id_map_;
size_t total_bytes_;
```

当前接口：

```cpp
uint32_t intern(const std::string& s);
const std::string& get(uint32_t id) const;
size_t size() const;
size_t total_bytes() const;
const std::vector<std::string>& all_strings() const;
void reserve(size_t count);
```

行为说明：

- `intern()` 如果字符串已存在，返回已有 ID；否则追加到 `strings_` 并记录映射。
- `get()` 根据 ID 返回字符串引用。
- `Serializer` 使用 `all_strings()` 顺序写出字符串池，读取时通过 `intern()` 恢复 ID 顺序。

当前注意事项：

- `get()` 会在 ID 越界时抛出 `std::out_of_range`，入口层需要捕获异常并输出错误。
- 当前 ID 使用 `uint32_t`，如果字符串数量超过范围，应在写入缓存前做检查。

---

## 7. `XmlParser`

`XmlParser` 是手写流式 XML 解析器。它只负责从 XML 中解析 `<article>` 记录，并把字段写入 `StringPool` 和 `records`。

当前核心状态：

```cpp
static constexpr size_t CHUNK_SIZE = 8 * 1024 * 1024;

std::ifstream file_;
std::string buffer_;
size_t pos_;
size_t buffer_offset_;
```

解析流程：

```text
parse(xml_path, string_pool, records, db)
  1. open_file(xml_path)
  2. refill() 读取首个块
  3. skip_prologue() 跳过 XML 声明和文档头
  4. 循环查找 "<article "
  5. ensure_complete_article() 确保当前缓冲区中有完整 article
  6. parse_article() 解析属性和子标签
  7. push 到 records
```

当前识别字段：

- `mdate`
- `key`
- `author`
- `title`
- `journal`
- `volume`
- `month`
- `year`
- `cdrom`
- `ee`

错误定位：

- `absolute_pos()` 返回当前解析位置在 XML 文件中的偏移量。
- `context(pos)` 输出错误位置附近的文本片段。
- 解析失败时通过 `ERROR` 输出 `ParseResult`、已解析记录数、offset 和 context。

当前限制：

- 只主动查找 `<article `，不会解析 `inproceedings`、`book` 等其他 DBLP 类型。
- `parse_tag()` 当前按简单 `<tag>text</tag>` 模式解析，不适合复杂嵌套标签或带属性的子标签。
- `ensure_complete_article()` 以 `</article>` 作为安全边界，所以目前仅适配 article 记录。
- 如果后续要支持更多 DBLP 记录类型，需要先抽象“记录起止标签”和字段映射。

---

## 8. `Serializer`

`Serializer` 负责将 `StringPool` 和 `vector<XmlValue>` 写入二进制缓存，并从缓存恢复内存结构。

当前缓存结构：

```cpp
struct Header {
    char magic[4];              // "DBLP"
    uint64_t string_pool_size;
    uint64_t records_size;
};

struct Xml {
    uint32_t mdate_id;
    uint32_t key_id_;
    uint32_t title_id_;
    uint32_t journal_id_;
    uint32_t volume_id_;
    uint32_t month_id_;
    uint32_t year_id_;
    uint32_t cdrom_id_;
    uint32_t ee_id_;
    uint32_t author_size;
};
```

文件格式：

```text
Header
  magic[4]
  string_pool_size
  records_size

StringPool
  重复 string_pool_size 次：
    uint32_t str_len
    char[str_len] bytes

Records
  重复 records_size 次：
    Xml 固定字段
    重复 author_size 次：
      uint32_t author_id
```

当前接口：

```cpp
ParseResult load(
    const std::string& cache_path,
    StringPool& out_string_pool,
    std::vector<XmlValue>& out_records,
    const Database* db
);

ParseResult save(
    const std::string& xml_path,
    const StringPool& string_pool,
    const std::vector<XmlValue>& records
);
```

当前注意事项：

- `Header::magic` 写入了 `"DBLP"`，但读取端当前尚未校验 magic。
- 早期文档中的 `version` 和 `xml_mtime` 失效检测尚未实现。
- `save()` 参数名仍是 `xml_path`，但从设计意图看它应该表示缓存输出路径；后续建议改名为 `cache_path` 并统一调用方。
- `load()` 读取 records 时会恢复 `XmlValue::db_`，这是 getter 能正常工作的前提。

---

## 9. `ParseResult` 和错误输出

`ParseResult` 统一描述解析、文件和缓存错误。

当前辅助能力：

```cpp
#define ERROR(msg) ...
inline const char* parse_result_name(ParseResult result);
```

使用约定：

- 数据层函数优先返回 `ParseResult`，不要随意 `throw`。
- 确实需要抛出的异常应在程序入口捕获，并用 `ERROR` 输出。
- 解析失败应输出具体 `ParseResult`、文件偏移、上下文片段和已解析记录数。

---

## 10. 当前优先级建议

后续如果继续完善数据层，建议按以下顺序处理：

1. 修正 `Serializer::save()` 的参数语义和实际缓存输出路径。
2. 在 `Serializer::load()` 中校验 `magic`，并考虑加入版本号。
3. 在 `Database::rebuild_indices()` 前清空旧索引，支持重复加载。
4. 改进 `XmlParser` 对 DBLP 实际 XML 的兼容性，尤其是非简单子标签。
5. 将 `find_by_year(std::string& year)` 改为 `const std::string&`。
6. 根据功能模块需求决定是否解析更多 DBLP 记录类型。
