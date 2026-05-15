# 索引化数据层改造设计

## 1. 背景

当前程序运行时会通过 `Database::load()` 加载 `dblp.xml` 或 `articles.dat`，并把所有论文记录反序列化到内存中的 `records_`。虽然 `articles.dat` 避免了每次重新解析 XML，但没有解决全量数据常驻内存的问题。GUI 构造阶段还会同步构建搜索、统计和作者合作图相关数据，因此完整 DBLP 数据下容易出现启动慢、内存高、界面卡顿。

本次改造目标是把底层数据存取方式从“全量加载到内存”改为“离线构建索引、运行时加载轻量索引、按需读取论文详情”。

## 2. 目标

- GUI 在索引已存在时，启动到可操作控制在 5 秒以内。
- GUI 正常运行时内存尽量控制在 1GB 以内。
- 搜索结果分页显示，每页默认 100 条。
- 完整 DBLP 数据的索引构建目标为 30 分钟以内。
- 最终覆盖 F1-F6 功能，但业务语义参考现有 `src/search`、`src/analysis`、`src/graph` 模块，不重新设计功能。
- GUI 不再 fallback 到旧的 `Database::load(dblp.xml)` 全量加载路径。

## 3. 路径约定

原始数据文件固定放在：

```text
data/dblp.xml
```

索引文件固定放在：

```text
data/index/
```

索引构建工具固定使用默认路径，不支持命令行参数：

```text
index_builder.exe
```

运行 `index_builder.exe` 时：

- 默认读取 `data/dblp.xml`。
- 默认输出到 `data/index/`。
- 如果检测到旧索引，命令行询问是否删除并重建。
- 如果用户输入 `n`，退出并不修改旧索引。
- 如果用户输入 `y`，清空旧索引并重新构建。

## 4. 总体架构

```text
data/dblp.xml
      |
      v
index_builder.exe
      |
      v
data/index/*.dat
      |
      v
sci_lib.exe GUI
      |
      v
indexed::Database / indexed::SearchEngine
indexed::StatisticsAnalyzer / indexed::AuthorGraph
```

新增代码集中放在 `src/index` 和 `src/indexed` 下。原 `src/common`、`src/search`、`src/analysis`、`src/graph`、`src/gui` 只作为参考和对照，暂时保留。

新旧模块中负责相同功能的类名保持一致，但新模块放入 `indexed` 命名空间，避免 C++ 同名冲突。

## 5. 代码结构

```text
src/index/
  index_builder.hpp
  index_builder.cpp
  index_writer.hpp
  index_writer.cpp
  index_format.hpp

src/indexed/common/
  database.hpp
  database.cpp
  serializer.hpp
  serializer.cpp
  string_pool.hpp
  string_pool.cpp
  xml_value.hpp
  xml_value.cpp

src/indexed/search/
  search_engine.hpp
  search_engine.cpp

src/indexed/analysis/
  statistics_analyzer.hpp
  statistics_analyzer.cpp

src/indexed/graph/
  AuthorGraph.hpp
  AuthorGraph.cpp

src/indexed/gui/
  mainwindow.h
  mainwindow.cpp
  qcustomplot.h
  qcustomplot.cpp
```

`src/indexed/gui/` 中的 GUI 代码形式与原 `src/gui/` 保持一致，直接参考/拷贝原 GUI 后做必要修改，不引入复杂 adapter，不做大规模 GUI 架构重构。

必要修改包括：

- include 路径改到 `src/indexed`。
- `Database`、`SearchEngine`、`StatisticsAnalyzer`、`AuthorGraph` 使用 `indexed` 命名空间下同名类。
- 启动时检测 `data/index/manifest.bin`。
- 缺少索引时弹窗提示运行 `index_builder.exe`。
- 搜索结果分页显示。
- 删除旧的全量加载 `dblp.xml` 路径。

## 6. 索引文件

索引文件使用自定义二进制格式。第一版不做索引格式版本号校验，不校验源 XML 文件大小和修改时间。索引格式变更或源文件更新时，由开发者手动删除并重建 `data/index/`。

建议文件：

```text
manifest.bin
string_pool.dat
articles.dat
article_offsets.dat
key_index.dat
author_lookup.dat
author_index_dir.dat
author_index.dat
title_exact_dir.dat
title_exact_index.dat
title_word_lookup.dat
title_word_index_dir.dat
title_word_index.dat
year_index_dir.dat
year_index.dat
journal_index_dir.dat
journal_index.dat
volume_index_dir.dat
volume_index.dat
stats_index.dat
coauthor_graph.dat
clique_stats.dat
```

