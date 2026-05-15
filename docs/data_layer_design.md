# 索引化公共数据层设计与使用说明

本文档说明当前公共数据层的职责、接口和索引文件读取方式。当前版本已经不再使用旧的 `Database::load(dblp.xml)`、`XmlParser`、`Serializer`、全量 `records_` 常驻内存方案。

公共数据层位于：

```text
src/common/
  database.hpp / database.cpp
  xml_value.hpp / xml_value.cpp
  string_pool.hpp / string_pool.cpp
```

相关类型仍放在 `namespace indexed` 中，目的是区分索引版实现。

---

## 1. 数据层如何使用

### 1.1 打开索引

业务模块统一从 `indexed::Database` 打开索引：

```cpp
indexed::Database db;
if (!indexed::Database::has_index("data/index") || !db.open("data/index")) {
    // 提示先运行 index_builder.exe
}
```

说明：

- 传入的是索引目录，不是 `dblp.xml` 路径。
- 默认索引目录为 `data/index/`。
- GUI 不负责构建索引；缺少索引时只提示用户运行 `index_builder.exe`。

### 1.2 按需读取论文

数据层不会把全部文章加载进内存。读取论文详情时使用 `record_id`：

```cpp
auto article = db.read_article(record_id);
if (article) {
    article->title();
    article->author_count();
    article->author_at(0);
}
```

批量读取搜索结果时：

```cpp
auto articles = db.read_articles(record_ids, 0, 100);
```

其中 `offset` 和 `limit` 用于分页。

### 1.3 使用 XmlValue

`XmlValue` 表示一篇 `<article>`。它内部保存字符串 ID，并通过 `Database::get_string()` 反查真实字符串。

常用接口：

| 接口 | 含义 |
|------|------|
| `key()` | DBLP 唯一 key |
| `title()` | 论文标题 |
| `author_count()` | 作者数量 |
| `author_at(i)` | 第 i 个作者 |
| `authors()` | 作者列表拷贝 |
| `year()` | 年份 |
| `journal()` | 期刊 |
| `volume()` | 卷号 |
| `month()` | 月份 |
| `cdroms()` | cdrom 字段列表 |
| `ees()` | 电子资源链接列表 |

字段缺失时返回 `"<missing_string>"`。

---

## 2. 当前查询接口

### 2.1 基础信息

| 接口 | 作用 |
|------|------|
| `size()` | 文章数量 |
| `get_string(id)` | 从字符串池取真实字符串 |
| `find_key_id(key)` | 查找 key 对应的 record_id |
| `find_by_key(key)` | 按 DBLP key 读取文章 |

### 2.2 搜索索引

| 接口 | 作用 |
|------|------|
| `author_records_exact(author)` | 作者名精确搜索 |
| `author_records_contains(author)` | 作者名包含搜索 |
| `title_records_exact(title)` | 完整标题精确搜索 |
| `title_word_records(query, match_all)` | 标题关键词搜索 |
| `field_records(field, value)` | 年份、期刊、卷号过滤 |

`field` 当前支持：

```text
year
journal
volume
```

### 2.3 统计和图接口

| 接口 | 作用 |
|------|------|
| `top_authors(limit)` | 读取发文量前 N 作者 |
| `yearly_hot_keywords(limit)` | 读取每年 TopN 标题关键词 |
| `coauthors(author)` | 查询作者合作邻接表 |
| `count_cliques_by_order()` | 读取聚团各阶数量 |
| `author_paper_counts()` | 读取所有作者发文量 |

---

## 3. 索引读取流程

### 3.1 总流程

```text
data/index/
  |
  v
Database::open()
  |
  +-- load_string_pool()
  +-- load_offsets()
  +-- load_lookup_files()
  +-- load_stats()
  +-- load_graph()
  +-- load_clique_stats()
  |
  v
打开 articles.dat 文件句柄，等待按需 seek 读取
```

`Database::open()` 必须在 `manifest.bin` 存在时才认为索引有效。

### 3.2 搜索读取流程

