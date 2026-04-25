# 科学文献管理系统 - 当前设计文档

本文档按当前仓库实现更新。早期设计中的 `Paper`、`DataStore`、`datastore.h/.cpp`、`search/`、`graph/`、`stats/`、`gui/` 等模块目前尚未落地，当前代码主要集中在公共数据层。

---

## 1. 项目概述

- **目标**：基于 DBLP 本地 XML 数据构建科学文献管理系统。
- **当前阶段**：已实现公共数据层雏形，包括 XML 解析、字符串驻留、二进制缓存、基础索引和查询接口。
- **数据源**：仓库根目录下的 `dblp.xml`。
- **约束**：不使用数据库系统，数据结构和缓存均由 C++ 标准库实现。
- **语言与构建**：C++17 + CMake。

---

## 2. 当前工程结构

```text
sci_lib/
├── CMakeLists.txt
├── dblp.xml
├── docs/
│   ├── design.md
│   └── data_layer_design.md
├── src/
│   └── common/
│       ├── database.hpp / .cpp
│       ├── xml_value.hpp / .cpp
│       ├── string_pool.hpp / .cpp
│       ├── xml_parser.hpp / .cpp
│       ├── serializer.hpp / .cpp
│       └── parse_result.hpp
└── test/
    └── test1.cpp
```

`builds/` 是 CMake/Visual Studio 生成目录，不属于源代码设计范围。

---

## 3. 构建目标

当前 `CMakeLists.txt` 定义两个核心目标：

```text
common  静态库，包含 src/common 下的数据层代码
sci_lib 可执行程序，当前由 test/test1.cpp 构建，用于加载和打印样例数据
```

当前构建约束：

- 使用 C++17。
- MSVC 使用 `/W4 /WX /utf-8`。
- GCC/Clang 使用 `-Wall -Wextra -Wpedantic -finput-charset=UTF-8`。
- Visual Studio 启动项目设置为 `sci_lib`。

---

## 4. 当前架构

```text
test/test1.cpp
  │
  ▼
Database
  │
  ├── XmlParser      解析 dblp.xml
  ├── Serializer     读写 articles.dat 缓存
  ├── StringPool     字符串去重和 ID 映射
  └── XmlValue       单条 article 记录
```

当前只有公共数据层已经实现。上层搜索、统计、图分析和 GUI 模块仍属于后续扩展方向。

---

## 5. 公共数据层

公共数据层的详细设计见 `docs/data_layer_design.md`。

核心类职责：

- `Database`：对外门面，负责加载、索引、查询和字符串反查。
- `XmlParser`：按块读取 XML，只解析 `<article>` 记录。
- `XmlValue`：保存单条 article 的字段 ID，通过 `Database` 取回字符串。
- `StringPool`：字符串驻留池，避免重复字符串占用额外内存。
- `Serializer`：负责二进制缓存读写。
- `ParseResult`：统一错误码，配合 `ERROR` 宏输出定位信息。

---

## 6. 当前已支持能力

- 从 XML 文件加载 article 数据。
- 将字符串字段写入 `StringPool`，记录中只保存 ID。
- 构建 `key`、`author`、`year` 三类索引。
- 通过 `key` 精确查询单条记录。
- 通过 `author` 查询作者相关记录。
- 通过 `year` 查询年份相关记录。
- 通过 KMP 对标题做大小写不敏感的关键字子串匹配。
- 通过 `Serializer` 读写二进制缓存数据。
- 解析失败时输出 `ParseResult`、文件偏移和上下文片段。

---

## 7. 当前限制

- 解析器目前只查找 `<article `，不处理 `inproceedings`、`book` 等其他 DBLP 记录类型。
- 子标签解析当前按简单 `<tag>text</tag>` 模式处理，复杂嵌套标签和带属性子标签需要继续增强。
- `Serializer` 尚未实现 `magic` 校验、版本号校验和 XML 更新时间校验。
- `Database::rebuild_indices()` 当前没有先清空旧索引，同一个对象重复加载时需要注意。
- `test/test1.cpp` 中仍使用本地绝对路径加载 `dblp.xml`，后续可改为命令行参数或相对路径。
- 搜索、统计、图分析和 GUI 模块尚未实现。

---

## 8. 后续模块规划

原始功能规划仍可作为后续方向，但需要基于当前 `Database` 接口重新设计。

| 模块 | 当前状态 | 建议依赖 |
|------|----------|----------|
| 基本搜索 | 部分已在 `Database` 中实现 | `find_by_key`、`find_by_author`、`find_by_title_keyword` |
| 作者统计 | 未实现 | `Database::all()` 或 `author_index_` 的公开统计接口 |
| 热点分析 | 未实现 | `Database::all()` 中的 `title()` 和 `year()` |
| 合作图分析 | 未实现 | `XmlValue::authors()` |
| GUI | 未实现 | 统一调用功能模块，不直接解析 XML |

---

## 9. 近期建议

1. 先稳定数据层，修正缓存输出路径、缓存校验和 XML 解析兼容性。
2. 给 `test/test1.cpp` 增加可配置 XML 路径，避免硬编码绝对路径。
3. 把 `Database` 的基础查询接口稳定下来，再让搜索、统计、图分析模块依赖它。
4. 如果需要解析更多 DBLP 记录类型，优先扩展 `XmlParser` 的记录类型抽象。
5. 在功能模块增加前，先补充小规模 XML 测试文件，降低每次调试对完整 `dblp.xml` 的依赖。