### 6.1 manifest.bin

`manifest.bin` 是索引构建完成标记。构建工具必须最后写入该文件。

GUI 启动时必须检测：

- `manifest.bin` 是否存在。
- 必要索引文件是否存在。

如果缺少 `manifest.bin`，GUI 视为索引不存在或不完整，弹窗提示：

```text
未检测到索引，请先运行 index_builder.exe 构建索引。
```

### 6.2 string_pool.dat

保存全局字符串池。作者名、标题、期刊、年份、卷号、DBLP key、标题关键词等字符串统一存储一次，其他索引只引用 `string_id`。

这样可以减少重复字符串占用，并保持与现有 `StringPool` 设计思想一致。

### 6.3 articles.dat 与 article_offsets.dat

`articles.dat` 保存完整论文记录，但 GUI 启动时不全量加载。

`article_offsets.dat` 保存：

```text
record_id -> offset, length
```

读取论文详情时：

```text
record_id
  -> article_offsets.dat 找 offset/length
  -> seek articles.dat
  -> 读取单条论文记录
  -> string_id 反查 string_pool
  -> GUI 显示
```

`record_id` 按解析顺序从 0 递增。内部索引全部使用数字 `record_id`，同时通过 `key_index.dat` 保留 `key -> record_id` 能力。

### 6.4 author_lookup / author_index

作者索引用于作者搜索。

启动时加载轻量目录：

```text
author_lookup.dat:
  normalized_author_name -> author_id

author_index_dir.dat:
  author_id -> offset, count
```

大倒排列表保存在：

```text
author_index.dat:
  record_id list
```

作者精确搜索流程：

```text
author name
  -> author_lookup 得到 author_id
  -> author_index_dir 得到 offset/count
  -> seek author_index.dat 读取 record_id 列表
  -> 分页取 100 条
  -> read_articles(page_record_ids)
  -> GUI 显示
```

作者包含匹配不扫描论文，也不顺序遍历磁盘索引。运行时加载作者名列表到内存，包含匹配时扫描作者名列表，得到多个 `author_id`，再分别读取倒排列表、合并去重、分页显示。

### 6.5 title_exact 索引

标题搜索分为完整标题精确查找和标题关键词查找。

完整标题精确查找不顺序扫描 `title_exact_index.dat`。为控制内存，第一版推荐使用标题哈希目录：

```text
title_exact_dir.dat:
  normalized_title_hash -> offset, count

title_exact_index.dat:
  title_string_id, record_id
```

查询流程：

```text
input title
  -> normalize
  -> hash
  -> title_exact_dir 得到 offset/count
  -> seek title_exact_index.dat 读取候选项
  -> 通过 string_pool 取 title_string_id 对应标题
  -> normalize 后比较，处理 hash 冲突
  -> 得到 record_id
  -> 读取文章详情
```

这样避免把所有完整标题字符串都加载进内存。

### 6.6 title_word 索引

标题关键词搜索使用倒排索引。

构建时对标题分词，建立：

```text
title_word_lookup.dat:
  word -> word_id

title_word_index_dir.dat:
  word_id -> offset, count

title_word_index.dat:
  sorted record_id list
```

多关键词查询流程：

```text
input: "graph mining"
  -> tokenize: graph, mining
  -> lookup word_id
  -> 分别读取两个 record_id 有序列表
  -> 做交集
  -> 分页读取文章详情
```

第一版按空格/符号切分，不做复杂自然语言处理。具体分词语义参考现有 `src/search` 行为。

### 6.7 year / journal / volume 索引

字段过滤索引统一采用：

```text
field_index_dir.dat:
  field_string_id -> offset, count

field_index.dat:
  sorted record_id list
```

它们用于 GUI 多条件过滤：

```text
年份
期刊
卷号
```

搜索主条件得到 record_id 列表后，再与过滤字段列表做交集。

### 6.8 stats_index.dat

保存预计算统计结果：

- F3：发表论文数量最多的前 100 名作者。
- F4：每年标题关键词 Top10。

GUI 显示统计图表和作者排名时直接读取该文件，不再运行时扫描全库。

