// 防止头文件被重复包含（避免重复编译报错）
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// 引入Qt主窗口类
#include <QMainWindow>
// 引入单行输入框控件（搜索框用）
#include <QLineEdit>
// 引入按钮控件
#include <QPushButton>
// 引入标签页控件（切换图表/关系图）
#include <QTabWidget>
// 引入图形视图控件（展示合作关系图）
#include <QGraphicsView>
// 引入图形场景（绘制关系图的画布）
#include <QGraphicsScene>
// 引入定时器（这里备用）
#include <QTimer>
// 引入第三方绘图库QCustomPlot（画柱状图用）
#include "qcustomplot.h"
#include <QTableWidget>
#include <QComboBox>

#include "indexed/analysis/statistics_analyzer.hpp"
#include "indexed/common/database.hpp"
#include "indexed/graph/AuthorGraph.hpp"
#include "indexed/search/search_engine.hpp"

#include <memory>
// 作者节点结构体：存储作者名 + 发表论文数量
struct AuthorNode {
    QString name;      // 作者姓名
    int paperCount;    // 该作者发表的论文总数
};

// 作者合作边结构体：存储两个作者 + 合作次数
struct AuthorEdge {
    QString a1;        // 作者1
    QString a2;        // 作者2
    int cooperateNum;  // 两人合作发表的论文数
};

// 论文数据结构体：存储一篇论文的完整信息
struct PaperData
{
    QString key;       // DBLP唯一key
    QString title;     // 论文标题
    QString author;    // 作者
    QString journal;   // 期刊
    QString volume;    // 卷号
    QString year;      // 年份
    QString month;     // 月份
    QString eeLink;    // 新增：DBLP论文链接
    QString keyword;
};


// 主窗口类，继承自Qt主窗口QMainWindow
class MainWindow : public QMainWindow
{
    // Qt信号槽宏，必须加，用于按钮点击等事件响应
    Q_OBJECT

public:
    // 构造函数：创建窗口时执行
    MainWindow(QWidget *parent = nullptr);
    // 析构函数：关闭窗口时执行（释放内存）
    ~MainWindow();

private slots:
    // 槽函数：点击搜索按钮时执行
    void onSearchClick();
    // 槽函数：点击搜索结果表格项时执行
    void onResultCellClick(int row, int column);
    void onCliqueAnalyzeClick();

private:
    // ===================== 搜索区界面控件 =====================
    QString searchTargetAuthor;// 新增：保存用户搜索的目标作者
    QLineEdit *authorInput;   // 作者名搜索输入框
    QLineEdit *titleInput;    // 论文标题搜索输入框
    QLineEdit *keywordInput;  // 关键词搜索输入框
    QLineEdit *journalInput;   // 期刊名搜索框
    QLineEdit *volumeInput;    // 卷号搜索框
    QLineEdit *yearInput;      // 年份搜索框
    QPushButton *searchBtn;   // 搜索按钮
    QTableWidget *resultTable;  // 搜索结果展示表格

    // ===================== 标签页与绘图控件 =====================
    QTabWidget *tabWidget;        // 标签页控件（切换关系图 / 统计图表）
    QGraphicsView *graphView;     // 视图控件：显示作者合作关系图
    QGraphicsScene *graphScene;   // 场景控件：绘制合作关系图的画布
    QCustomPlot *barChartPlot;    // 柱状图控件：显示论文发表年份统计

    // ===================== 全局原始数据 =====================
    indexed::Database m_db;             // src/indexed 数据库，作为 GUI 的统一数据源
    indexed::StatisticsAnalyzer m_stats; // src/indexed 统计模块
    indexed::AuthorGraph m_authorGraph;  // src/indexed 作者合作图模块
    std::unique_ptr<indexed::SearchEngine> m_search; // src/indexed 搜索模块
    QVector<AuthorNode> m_nodes;  // 存储所有作者节点数据

    // ===================== 临时绘图数据 =====================
    QVector<AuthorNode> m_tempNodes;  // 当前搜索的作者相关节点（绘图用）
    QVector<AuthorEdge> m_tempEdges;  // 当前搜索的作者合作边（绘图用）

    QTabWidget *searchBottomTab;
    QTableWidget *authorTable = nullptr;
     void showAuthorRankTable(bool desc);
    QWidget *authorRankWidget;
    QPushButton *btnAuthorAsc;
     QPushButton *btnAuthorDesc;
    QComboBox *keywordYearCombo = nullptr;
    QTableWidget *cliqueTable = nullptr;
    QPushButton *cliqueAnalyzeBtn = nullptr;
    // ===================== 核心功能函数 =====================
    // 从dblp.xml文件加载所有论文、作者、合作数据
    void loadDblpXml(const QString &filePath);
    // 根据搜索的作者名，筛选出该作者的关联数据
    void filterAuthorData(const QString &targetAuthor);
    // 绘制作者合作关系图
    void drawCooperationGraph();
    // 绘制年份统计柱状图
    void drawGraphicsBarChart();
    void populateKeywordYearCombo();
    void showCliqueStatistics();

    void showPaperDetails(const PaperData& paper);
};

// 结束头文件保护
#endif // MAINWINDOW_H

//这份头文件的整体作用总结
//定义了3 个数据结构：作者、合作关系、论文
//定义了主窗口所有控件：搜索框、按钮、结果列表、图表、关系图
//定义了存储数据的变量：原始数据 + 临时绘图数据
//定义了核心功能函数：读 XML、筛选数据、画图、统计