以作者搜索为例：

```text
作者名
  -> normalize
  -> author_lookup_ 查 author_id
  -> author_dir_ 查 offset/count
  -> seek author_index.dat 读取 record_id 列表
  -> read_articles() 按需读取前 100 条
```

标题关键词搜索类似，只是使用 `title_word_lookup_`、`title_word_dir_` 和 `title_word_index.dat`。

### 3.3 论文详情读取流程

```text
record_id
  -> offsets_[record_id]
  -> seek articles.dat
  -> 读取单条文章二进制记录
  -> 生成 XmlValue
  -> 字符串字段通过 StringPool 反查
```

---

## 4. 索引文件职责

| 文件 | 作用 |
|------|------|
| `manifest.bin` | 索引构建完成标记 |
| `string_pool.dat` | 全局字符串池 |
| `articles.dat` | 完整文章记录 |
| `article_offsets.dat` | `record_id -> offset/length` |
| `key_index.dat` | `key -> record_id` |
| `author_lookup.dat` | 作者名到作者 ID |
| `author_index_dir.dat` | 作者倒排表目录 |
| `author_index.dat` | 作者倒排表 |
| `title_exact_dir.dat` | 标题哈希目录 |
| `title_exact_index.dat` | 完整标题候选记录 |
| `title_word_lookup.dat` | 标题词到词 ID |
| `title_word_index_dir.dat` | 标题词倒排表目录 |
| `title_word_index.dat` | 标题词倒排表 |
| `year_index_dir.dat / year_index.dat` | 年份过滤索引 |
| `journal_index_dir.dat / journal_index.dat` | 期刊过滤索引 |
| `volume_index_dir.dat / volume_index.dat` | 卷号过滤索引 |
| `stats_index.dat` | F3/F4 预计算统计 |
| `coauthor_graph.dat` | 作者合作图 |
| `clique_stats.dat` | F6 聚团统计结果 |

---

## 5. 核心类职责

### 5.1 Database

职责：运行时索引读取入口。

主要成员：

| 成员 | 含义 |
|------|------|
| `string_pool_` | 字符串池 |
| `offsets_` | 文章偏移表 |
| `author_lookup_` | 作者名到作者 ID |
| `author_dir_` | 作者倒排表目录 |
| `title_exact_dir_` | 标题哈希目录 |
| `title_word_lookup_` | 标题词到词 ID |
| `top_author_stats_` | Top 作者统计 |
| `yearly_keywords_` | 年度关键词统计 |
| `graph_` | 作者合作图邻接表 |
| `clique_counts_` | 聚团各阶数量 |
| `articles_` | `articles.dat` 文件流 |

### 5.2 XmlValue

职责：表示一篇文章，保存字段字符串 ID 和所属 `Database` 指针。

它不拥有完整字符串内容，真实字符串由 `StringPool` 保存。

### 5.3 StringPool

职责：去重保存字符串。

| 接口 | 作用 |
|------|------|
| `intern()` | 构建索引时写入字符串，返回 ID |
| `add_loaded()` | 运行时从 `string_pool.dat` 恢复字符串 |
| `get()` | 根据 ID 取字符串 |
| `find()` | 查找字符串对应 ID |
| `reserve()` | 预留空间 |

---

## 6. 组员注意事项

1. 不要再调用旧的 `Database::load()`，当前没有该接口。
2. 不要在 GUI 或业务模块里解析 `dblp.xml`。
3. 不要把所有论文读取到内存后再搜索或统计。
4. 搜索优先返回 `record_id`，展示时再按需读取文章。
5. F3/F4/F6 都是构建期预计算，运行时只读取结果。
6. 缺少 `clique_stats.dat` 时，GUI 聚团表会提示重新构建索引，其他功能仍可使用。
7. 当前只解析 `<article>`，统计口径也是 article 类型论文。
8. 作者名以 DBLP 原始字符串为准，不做同名消歧。
9. 索引格式变更或源 XML 更新后，应重新运行 `index_builder.exe`。