### 6.9 coauthor_graph.dat

保存作者合作图：

```text
author_id -> [(coauthor_id, weight)]
```

构建时每解析一篇论文，就对同篇论文作者两两加边：

```text
(author_a, author_b) -> weight++
```

运行时：

- F2 查询合作者直接读取某作者邻接表。
- F7 作者合作关系图直接使用邻接表绘图。
- F6 聚团分析不在 GUI 中现场计算，改为读取构建期预计算的 `clique_stats.dat`。

### 6.10 clique_stats.dat

保存 F6 聚团分析的预计算结果：

```text
counts[k] = k 阶完全子图数量，第一版限制统计到 8 阶
```

文件只保存各阶数量，不保存额外元信息。`index_builder.exe` 在构建合作图后统计 1 到 8 阶完全子图数量，并写入该文件。GUI 启动时读取该文件并预填聚团分析表格。

如果旧索引缺少 `clique_stats.dat`，GUI 其他功能照常使用，聚团分析表格显示：

```text
- | 缺少聚团统计，请重新运行 index_builder.exe 构建索引
```

## 7. 索引构建流程

`index_builder.exe` 流程：

```text
1. 检查 data/dblp.xml 是否存在
2. 检查 data/index/ 是否已有旧索引
3. 若已有，询问是否删除并重建
4. 创建/清理 data/index/
5. 流式解析 data/dblp.xml
6. 每解析一篇论文，生成临时 ArticleRecord
7. 立即写入 articles.dat 并记录 offset
8. 更新 key/author/title/keyword/field/stat/coauthor 中间索引
9. 基于作者合作图预计算 F6 各阶完全子图数量
10. 写出各索引文件和 `clique_stats.dat`
11. 最后写 manifest.bin
```

构建阶段允许使用比 GUI 更高的内存，但不能依赖“把所有论文记录保存到 `vector<XmlValue>` 后再处理”。构建器可以保留倒排索引、统计、合作图等中间结构；如果后续内存过高，再对部分倒排列表做分批写盘优化。

临时文章结构示例：

```cpp
struct ArticleRecord {
    uint32_t record_id;
    uint32_t key_id;
    std::vector<uint32_t> author_ids;
    uint32_t title_id;
    uint32_t journal_id;
    uint32_t volume_id;
    uint32_t year_id;
    uint32_t month_id;
    std::vector<uint32_t> ee_ids;
};
```

## 8. GUI 运行时流程

GUI 启动时：

```text
1. 检查 data/index/manifest.bin
2. 检查必要索引文件
3. 如果缺失，弹窗提示运行 index_builder.exe
4. 如果存在，indexed::Database 打开索引
5. 初始化 indexed::SearchEngine / StatisticsAnalyzer / AuthorGraph
6. 主界面可操作
```

GUI 不再执行：

```cpp
m_db.load("dblp.xml");
m_authorGraph.buildGraph(m_db);
```

搜索时：

```text
SearchEngine 查询索引 -> record_id 列表
分页取 100 条
Database 按 record_id 批量读取 articles.dat
GUI 填表
```

统计时：

```text
StatisticsAnalyzer 读取 stats_index.dat
GUI 显示表格或图表
```

合作关系图：

```text
AuthorGraph 从 coauthor_graph.dat 查询作者邻接表
GUI 绘图
```

聚团分析：

```text
Database 读取 clique_stats.dat
GUI 启动后预填表格，只显示阶数和数量
```

F6 不在 GUI 启动时或点击时计算。统计发生在 `index_builder.exe` 构建索引阶段。由于完整 DBLP 中存在数百作者论文，完整枚举所有阶完全子图会组合爆炸，第一版限制统计到 7 阶，并在大候选集本身为完全图时用组合数直接累加，避免逐个枚举。

## 9. 分阶段实施计划

### 阶段 1：骨架与索引有效性

- 新增 `index_builder.exe`。
- 新增 `src/index/*`。
- 新增 `src/indexed/common/database.*`。
- 生成并检测 `manifest.bin`。
- GUI 启动改为检测索引。
- 缺失索引时弹窗提示运行 `index_builder.exe`。

### 阶段 2：F1 搜索与文章随机读取

