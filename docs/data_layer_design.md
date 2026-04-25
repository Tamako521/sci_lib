# 公共数据层设计与使用说明

本文档给其他组员说明：公共数据层已经把 `dblp.xml` 解析并缓存为 `articles.dat`，使用时只需要通过 `Database` 读取 `XmlValue`，不需要直接处理 XML 或二进制文件。

---

## 1. 公共数据如何使用

### 1.1 加载数据

业务模块统一从 `Database` 入口加载：

```cpp
Database db;
db.load("D:/LearnSpaces/GitSpace/sci_lib/dblp.xml");
```

说明：

- 传入的是 `dblp.xml` 路径。
- 如果同目录下已有 `articles.dat`，会直接加载缓存。
- 如果没有 `articles.dat`，会先解析 XML，再自动生成缓存。

### 1.2 使用 XmlValue

`Database::all()` 返回全部文章记录：

```cpp
for (const XmlValue& article : db.all()) {
    article.title();
    article.year();
    article.authors();
}
```

`XmlValue` 常用字段：

| 接口 | 含义 |
|------|------|
| `key()` | DBLP 唯一 key |
| `title()` | 论文标题 |
| `authors()` | 作者列表 |
| `year()` | 年份 |
| `journal()` | 期刊 |
| `mdate()` | 修改日期 |
| `cdroms()` | cdrom 字段列表 |
| `ees()` | 电子版链接列表 |

字段缺失时返回 `"<missing_string>"`。

### 1.3 使用已建立索引

公共数据层已经建立三个索引：

```text
key_index_     key    -> 一篇文章
author_index_  作者名 -> 多篇文章
year_index_    年份   -> 多篇文章
```

对应接口：

| 接口 | 用途 |
|------|------|
| `find_by_key(key)` | 按 DBLP key 精确查找 |
| `find_by_author(author)` | 按作者名精确查找 |
| `find_by_year(year)` | 按年份查找 |
| `find_by_title_keyword(keyword)` | 按标题关键字模糊查找 |

各成员建议：

- F1/F5 搜索：优先使用查询接口。
- F2/F6 合作图：遍历 `db.all()`，使用 `authors()`。
- F3 作者统计：遍历 `db.all()`，统计 `authors()`。
- F4 热点分析：遍历 `db.all()`，使用 `year()` 和 `title()`。
- F7 可视化：使用 F1-F6 输出结果，不直接访问 XML 或缓存。

---

## 2. 解析流程和存储流程

### 2.1 总流程

```text
第一次运行：

dblp.xml
   │
   ▼
XmlParser 解析 article
   │
   ▼
StringPool + vector<XmlValue>
   │
   ├── rebuild_indices()
   │
   └── Serializer::save()
             │
             ▼
        articles.dat


后续运行：

articles.dat
   │
   ▼
Serializer::load()
   │
   ▼
StringPool + vector<XmlValue>
   │
   ▼
rebuild_indices()
```

### 2.2 解析流程

```text
open dblp.xml
  │
  ▼
按 8MB 分块读入
  │
  ▼
寻找 <article ...>...</article>
  │
  ▼
解析 mdate/key 和子标签
  │
  ▼
字符串进入 StringPool
  │
  ▼
XmlValue 只保存字符串 ID
```

解析到的主要字段：

```text
mdate, key, author, title, journal, volume, month, year, cdrom, ee
```

### 2.3 存储流程

核心逻辑可以理解为：

```cpp
if (articles.dat 不存在) {
    parse(dblp.xml);
    save(articles.dat);
} else {
    load(articles.dat);
}
```

存储时写入顺序：

```text
Header -> StringPool -> Records
```

读取时按同样顺序恢复：

```text
Header -> 恢复 StringPool -> 恢复 XmlValue 列表
```

---

## 3. articles.dat 数据布局

`articles.dat` 是二进制缓存文件，用来避免每次都重新解析大型 XML。

整体布局：

```text
┌──────────────────────────────┐
│ Header                       │
│ magic = "DBLP"               │
│ string_pool_size             │
│ records_size                 │
├──────────────────────────────┤
│ StringPool 区                │
│ str_len + string bytes       │
│ str_len + string bytes       │
│ ...                          │
├──────────────────────────────┤
│ Records 区                   │
│ XmlRecord + authors/cdrom/ee │
│ XmlRecord + authors/cdrom/ee │
│ ...                          │
└──────────────────────────────┘
```

