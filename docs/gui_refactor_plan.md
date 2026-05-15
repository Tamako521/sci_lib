# GUI 开发与维护说明

本文档说明当前 GUI 的结构、调用关系和后续修改原则。当前 GUI 已经接入索引化数据层，不再负责解析 XML、构建搜索索引、统计作者或现场计算聚团。

---

## 一、开发原则

1. **GUI 只负责界面和交互**

   `src/gui` 负责输入框、按钮、表格、图表、弹窗和图形展示，不在 GUI 中实现大规模数据解析或全局统计。

2. **功能逻辑统一调用索引版模块**

   GUI 优先调用：

   - 数据读取：`indexed::Database`
   - 搜索：`indexed::SearchEngine`
   - 作者统计、关键词统计：`indexed::StatisticsAnalyzer`
   - 作者合作图、聚团统计：`indexed::AuthorGraph`

3. **GUI 不构建索引**

   如果 `data/index/manifest.bin` 不存在，GUI 只弹窗提示：

   ```text
   未检测到索引，请先运行 index_builder.exe 构建索引。
   ```

   不在 GUI 中解析 `dblp.xml`，也不调用 `IndexBuilder`。

4. **避免主界面卡死**

   搜索、统计、合作图和聚团分析都应读取索引或预计算结果。不要在按钮点击中执行全库扫描、全图聚团枚举等高耗时任务。

5. **保留 Qt Widgets**

   当前使用 Qt Widgets 和 QCustomPlot，不切换 QML，不大规模重写界面。

6. **每次修改后编译验证**

   修改 GUI 后至少验证：

   ```text
   qmake sci_lib.pro -spec win32-g++ CONFIG+=release
   mingw32-make -f Makefile.Release
   ```

   本项目在 Windows 下应优先使用 Qt 自带 MinGW，避免和其他 MinGW 版本混用。

---

## 二、当前 GUI 结构

主窗口位于：

```text
src/gui/mainwindow.h
src/gui/mainwindow.cpp
```

主要控件：

| 控件 / 成员 | 作用 |
|------------|------|
| `authorInput` | 作者搜索输入 |
| `titleInput` | 标题搜索输入 |
| `keywordInput` | 标题关键词输入 |
| `journalInput` | 期刊过滤 |
| `volumeInput` | 卷号过滤 |
| `yearInput` | 年份过滤 |
| `resultTable` | 文献搜索结果 |
| `graphView / graphScene` | 作者合作关系图 |
| `authorDetailTable` | 合作图右侧作者详情表 |
| `barChartPlot` | 年度关键词柱状图 |
| `authorTable` | 发文量前 100 作者 |
| `cliqueTable` | 聚团分析结果 |

核心数据成员：

| 成员 | 作用 |
|------|------|
| `indexed::Database m_db` | 索引数据入口 |
| `indexed::StatisticsAnalyzer m_stats` | 统计接口 |
| `indexed::AuthorGraph m_authorGraph` | 合作图接口 |
| `std::unique_ptr<indexed::SearchEngine> m_search` | 搜索接口 |
| `m_nodes` | 作者发文量缓存，用于合作图详情 |
| `m_tempNodes / m_tempEdges` | 当前合作图绘制数据 |

---

## 三、启动流程

当前启动流程：

```text
MainWindow()
  -> 创建搜索区、结果表、合作图、统计页、聚团页
  -> loadDblpXml("data/index")
       -> Database::has_index()
       -> Database::open()
       -> m_search = SearchEngine(&m_db)
       -> m_authorGraph.buildGraph(m_db)
       -> m_nodes = m_db.author_paper_counts()
       -> populateKeywordYearCombo()
       -> drawGraphicsBarChart()
       -> showCliqueStatistics()
```

注意：`loadDblpXml()` 是历史遗留函数名，当前实际加载的是索引目录。

---

## 四、搜索结果页面

搜索按钮触发 `onSearchClick()`。

搜索逻辑：

1. 读取作者、标题、关键词、期刊、卷号、年份输入。
2. 各条件分别查询索引，得到 `record_id` 集合。
3. 多条件之间取交集。
4. 最多显示前 100 条。
5. 使用 `Database::read_article(record_id)` 按需读取文章。
6. 填充 `resultTable`。

搜索结果列：

```text
标题 | 作者 | 年份 | 期刊 | 链接
```

双击搜索结果行会弹出论文详情。

---

## 五、作者合作关系图

作者合作图位于“作者合作关系图”tab。

当前布局：

```text
左侧 3/4：QGraphicsView 合作图
右侧 1/4：作者详情表
```

绘图流程：

```text
filterAuthorData(author)
  -> 查找中心作者
  -> AuthorGraph::queryCoauthors()
  -> 取合作次数最高的前 30 位合作者
  -> 填充 m_tempNodes / m_tempEdges

drawCooperationGraph()
  -> 绘制中心节点和外围节点
  -> 根据合作次数绘制不同颜色边
  -> 绘制图例
  -> 默认显示中心作者详情
```

作者详情表字段：

```text
作者姓名
累计发文量
直接合作者数量
与中心作者合作次数
```

点击节点后通过 `showAuthorDetail(authorName)` 刷新右侧详情表。节点下方不显示作者姓名，避免图中标签拥挤。

---

## 六、统计图表

### 6.1 作者排名

作者排名调用：

```cpp
m_stats.top_authors(m_db, 100)
```

GUI 展示：

```text
排名 | 作者姓名 | 累计发文量
```

支持升序和降序按钮切换。

### 6.2 年度标题关键词

关键词统计调用：

```cpp
m_stats.yearly_hot_keywords(m_db, 10)
```

GUI 提供年份下拉框，选择年份后用 QCustomPlot 绘制 Top10 柱状图。

---

## 七、聚团分析

聚团分析 tab 只显示结果表：

```text
阶数 | 完全子图个数
```

调用关系：

```cpp
m_authorGraph.countCliquesByOrder()
```

该接口读取构建期写入的 `clique_stats.dat`，GUI 不现场计算完全子图。

如果缺少聚团统计文件，表格显示：

```text
- | 缺少聚团统计，请重新运行 index_builder.exe 构建索引
```

---

## 八、后续修改清单

1. 保持 GUI 只读索引，不新增运行时全库扫描。
2. 新增搜索条件时，优先在 `Database` 中增加索引接口，再接 GUI。
3. 修改作者合作图时，保持最多展示 Top N 合作者，避免图过密。
4. 修改聚团分析时，优先改 `index_builder` 的预计算逻辑，不在 GUI 中计算。
5. 修改路径时同步更新 `sci_lib.pro`、`CMakeLists.txt` 和本文档。
6. 修改表格列名或展示字段时，同步更新 `readme.md` 和 `design.md` 中的功能描述。