- 实现 `string_pool.dat`。
- 实现 `articles.dat`。
- 实现 `article_offsets.dat`。
- 实现 `key_index.dat`。
- 实现 `author_index`。
- 实现 `title_exact`。
- 实现 `indexed::Database::read_article(record_id)`。
- 实现 `indexed::SearchEngine` 作者/标题查询。
- GUI 搜索结果分页，每页 100 条。

### 阶段 3：F5 与字段过滤

- 实现 `title_word_index`。
- 实现多关键词交集。
- 实现 `year_index`、`journal_index`、`volume_index`。
- GUI 多条件搜索改为索引交集。

### 阶段 4：F3/F4 统计

- 实现 `stats_index.dat`。
- 构建阶段预计算 Top100 作者。
- 构建阶段预计算每年 Top10 标题关键词。
- GUI 作者排名和年度关键词图表改为读预计算结果。

### 阶段 5：F2 合作图

- 实现 `coauthor_graph.dat`。
- 实现 `indexed::AuthorGraph` 合作者查询。
- GUI 作者合作关系图改为读取图索引。

### 阶段 6：F6 聚团分析

- 基于构建期的作者合作图实现 1 到 8 阶完全子图计数。
- 将结果写入 `clique_stats.dat`。
- `Database` 读取 `clique_stats.dat`。
- GUI 聚团分析 tab 只显示：

```text
阶数 | 完全子图个数
```

- 根据完整数据运行表现，再决定是否预计算或限制阶数。
- GUI 删除“统计各阶完全子图”按钮，打开程序后直接预填表格。

## 10. 风险与处理

### 10.1 索引构建内存过高

风险：`author_index`、`title_word_index`、`coauthor_graph` 的中间结构可能较大。

处理：第一版允许构建器使用较多内存，但不保存完整论文对象。若仍超出可接受范围，再把倒排列表和图边做分批排序写盘。

### 10.2 二进制格式调试困难

风险：自定义二进制不方便人工查看。

处理：构建器输出摘要信息，包括记录数、作者数、标题词数量、合作边数量、各文件大小等。

### 10.3 新旧同名类冲突

风险：旧模块和新模块都存在 `Database`、`SearchEngine`、`StatisticsAnalyzer`、`AuthorGraph`。

处理：新模块放在 `namespace indexed`，GUI 显式使用 `indexed::`。

### 10.4 GUI 迁移期间结果不一致

风险：新旧搜索或统计结果不一致。

处理：旧模块暂时保留，用对照测试验证同一查询的结果数量和前若干条。

### 10.5 F6 聚团分析仍然慢

风险：统计所有完全子图本身可能组合爆炸。

处理：F6 放到索引构建阶段预计算，避免 GUI 卡顿。完整 DBLP 中存在数百作者论文，完整统计所有阶完全子图会达到指数级规模，因此第一版限制到 7 阶，并使用组合数剪枝处理大完全候选集。

## 11. 决策记录

- 采用离线索引构建 + GUI 只读索引 + 按需读取文章详情。
- 原始数据路径为 `data/dblp.xml`。
- 索引路径为 `data/index/`。
- 索引构建工具为独立命令行程序 `index_builder.exe`。
- 构建工具不支持命令行参数。
- 旧索引存在时询问是否重建。
- 构建直接写 `data/index/`，最后写 `manifest.bin`。
- GUI 不 fallback 到旧 `Database::load(dblp.xml)`。
- 索引文件使用自定义二进制格式。
- 保留字符串池。
- 完整论文记录写入 `articles.dat`，按 `record_id` 随机读取。
- 内部使用递增 `record_id`，同时保留 `key -> record_id`。
- 搜索、统计和图功能语义参考现有模块，不重新设计业务功能。
- 新建 `src/indexed` 模块，旧模块暂时保留用于对照。
- 新模块类名保持旧类名，但放入 `indexed` 命名空间。
- 新增 `src/indexed/gui/`，GUI 代码形式参考原 `src/gui/`，直接拷贝后小改。
- 搜索结果分页显示，每页默认 100 条。
- GUI 启动目标为索引存在时 5 秒内可操作。
- GUI 运行内存尽量控制在 1GB 以内。
- 完整 DBLP 索引构建目标为 30 分钟以内。
- 按阶段实施，最终覆盖 F1-F6。
- F6 聚团分析在索引构建阶段预计算，结果写入 `clique_stats.dat`。
- GUI 聚团分析页只保留结果表格；缺少 `clique_stats.dat` 时表格提示重新构建索引。