单条字符串布局：

```text
uint32_t str_len
char[str_len] bytes
```

单条文章记录布局：

```text
XmlRecord 固定区
├── mdate_id
├── key_id
├── title_id
├── journal_id
├── volume_id
├── month_id
├── year_id
├── author_size
├── cdrom_size
└── ee_size

变长区
├── author_ids[author_size]
├── cdrom_ids[cdrom_size]
└── ee_ids[ee_size]
```

理解方式：

```text
StringPool 保存真实字符串
XmlValue / XmlRecord 保存字符串 ID
ID 再反查 StringPool 得到真实字段
```

---

## 4. 核心类职责

### 4.1 Database

职责：公共数据层入口，拥有全部数据和索引。

主要成员：

| 成员 | 含义 |
|------|------|
| `records_` | 全部 `XmlValue` 记录 |
| `string_pool_` | 字符串池 |
| `key_index_` | key 到文章下标 |
| `author_index_` | 作者到文章下标列表 |
| `year_index_` | 年份到文章下标列表 |

主要接口：

| 接口 | 作用 |
|------|------|
| `load()` | 加载 XML 或缓存 |
| `all()` | 获取全部文章 |
| `size()` | 获取文章数量 |
| `find_by_key()` | key 精确查找 |
| `find_by_author()` | 作者精确查找 |
| `find_by_year()` | 年份查找 |
| `find_by_title_keyword()` | 标题关键字查找 |

### 4.2 XmlValue

职责：表示一篇 article。

主要成员：

| 成员 | 含义 |
|------|------|
| `mdate_id_` | 修改日期字符串 ID |
| `key_id_` | DBLP key 字符串 ID |
| `author_ids_` | 作者字符串 ID 列表 |
| `title_id_` | 标题字符串 ID |
| `journal_id_` | 期刊字符串 ID |
| `volume_id_` | 卷号字符串 ID |
| `month_id_` | 月份字符串 ID |
| `year_id_` | 年份字符串 ID |
| `cdrom_ids_` | cdrom 字符串 ID 列表 |
| `ee_ids_` | ee 字符串 ID 列表 |
| `db_` | 反查字符串池用的 Database 指针 |

主要接口：`title()`、`authors()`、`year()` 等 getter，用来把 ID 转回字符串。

### 4.3 StringPool

职责：去重保存字符串。

```text
字符串 -> ID
ID -> 字符串
```

主要接口：

| 接口 | 作用 |
|------|------|
| `intern()` | 写入字符串，返回 ID |
| `get()` | 根据 ID 取字符串 |
| `all_strings()` | 序列化时取全部字符串 |

### 4.4 XmlParser

职责：从 `dblp.xml` 中解析 `<article>`，填充 `StringPool` 和 `records_`。

特点：

- 按块读取大文件。
- 当前只解析 `<article>`。
- 解析出的文本先进入 `StringPool`，再把 ID 写入 `XmlValue`。

### 4.5 Serializer

职责：读写 `articles.dat`。

主要接口：

| 接口 | 作用 |
|------|------|
| `save()` | 把 `StringPool + records_` 写入缓存 |
| `load()` | 从缓存恢复 `StringPool + records_` |

---

## 5. 组员注意事项

1. 其他模块只使用 `Database` 和 `XmlValue`，不要直接调用 `XmlParser` 或 `Serializer`。
2. `Database::load()` 传入的是 `dblp.xml` 路径，不是 `articles.dat` 路径。
3. `articles.dat` 需要上传和共享；`dblp.xml` 文件太大，不上传。
4. 如果更新了 `dblp.xml`，需要删除旧 `articles.dat` 后重新生成缓存。
5. 当前只解析 `<article>`，统计结果暂时不包含 `inproceedings`、`book` 等类型。
6. `find_by_year()` 需要传入 `std::string` 变量，不能直接传 `"2020"`。
7. `authors()`、`cdroms()`、`ees()` 会返回 vector 拷贝，大规模循环中不要重复无意义调用。
8. 字段缺失时返回 `"<missing_string>"`，统计和展示时需要自行过滤。
9. 标题关键字搜索目前是全量扫描，适合先完成需求；如果后续性能不够，再考虑倒排索引。
