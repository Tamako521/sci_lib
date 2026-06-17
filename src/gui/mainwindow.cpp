// 引入主窗口头文件，包含类定义、控件、结构体声明
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QDialog>
#include <QPushButton>
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QMessageBox>
#include <QPainter>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QGraphicsItem>
#include <QHash>
#include <unordered_set>
#include <limits>
#include <set>
#include <filesystem>

using indexed::AuthorStat;
using indexed::KeywordStat;
using indexed::SearchMode;
using indexed::SearchResult;
using indexed::XmlValue;
using indexed::YearKeywordTop;

namespace {
constexpr int kDefaultGraphCoauthorLimit = 100;
constexpr int kMaxFullGraphCoauthors = 500;
constexpr int kCoauthorPageSize = 100;

std::filesystem::path projectDataIndexPath()
{
    std::filesystem::path appDir = QCoreApplication::applicationDirPath().toStdWString();
    const auto dirName = appDir.filename().wstring();

    std::filesystem::path projectRoot = appDir;
    if (dirName == L"release" || dirName == L"debug") {
        projectRoot = appDir.parent_path();
    }

    return projectRoot / "data" / "index";
}

QString toDisplayString(const std::string& value)
{
    if (value.empty() || value == MISSING_STRING) {
        return {};
    }
    return QString::fromStdString(value);
}

PaperData paperFromXmlValue(const XmlValue& article)
{
    PaperData paper;
    QStringList authors;
    for (size_t i = 0; i < article.author_count(); ++i) {
        const QString author = toDisplayString(article.author_at(i));
        if (!author.isEmpty()) {
            authors.append(author);
        }
    }

    paper.key = toDisplayString(article.key());
    paper.title = toDisplayString(article.title());
    paper.author = authors.join(",");
    paper.journal = toDisplayString(article.journal());
    paper.volume = toDisplayString(article.volume());
    paper.year = toDisplayString(article.year());
    paper.month = toDisplayString(article.month());

    const std::vector<std::string> ees = article.ees();
    if (!ees.empty()) {
        paper.eeLink = toDisplayString(ees.front());
    }
    paper.keyword = "DBLP";
    return paper;
}

std::string toStdString(const QString& value)
{
    return value.trimmed().toStdString();
}

bool containsText(const QString& text, const QString& key)
{
    return key.isEmpty() || text.contains(key, Qt::CaseInsensitive);
}

bool paperMatchesFilters(const PaperData& paper,
                         const QString& authorKey,
                         const QString& titleKey,
                         const QString& keywordKey,
                         const QString& journalKey,
                         const QString& volumeKey,
                         const QString& yearKey)
{
    if (!containsText(paper.author, authorKey)) return false;
    if (!containsText(paper.title, titleKey)) return false;
    if (!keywordKey.isEmpty()
        && !paper.title.contains(keywordKey, Qt::CaseInsensitive)
        && !paper.keyword.contains(keywordKey, Qt::CaseInsensitive)) {
        return false;
    }
    if (!containsText(paper.journal, journalKey)) return false;
    if (!containsText(paper.volume, volumeKey)) return false;
    if (!containsText(paper.year, yearKey)) return false;
    return true;
}
}
// -----------------------------------------------------------------------------
// 构造函数：程序启动时执行，负责创建界面、初始化控件、加载数据
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// 构造函数：程序启动时执行，负责创建界面、初始化控件、加载数据
// -----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // --------------------------
    // 1. 搜索区整体布局（垂直排列）
    // --------------------------
    QVBoxLayout *searchMainLayout = new QVBoxLayout;

    // ===================== 顶部三大模块切换按钮（手绘三段式） =====================
    QHBoxLayout *tabBtnLayout = new QHBoxLayout;
    QPushButton *btnSearchPage = new QPushButton("文献搜索");
    QPushButton *btnKeywordTop = new QPushButton("每年标题关键词Top10");
    QPushButton *btnAuthorTop = new QPushButton("发文量前100作者");
    QPushButton *btnCliqueStats = new QPushButton("聚团分析");

    tabBtnLayout->addWidget(btnSearchPage);
    tabBtnLayout->addWidget(btnKeywordTop);
    tabBtnLayout->addWidget(btnAuthorTop);
    tabBtnLayout->addWidget(btnCliqueStats);
    searchMainLayout->insertLayout(0, tabBtnLayout);

    // 设置窗口标题
    setWindowTitle("文献管理系统");
    resize(1200, 800);

    // 创建中心控件
    QWidget *centralW = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralW);
    setCentralWidget(centralW);

    // 1.1 作者搜索行
    QHBoxLayout *authorSearchLayout = new QHBoxLayout;
    QLabel *authorLabel = new QLabel("作者：");
    authorInput = new QLineEdit;
    authorInput->setPlaceholderText("输入完整作者名精确搜索");
    QPushButton *clearAuthorBtn = new QPushButton("清空");
    authorSearchLayout->addWidget(authorLabel);
    authorSearchLayout->addWidget(authorInput);
    authorSearchLayout->addWidget(clearAuthorBtn);
    connect(clearAuthorBtn, &QPushButton::clicked, this, [=](){
        authorInput->clear();
        resultTable->setRowCount(0);
        clearGraphView();
    });

    // 1.2 标题搜索行
    QHBoxLayout *titleSearchLayout = new QHBoxLayout;
    QLabel *titleLabel = new QLabel("标题：");
    titleInput = new QLineEdit;
    titleInput->setPlaceholderText("输入标题关键词搜索");
    QPushButton *clearTitleBtn = new QPushButton("清空");
    titleSearchLayout->addWidget(titleLabel);
    titleSearchLayout->addWidget(titleInput);
    titleSearchLayout->addWidget(clearTitleBtn);
    connect(clearTitleBtn, &QPushButton::clicked, this, [=](){
        titleInput->clear();
        resultTable->setRowCount(0);
        clearGraphView();
    });
    QHBoxLayout *keywordSearchLayout = new QHBoxLayout;
    QLabel *keywordLabel = new QLabel("关键词：");
    keywordInput = new QLineEdit;
    keywordInput->setPlaceholderText("输入关键词搜索");
    QPushButton *clearKeywordBtn = new QPushButton("清空");
    keywordSearchLayout->addWidget(keywordLabel);
    keywordSearchLayout->addWidget(keywordInput);
    keywordSearchLayout->addWidget(clearKeywordBtn);
    connect(clearKeywordBtn, &QPushButton::clicked, this, [=](){
        keywordInput->clear();
        resultTable->setRowCount(0);
        clearGraphView();
    });

    // 期刊搜索行
    QHBoxLayout *journalSearchLayout = new QHBoxLayout;
    QLabel *journalLabel = new QLabel("期刊：");
    journalInput = new QLineEdit;
    journalInput->setPlaceholderText("输入期刊名搜索");
    QPushButton *clearJournalBtn = new QPushButton("清空");
    journalSearchLayout->addWidget(journalLabel);
    journalSearchLayout->addWidget(journalInput);
    journalSearchLayout->addWidget(clearJournalBtn);
    connect(clearJournalBtn, &QPushButton::clicked, this, [=](){
        journalInput->clear();
        resultTable->setRowCount(0);
        clearGraphView();
    });
    QHBoxLayout *volumeSearchLayout = new QHBoxLayout;
    QLabel *volumeLabel = new QLabel("卷号：");
    volumeInput = new QLineEdit;
    volumeInput->setPlaceholderText("输入卷号搜索");
    QPushButton *clearVolumeBtn = new QPushButton("清空");
    volumeSearchLayout->addWidget(volumeLabel);
    volumeSearchLayout->addWidget(volumeInput);
    volumeSearchLayout->addWidget(clearVolumeBtn);
    connect(clearVolumeBtn, &QPushButton::clicked, this, [=](){
        volumeInput->clear();
        resultTable->setRowCount(0);
        clearGraphView();
    });

    // 年份搜索行
    QHBoxLayout *yearSearchLayout = new QHBoxLayout;
    QLabel *yearLabel = new QLabel("年份：");
    yearInput = new QLineEdit;
    yearInput->setPlaceholderText("输入年份搜索");
    QPushButton *clearYearBtn = new QPushButton("清空");
    yearSearchLayout->addWidget(yearLabel);
    yearSearchLayout->addWidget(yearInput);
    yearSearchLayout->addWidget(clearYearBtn);
    connect(clearYearBtn, &QPushButton::clicked, this, [=](){
        yearInput->clear();
        resultTable->setRowCount(0);
        clearGraphView();
    });

    // ===================== 页面切换逻辑 =====================
    connect(btnSearchPage, &QPushButton::clicked, this, [=](){
        authorInput->show();
        titleInput->show();
        keywordInput->show();
        journalInput->show();
        volumeInput->show();
        yearInput->show();

        clearAuthorBtn->show();
        clearTitleBtn->show();
        clearKeywordBtn->show();
        clearJournalBtn->show();
        clearVolumeBtn->show();
        clearYearBtn->show();

        searchBtn->show();

        authorLabel->show();
        titleLabel->show();
        keywordLabel->show();
        journalLabel->show();
        volumeLabel->show();
        yearLabel->show();

        searchBottomTab->show();
        tabWidget->hide();
        tabWidget->setTabVisible(0, true); // 恢复显示
        tabWidget->setTabVisible(1, true); // 恢复显示
        tabWidget->setTabVisible(2, true);
    });

    connect(btnKeywordTop, &QPushButton::clicked, this, [=](){
        // 隐藏所有搜索相关控件
        authorInput->hide();
        titleInput->hide();
        keywordInput->hide();
        journalInput->hide();
        volumeInput->hide();
        yearInput->hide();

        clearAuthorBtn->hide();
        clearTitleBtn->hide();
        clearKeywordBtn->hide();
        clearJournalBtn->hide();
        clearVolumeBtn->hide();
        clearYearBtn->hide();

        searchBtn->hide();

        authorLabel->hide();
        titleLabel->hide();
        keywordLabel->hide();
        journalLabel->hide();
        volumeLabel->hide();
        yearLabel->hide();

        searchBottomTab->hide();
        tabWidget->show();
        tabWidget->setCurrentIndex(0);
        tabWidget->setTabVisible(0, true);
        tabWidget->setTabVisible(1, false); // ✅ 隐藏作者表格标签
        tabWidget->setTabVisible(2, false);
        drawGraphicsBarChart();
    });

    // ✅ 修复：这里补全 })
    connect(btnAuthorTop, &QPushButton::clicked, this, [=](){
        // 隐藏所有搜索相关控件
        authorInput->hide();
        titleInput->hide();
        keywordInput->hide();
        journalInput->hide();
        volumeInput->hide();
        yearInput->hide();

        clearAuthorBtn->hide();
        clearTitleBtn->hide();
        clearKeywordBtn->hide();
        clearJournalBtn->hide();
        clearVolumeBtn->hide();
        clearYearBtn->hide();

        searchBtn->hide();

        authorLabel->hide();
        titleLabel->hide();
        keywordLabel->hide();
        journalLabel->hide();
        volumeLabel->hide();
        yearLabel->hide();

        searchBottomTab->hide();
        tabWidget->show();
        tabWidget->setCurrentIndex(1);
        tabWidget->setTabVisible(0, false); // ✅ 隐藏关键词统计图标签
        tabWidget->setTabVisible(1, true);
        tabWidget->setTabVisible(2, false);
        showAuthorRankTable(true);
    });

    connect(btnCliqueStats, &QPushButton::clicked, this, [=](){
        authorInput->hide();
        titleInput->hide();
        keywordInput->hide();
        journalInput->hide();
        volumeInput->hide();
        yearInput->hide();

        clearAuthorBtn->hide();
        clearTitleBtn->hide();
        clearKeywordBtn->hide();
        clearJournalBtn->hide();
        clearVolumeBtn->hide();
        clearYearBtn->hide();

        searchBtn->hide();

        authorLabel->hide();
        titleLabel->hide();
        keywordLabel->hide();
        journalLabel->hide();
        volumeLabel->hide();
        yearLabel->hide();

        searchBottomTab->hide();
        tabWidget->show();
        tabWidget->setCurrentIndex(2);
        tabWidget->setTabVisible(0, false);
        tabWidget->setTabVisible(1, false);
        tabWidget->setTabVisible(2, true);
    });

    // ==============================
    // 清空自动恢复
    // ==============================
    // 作者栏清空时，立即清空合作图
    connect(authorInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty())
        {
            clearGraphView();
            resultTable->setRowCount(0);
        }
    });
    // 其他搜索栏清空时，只有全部为空才清空表格
    connect(titleInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
        }
    });
    connect(keywordInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
        }
    });
    connect(journalInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
        }
    });
    connect(volumeInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
        }
    });
    connect(yearInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
        }
    });

    // 搜索按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    searchBtn = new QPushButton("搜索");
    btnLayout->addStretch();
    btnLayout->addWidget(searchBtn);

    searchMainLayout->addLayout(authorSearchLayout);
    searchMainLayout->addLayout(titleSearchLayout);
    searchMainLayout->addLayout(keywordSearchLayout);
    searchMainLayout->addLayout(journalSearchLayout);
    searchMainLayout->addLayout(volumeSearchLayout);
    searchMainLayout->addLayout(yearSearchLayout);
    searchMainLayout->addLayout(btnLayout);

    mainLayout->addLayout(searchMainLayout);

    // 搜索结果表格
    resultTable = new QTableWidget;
    resultTable->setColumnCount(5);
    resultTable->setHorizontalHeaderLabels({"标题", "作者", "年份", "期刊", "链接"});
    resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    resultTable->setAlternatingRowColors(true);
    resultTable->setWordWrap(true);
    resultTable->verticalHeader()->setVisible(false);
    resultTable->horizontalHeader()->setStretchLastSection(true);
    resultTable->setColumnWidth(0, 420);
    resultTable->setColumnWidth(1, 260);
    resultTable->setColumnWidth(2, 80);
    resultTable->setColumnWidth(3, 260);

    // 关系图
    graphView = new QGraphicsView;
    graphScene = new QGraphicsScene;
    graphView->setScene(graphScene);
    graphView->setRenderHint(QPainter::Antialiasing);
    graphView->setDragMode(QGraphicsView::ScrollHandDrag);
    graphView->setInteractive(true);
    // 启用 Ctrl+滚轮缩放 + 鼠标拖拽平移
    graphView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    graphView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    graphView->viewport()->installEventFilter(this);

    authorDetailTable = new QTableWidget;
    authorDetailTable->setColumnCount(2);
    authorDetailTable->setHorizontalHeaderLabels({"项目", "内容"});
    authorDetailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    authorDetailTable->setSelectionMode(QAbstractItemView::NoSelection);
    authorDetailTable->verticalHeader()->setVisible(false);
    authorDetailTable->horizontalHeader()->setStretchLastSection(true);
    authorDetailTable->setColumnWidth(0, 130);
    authorDetailTable->setWordWrap(true);
    clearAuthorDetail();

    QWidget *graphTabWidget = new QWidget;
    QVBoxLayout *graphTabLayout = new QVBoxLayout(graphTabWidget);
    graphTabLayout->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *graphControlLayout = new QHBoxLayout;
    graphSummaryLabel = new QLabel("请先搜索作者以查看合作关系");
    showAllGraphBtn = new QPushButton("显示全部图");
    showAllGraphBtn->setEnabled(false);
    graphControlLayout->addWidget(graphSummaryLabel);
    graphControlLayout->addStretch();
    graphControlLayout->addWidget(showAllGraphBtn);
    graphTabLayout->addLayout(graphControlLayout);

    QHBoxLayout *graphContentLayout = new QHBoxLayout;
    graphContentLayout->setContentsMargins(0, 0, 0, 0);
    graphContentLayout->addWidget(graphView, 3);
    graphContentLayout->addWidget(authorDetailTable, 1);
    graphTabLayout->addLayout(graphContentLayout);

    connect(graphScene, &QGraphicsScene::selectionChanged, this, [this]() {
        const QList<QGraphicsItem*> items = graphScene->selectedItems();
        for (QGraphicsItem *item : items) {
            const QVariant author = item->data(0);
            if (author.isValid() && !author.toString().isEmpty()) {
                showAuthorDetail(author.toString());
                return;
            }
        }
    });

    // 下方三标签：文献搜索结果 / 作者合作关系图 / 作者合作列表
    searchBottomTab = new QTabWidget;
    searchBottomTab->addTab(resultTable, "文献搜索结果");
    searchBottomTab->addTab(graphTabWidget, "作者合作关系图");

    // ========== Tab3: 作者合作列表 ==========
    QWidget *coauthorListWidget = new QWidget;
    QVBoxLayout *coauthorListLayout = new QVBoxLayout(coauthorListWidget);

    coauthorListTable = new QTableWidget;
    coauthorListTable->setColumnCount(3);
    coauthorListTable->setHorizontalHeaderLabels({"序号", "合作者姓名", "合作次数"});
    coauthorListTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    coauthorListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    coauthorListTable->setAlternatingRowColors(true);
    coauthorListTable->verticalHeader()->setVisible(false);
    coauthorListTable->horizontalHeader()->setStretchLastSection(true);
    coauthorListTable->setColumnWidth(0, 60);
    // 初始显示提示
    coauthorListTable->setRowCount(1);
    auto *tipItem = new QTableWidgetItem("请先搜索作者以查看合作列表");
    tipItem->setTextAlignment(Qt::AlignCenter);
    coauthorListTable->setSpan(0, 0, 1, 3);
    coauthorListTable->setItem(0, 0, tipItem);
    coauthorListTable->setEnabled(false); // 无数据时禁用

    coauthorListLayout->addWidget(coauthorListTable);
    QHBoxLayout *coauthorPageLayout = new QHBoxLayout;
    coauthorPrevBtn = new QPushButton("上一页");
    coauthorNextBtn = new QPushButton("下一页");
    coauthorPageLabel = new QLabel("第 0 / 0 页，共 0 位合作者");
    coauthorPrevBtn->setEnabled(false);
    coauthorNextBtn->setEnabled(false);
    coauthorPageLayout->addWidget(coauthorPrevBtn);
    coauthorPageLayout->addWidget(coauthorNextBtn);
    coauthorPageLayout->addStretch();
    coauthorPageLayout->addWidget(coauthorPageLabel);
    coauthorListLayout->addLayout(coauthorPageLayout);
    searchBottomTab->addTab(coauthorListWidget, "作者合作列表");

    mainLayout->addWidget(searchBottomTab, 99);

    // 统计图表 + 作者表格
    tabWidget = new QTabWidget;
    QWidget *keywordStatsWidget = new QWidget;
    QVBoxLayout *keywordStatsLayout = new QVBoxLayout(keywordStatsWidget);
    QHBoxLayout *keywordYearLayout = new QHBoxLayout;
    QLabel *keywordYearLabel = new QLabel("年份：");
    keywordYearCombo = new QComboBox;
    keywordYearCombo->setMinimumWidth(120);
    keywordYearLayout->addWidget(keywordYearLabel);
    keywordYearLayout->addWidget(keywordYearCombo);
    keywordYearLayout->addStretch();
    keywordStatsLayout->addLayout(keywordYearLayout);
    barChartPlot = new QCustomPlot;
    barChartPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    keywordStatsLayout->addWidget(barChartPlot);
    tabWidget->addTab(keywordStatsWidget, "统计图表");
    connect(keywordYearCombo, &QComboBox::currentTextChanged, this, [=](const QString&){
        drawGraphicsBarChart();
    });

    // ✅ 作者表格（正确位置）
    authorRankWidget = new QWidget;
    QVBoxLayout *authorRankLayout = new QVBoxLayout(authorRankWidget);
    QHBoxLayout *authorSortLayout = new QHBoxLayout;
    btnAuthorDesc = new QPushButton("发文量 降序(多→少)");
    btnAuthorAsc = new QPushButton("发文量 升序(少→多)");
    authorSortLayout->addWidget(btnAuthorDesc);
    authorSortLayout->addWidget(btnAuthorAsc);
    authorRankLayout->addLayout(authorSortLayout);

    authorTable = new QTableWidget;
    authorTable->setColumnCount(3);
    authorTable->setHorizontalHeaderLabels({"排名", "作者姓名", "累计发文量"});
    authorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    authorRankLayout->addWidget(authorTable);
    tabWidget->addTab(authorRankWidget, "发文量前100作者");

    QWidget *cliqueWidget = new QWidget;
    QVBoxLayout *cliqueLayout = new QVBoxLayout(cliqueWidget);

    cliqueTable = new QTableWidget;
    cliqueTable->setColumnCount(2);
    cliqueTable->setHorizontalHeaderLabels({"阶数", "完全子图个数"});
    cliqueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cliqueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    cliqueTable->verticalHeader()->setVisible(false);
    cliqueTable->horizontalHeader()->setStretchLastSection(true);
    cliqueTable->setColumnWidth(0, 100);
    cliqueLayout->addWidget(cliqueTable);
    tabWidget->addTab(cliqueWidget, "聚团分析");

    // ✅ 按钮绑定（现在才正确）
    connect(btnAuthorDesc, &QPushButton::clicked, this, [=](){
        showAuthorRankTable(true);
    });
    connect(btnAuthorAsc, &QPushButton::clicked, this, [=](){
        showAuthorRankTable(false);
    });

    mainLayout->addWidget(tabWidget, 0);

    // 信号槽
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::onSearchClick);
    connect(resultTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onResultCellClick);
    connect(coauthorListTable, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int /*col*/) { onCoauthorListDoubleClick(row, 0); });
    connect(showAllGraphBtn, &QPushButton::clicked, this, &MainWindow::showAllCooperationGraph);
    connect(coauthorPrevBtn, &QPushButton::clicked, this, [this]() {
        if (m_coauthorPage > 0) {
            --m_coauthorPage;
            updateCoauthorListTable();
        }
    });
    connect(coauthorNextBtn, &QPushButton::clicked, this, [this]() {
        const int totalPages = (m_allCoauthorEdges.size() + kCoauthorPageSize - 1) / kCoauthorPageSize;
        if (m_coauthorPage + 1 < totalPages) {
            ++m_coauthorPage;
            updateCoauthorListTable();
        }
    });

    // 加载项目主目录下的 data/index。
    loadDblpXml(QString::fromStdWString(projectDataIndexPath().wstring()));
    graphScene->setSceneRect(0, 0, 1200, 800);
    graphScene->clear();


    // 默认隐藏统计图
    searchBottomTab->show();
    tabWidget->hide();
}

// -----------------------------------------------------------------------------
// 析构函数：窗口关闭时执行（本项目无手动释放资源，所以空实现）
// -----------------------------------------------------------------------------
MainWindow::~MainWindow()
{

}



// -----------------------------------------------------------------------------
// 搜索按钮点击槽函数：核心搜索逻辑
// -----------------------------------------------------------------------------
void MainWindow::onSearchClick()
{
    // 防重复点击
    searchBtn->setEnabled(false);
    QTimer::singleShot(500, this, [=](){
        searchBtn->setEnabled(true);
    });

    // 读取每个独立搜索框内容
    QString authorKey = authorInput->text().trimmed();
    QString titleKey = titleInput->text().trimmed();
    QString keywordKey = keywordInput->text().trimmed();
    QString journalKey = journalInput->text().trimmed();
    QString volumeKey = volumeInput->text().trimmed();
    QString yearKey = yearInput->text().trimmed();

    // 保存当前搜索作者
    searchTargetAuthor = authorKey;
    resultTable->setRowCount(0);

    std::set<std::uint32_t> candidateSet;
    bool hasCandidateSet = false;

    auto applyRecordIds = [&](const std::vector<std::uint32_t>& ids) {
        if (ids.empty()) {
            candidateSet.clear();
            hasCandidateSet = true;
            return;
        }

        std::set<std::uint32_t> next(ids.begin(), ids.end());

        if (!hasCandidateSet) {
            candidateSet = std::move(next);
            hasCandidateSet = true;
            return;
        }

        for (auto it = candidateSet.begin(); it != candidateSet.end();) {
            if (next.find(*it) == next.end()) {
                it = candidateSet.erase(it);
            } else {
                ++it;
            }
        }
    };

    if (!authorKey.isEmpty()) {
        applyRecordIds(m_db.author_records_exact(toStdString(authorKey)));
    }
    if (!titleKey.isEmpty()) {
        applyRecordIds(m_db.title_word_records(toStdString(titleKey), true));
    }
    if (!keywordKey.isEmpty()) {
        applyRecordIds(m_db.title_word_records(toStdString(keywordKey), true));
    }
    if (!journalKey.isEmpty()) {
        applyRecordIds(m_db.field_records("journal", toStdString(journalKey)));
    }
    if (!volumeKey.isEmpty()) {
        applyRecordIds(m_db.field_records("volume", toStdString(volumeKey)));
    }
    if (!yearKey.isEmpty()) {
        applyRecordIds(m_db.field_records("year", toStdString(yearKey)));
    }

    auto appendPaperIfMatch = [&](const XmlValue& article) {
        const PaperData paper = paperFromXmlValue(article);
        if (paperMatchesFilters(paper, authorKey, titleKey, keywordKey, journalKey, volumeKey, yearKey)) {
            const int row = resultTable->rowCount();
            resultTable->insertRow(row);

            auto *titleItem = new QTableWidgetItem(paper.title);
            titleItem->setData(Qt::UserRole, paper.key);
            resultTable->setItem(row, 0, titleItem);
            resultTable->setItem(row, 1, new QTableWidgetItem(paper.author));
            resultTable->setItem(row, 2, new QTableWidgetItem(paper.year));
            resultTable->setItem(row, 3, new QTableWidgetItem(paper.journal));
            resultTable->setItem(row, 4, new QTableWidgetItem(paper.eeLink.isEmpty() ? "无" : "可打开"));
        }
    };

    if (hasCandidateSet) {
        int displayed = 0;
        for (std::uint32_t recordId : candidateSet) {
            if (displayed >= 100) {
                break;
            }
            if (auto article = m_db.read_article(recordId)) {
                appendPaperIfMatch(*article);
                ++displayed;
            }
        }
    } else {
        QMessageBox::information(this, "搜索提示", "请输入至少一个搜索条件。");
        searchBtn->setEnabled(true);
        clearGraphView();
        return;
    }

    // ===================== 只有作者有内容，才画关系图；作者为空 → 清空图 =====================
    if(searchTargetAuthor.isEmpty())
    {
        clearGraphView();
    }
    else
    {
        filterAuthorData(searchTargetAuthor);
        drawCooperationGraph();
    }

    if (resultTable->rowCount() == 0) {
        QMessageBox::information(this, "搜索结果", "未找到相关文献");
    }
}

// -----------------------------------------------------------------------------
// 加载dblp.xml文件：读取论文、作者、合作关系 全字段完整版
// -----------------------------------------------------------------------------
void MainWindow::loadDblpXml(const QString &filePath)
{
    m_nodes.clear();
    m_authorPaperCountMap.clear();
    m_authorCanonicalNameMap.clear();

    const std::filesystem::path indexPath = filePath.toStdWString();
    if (!indexed::Database::has_index(indexPath) || !m_db.open(indexPath)) {
        QMessageBox::warning(this, "索引缺失", "未检测到索引，请先运行 index_builder.exe 构建索引。");
        return;
    }

    m_search = std::make_unique<indexed::SearchEngine>(&m_db);
    m_authorGraph.buildGraph(m_db);

    const auto authorCounts = m_db.author_paper_counts();
    m_nodes.reserve(static_cast<int>(authorCounts.size()));
    m_authorPaperCountMap.reserve(static_cast<int>(authorCounts.size()));
    m_authorCanonicalNameMap.reserve(static_cast<int>(authorCounts.size()));
    for (const auto& item : authorCounts) {
        const QString authorName = QString::fromStdString(item.first);
        const int paperCount = static_cast<int>(item.second);
        m_nodes.append({authorName, paperCount});
        m_authorPaperCountMap.insert(authorName, paperCount);
        m_authorCanonicalNameMap.insert(authorName.toLower(), authorName);
    }

    // 控制台打印真实总量
    qDebug() << "====================================";
    qDebug() << "✅ 索引加载完成";
    qDebug() << "总论文数量：" << m_db.size();
    qDebug() << "总作者数量：" << m_nodes.size();
    qDebug() << "作者合作图已由索引打开";
    qDebug() << "====================================";

    // 自动刷新关键词图表
    populateKeywordYearCombo();
    if(!m_nodes.isEmpty()) drawGraphicsBarChart();
    showCliqueStatistics();
}

// -----------------------------------------------------------------------------
// 筛选目标作者的关联数据（只保留该作者 + 合作者）
// -----------------------------------------------------------------------------
void MainWindow::filterAuthorData(const QString &targetAuthor)
{
    m_tempNodes.clear(); // 清空临时节点
    m_tempEdges.clear(); // 清空临时边
    m_allCoauthorEdges.clear();
    m_graphShowsAllCoauthors = false;
    m_coauthorPage = 0;

    const QString canonicalAuthor = m_authorCanonicalNameMap.value(targetAuthor.toLower());
    const int targetPaperCount = authorPaperCount(canonicalAuthor);

    if (canonicalAuthor.isEmpty()) return; // 没找到作者 → 直接返回

    const std::vector<std::pair<std::string, int>> coauthors =
        m_authorGraph.queryCoauthors(canonicalAuthor.toStdString());

    // 全量合作数据只进入分页列表；关系图默认只取前 100 个，避免大量 QGraphicsItem 卡顿。
    auto sorted_coauthors = coauthors;
    std::sort(sorted_coauthors.begin(), sorted_coauthors.end(),
              [](const auto& a, const auto& b) {
                  if (a.second != b.second) {
                      return a.second > b.second;
                  }
                  return a.first < b.first;
              });

    m_allCoauthorEdges.reserve(static_cast<int>(sorted_coauthors.size()));
    for (const auto& coauthor : sorted_coauthors) {
        const QString name = QString::fromStdString(coauthor.first);
        m_allCoauthorEdges.append({canonicalAuthor, name, coauthor.second});
    }

    m_tempNodes.append({canonicalAuthor, targetPaperCount});
    rebuildGraphSubset(false);
}

int MainWindow::authorPaperCount(const QString& authorName) const
{
    return m_authorPaperCountMap.value(authorName, 0);
}

void MainWindow::rebuildGraphSubset(bool showAll)
{
    if (m_tempNodes.isEmpty()) {
        m_tempEdges.clear();
        return;
    }

    const QString centerAuthor = m_tempNodes.first().name;
    const int centerPaperCount = m_tempNodes.first().paperCount;
    m_tempNodes.clear();
    m_tempEdges.clear();
    m_tempNodes.append({centerAuthor, centerPaperCount});

    const int totalCoauthors = m_allCoauthorEdges.size();
    const int displayCount = showAll
        ? qMin(totalCoauthors, kMaxFullGraphCoauthors)
        : qMin(totalCoauthors, kDefaultGraphCoauthorLimit);
    m_graphShowsAllCoauthors = showAll && totalCoauthors <= kMaxFullGraphCoauthors;

    m_tempEdges.reserve(displayCount);
    m_tempNodes.reserve(displayCount + 1);
    for (int i = 0; i < displayCount; ++i) {
        const AuthorEdge& edge = m_allCoauthorEdges[i];
        const QString coauthorName = (edge.a1 == centerAuthor) ? edge.a2 : edge.a1;
        m_tempNodes.append({coauthorName, authorPaperCount(coauthorName)});
        m_tempEdges.append(edge);
    }
}

void MainWindow::clearAuthorDetail()
{
    if (authorDetailTable == nullptr) {
        return;
    }
    authorDetailTable->setRowCount(0);
}

// 查找两位作者的合作论文
std::vector<std::uint32_t> MainWindow::findCoauthoredPapers(const QString& author1, const QString& author2)
{
    auto ids1 = m_db.author_records_exact(toStdString(author1));
    auto ids2 = m_db.author_records_exact(toStdString(author2));
    std::sort(ids1.begin(), ids1.end());
    std::sort(ids2.begin(), ids2.end());
    std::vector<std::uint32_t> result;
    std::set_intersection(ids1.begin(), ids1.end(), ids2.begin(), ids2.end(), std::back_inserter(result));
    return result;
}

void MainWindow::showAuthorDetail(const QString& authorName)
{
    if (authorName.isEmpty()) {
        return;
    }

    const QString centerAuthor = m_tempNodes.isEmpty() ? QString() : m_tempNodes.first().name;

    if (centerAuthor.isEmpty() || authorName == centerAuthor) {
        // 选中中心作者：右侧表格显示基本信息
        if (authorDetailTable == nullptr) return;
        authorDetailTable->setColumnCount(2);
        authorDetailTable->setHorizontalHeaderLabels({"项目", "内容"});

        const int paperCount = authorPaperCount(authorName);

        const int coauthorCount = static_cast<int>(m_authorGraph.queryCoauthors(authorName.toStdString()).size());

        authorDetailTable->setRowCount(3);
        const QStringList names = {"作者姓名", "累计发文量", "直接合作者数量"};
        const QStringList values = {
            authorName,
            QString::number(paperCount),
            QString::number(coauthorCount)
        };
        for (int row = 0; row < names.size(); ++row) {
            auto *nameItem = new QTableWidgetItem(names[row]);
            auto *valueItem = new QTableWidgetItem(values[row]);
            nameItem->setTextAlignment(Qt::AlignCenter);
            valueItem->setTextAlignment(Qt::AlignCenter);
            authorDetailTable->setItem(row, 0, nameItem);
            authorDetailTable->setItem(row, 1, valueItem);
        }
    } else {
        // 选中合作者：弹出窗口展示合作论文详情（合作列表已移至主界面 tab）
        auto coauthoredIds = findCoauthoredPapers(centerAuthor, authorName);

        QDialog *detailDialog = new QDialog(this);
        detailDialog->setAttribute(Qt::WA_DeleteOnClose);
        detailDialog->setWindowTitle(QString("合作论文 - %1 & %2").arg(centerAuthor).arg(authorName));
        detailDialog->resize(950, 600);
        detailDialog->setMinimumSize(700, 400);

        QVBoxLayout *dialogLayout = new QVBoxLayout(detailDialog);
        dialogLayout->setContentsMargins(15, 15, 15, 10);

        QLabel *titleLabel = new QLabel(
            QString("<b>%1</b> 与 <b>%2</b> 共合作 <b>%3</b> 篇论文：")
                .arg(centerAuthor).arg(authorName).arg(coauthoredIds.size()));
        dialogLayout->addWidget(titleLabel);

        QTableWidget *coPaperTable = new QTableWidget;
        coPaperTable->setColumnCount(5);
        coPaperTable->setHorizontalHeaderLabels({"标题", "作者", "年份", "期刊", "链接"});
        coPaperTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        coPaperTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        coPaperTable->setAlternatingRowColors(true);
        coPaperTable->verticalHeader()->setVisible(false);
        coPaperTable->horizontalHeader()->setStretchLastSection(true);
        coPaperTable->setColumnWidth(0, 350);
        coPaperTable->setColumnWidth(1, 240);
        coPaperTable->setColumnWidth(2, 60);
        coPaperTable->setColumnWidth(3, 140);
        dialogLayout->addWidget(coPaperTable);

        int displayed = 0;
        for (const auto id : coauthoredIds) {
            if (displayed >= 200) break;
            if (auto article = m_db.read_article(id)) {
                PaperData paper = paperFromXmlValue(*article);
                int row = coPaperTable->rowCount();
                coPaperTable->insertRow(row);
                coPaperTable->setItem(row, 0, new QTableWidgetItem(paper.title));
                coPaperTable->setItem(row, 1, new QTableWidgetItem(paper.author));
                coPaperTable->setItem(row, 2, new QTableWidgetItem(paper.year));
                coPaperTable->setItem(row, 3, new QTableWidgetItem(paper.journal));
                auto *linkItem = new QTableWidgetItem(paper.eeLink.isEmpty() ? "-" : "可打开");
                linkItem->setData(Qt::UserRole, paper.eeLink);
                coPaperTable->setItem(row, 4, linkItem);
                ++displayed;
            }
        }

        connect(coPaperTable, &QTableWidget::cellDoubleClicked, this,
            [=](int row, int /*column*/) {
                auto *item = coPaperTable->item(row, 4);
                if (!item) return;
                QString link = item->data(Qt::UserRole).toString();
                if (!link.isEmpty()) {
                    QDesktopServices::openUrl(QUrl(link));
                }
            });

        QPushButton *closeBtn = new QPushButton("关闭");
        dialogLayout->addWidget(closeBtn);
        connect(closeBtn, &QPushButton::clicked, detailDialog, &QDialog::close);

        detailDialog->exec();
    }
}

// -----------------------------------------------------------------------------
// 更新作者合作列表 tab 的表格内容
// -----------------------------------------------------------------------------
void MainWindow::updateCoauthorListTable()
{
    if (!coauthorListTable) return;

    coauthorListTable->setUpdatesEnabled(false);
    coauthorListTable->clearSpans();
    coauthorListTable->setColumnCount(0);
    coauthorListTable->setColumnCount(3);
    coauthorListTable->setHorizontalHeaderLabels({"序号", "合作者姓名", "合作次数"});
    coauthorListTable->setRowCount(0);

    // 无数据时显示提示
    if (m_allCoauthorEdges.isEmpty()) {
        coauthorListTable->setRowCount(1);
        auto *tipItem = new QTableWidgetItem("请先搜索作者以查看合作列表");
        tipItem->setTextAlignment(Qt::AlignCenter);
        tipItem->setFlags(tipItem->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        coauthorListTable->setSpan(0, 0, 1, 3);
        coauthorListTable->setItem(0, 0, tipItem);
        coauthorListTable->setEnabled(false);
        coauthorListTable->setUpdatesEnabled(true);
        updateCoauthorPaginationControls();
        return;
    }

    coauthorListTable->setEnabled(true);

    // 获取中心作者名（第一个节点即为中心作者）
    QString centerAuthor = m_tempNodes.isEmpty() ? QString() : m_tempNodes.first().name;

    const int total = m_allCoauthorEdges.size();
    const int totalPages = qMax(1, (total + kCoauthorPageSize - 1) / kCoauthorPageSize);
    m_coauthorPage = qBound(0, m_coauthorPage, totalPages - 1);
    const int start = m_coauthorPage * kCoauthorPageSize;
    const int end = qMin(start + kCoauthorPageSize, total);
    const int rowCount = end - start;
    coauthorListTable->setRowCount(rowCount);

    for (int i = start; i < end; ++i) {
        const auto& edge = m_allCoauthorEdges[i];
        const QString coauthorName = (edge.a1 == centerAuthor) ? edge.a2 : edge.a1;
        const int row = i - start;
        coauthorListTable->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
        coauthorListTable->setItem(row, 1, new QTableWidgetItem(coauthorName));
        coauthorListTable->setItem(row, 2, new QTableWidgetItem(QString::number(edge.cooperateNum)));
        coauthorListTable->item(row, 0)->setTextAlignment(Qt::AlignCenter);
        coauthorListTable->item(row, 2)->setTextAlignment(Qt::AlignCenter);
    }
    coauthorListTable->setUpdatesEnabled(true);
    updateCoauthorPaginationControls();
}

void MainWindow::updateCoauthorPaginationControls()
{
    const int total = m_allCoauthorEdges.size();
    const int totalPages = total == 0 ? 0 : (total + kCoauthorPageSize - 1) / kCoauthorPageSize;
    if (coauthorPageLabel != nullptr) {
        coauthorPageLabel->setText(QString("第 %1 / %2 页，共 %3 位合作者")
            .arg(total == 0 ? 0 : m_coauthorPage + 1)
            .arg(totalPages)
            .arg(total));
    }
    if (coauthorPrevBtn != nullptr) {
        coauthorPrevBtn->setEnabled(total > 0 && m_coauthorPage > 0);
    }
    if (coauthorNextBtn != nullptr) {
        coauthorNextBtn->setEnabled(total > 0 && m_coauthorPage + 1 < totalPages);
    }
}

// -----------------------------------------------------------------------------
// 绘制作者合作关系图（中心辐射图）
// -----------------------------------------------------------------------------
void MainWindow::drawCooperationGraph()
{
    graphScene->clear(); // 清空画布
    clearAuthorDetail();
    updateCoauthorListTable(); // 更新作者合作列表 tab
    // 无数据 → 显示提示
    if (m_tempNodes.isEmpty()) {
        if (graphSummaryLabel != nullptr) {
            graphSummaryLabel->setText("未找到该作者的合作关系");
        }
        if (showAllGraphBtn != nullptr) {
            showAllGraphBtn->setEnabled(false);
            showAllGraphBtn->setText("显示全部图");
        }
        QGraphicsTextItem* tipText = graphScene->addText("未找到该作者的合作关系，请重新搜索");
        tipText->setPos(200, 200);
        tipText->setDefaultTextColor(Qt::black);
        return;
    }

    graphScene->setBackgroundBrush(Qt::white); // 白色背景

    const int totalCoauthors = m_allCoauthorEdges.size();
    const int displayedCoauthors = qMax(0, m_tempNodes.size() - 1);
    const bool canShowAllGraph =
        totalCoauthors > displayedCoauthors && totalCoauthors <= kMaxFullGraphCoauthors;
    if (graphSummaryLabel != nullptr) {
        const QString suffix = totalCoauthors > kMaxFullGraphCoauthors
            ? QString("，合作者过多，完整名单请在列表分页查看")
            : QString();
        graphSummaryLabel->setText(QString("共 %1 位合作者，当前图中显示 %2 位%3")
            .arg(totalCoauthors)
            .arg(displayedCoauthors)
            .arg(suffix));
    }
    if (showAllGraphBtn != nullptr) {
        showAllGraphBtn->setEnabled(canShowAllGraph);
        showAllGraphBtn->setText(totalCoauthors > kMaxFullGraphCoauthors ? "合作者过多" : "显示全部图");
    }

    QGraphicsTextItem *limitText =
        graphScene->addText(QString("共 %1 位合作者，当前图中显示 %2 位")
                            .arg(totalCoauthors)
                            .arg(displayedCoauthors));
    limitText->setDefaultTextColor(Qt::black);
    limitText->setPos(20, 120);

    QMap<QString, QPointF> pos;       // 存储每个作者的坐标
    QMap<QString, int> degree;         // 存储每个作者的合作次数

    // 统计每个作者的关联次数
    for (auto& e : m_tempEdges) {
        degree[e.a1]++;
        degree[e.a2]++;
    }

    // 按关联次数从大到小排序，保持目标作者在中心
    QVector<QPair<int, QString>> list;
    if (!m_tempNodes.isEmpty()) {
        list.append({std::numeric_limits<int>::max(), m_tempNodes.first().name});
    }
    for (int i = 1; i < m_tempNodes.size(); ++i) {
        const auto& node = m_tempNodes[i];
        list.append({degree.value(node.name, 0), node.name});
    }
    std::sort(list.begin(), list.end(), std::greater<>());

    // 中心坐标（居中偏上，给下方留更多空间）
    int centerX = 450;
    int centerY = 350;
    int coreCount = 1;

    // --------------------------
    // 绘制中心节点（目标作者，橙色）
    // --------------------------
    if (!list.isEmpty()) {
        QString targetAuthor = list[0].second;
        int x = centerX;
        int y = centerY;
        pos[targetAuthor] = QPointF(x, y);
        // 画圆形节点
        QGraphicsEllipseItem *nodeItem =
            graphScene->addEllipse(x - 18, y - 18, 36, 36, QPen(Qt::black, 2), QBrush(QColor(255, 165, 0)));
        nodeItem->setData(0, targetAuthor);
        nodeItem->setFlag(QGraphicsItem::ItemIsSelectable);
        nodeItem->setCursor(Qt::PointingHandCursor);
        // 在节点下方显示作者姓名
        QGraphicsTextItem *nameText = graphScene->addText(targetAuthor);
        nameText->setDefaultTextColor(Qt::darkBlue);
        nameText->setFont(QFont("Arial", 9, QFont::Bold));
        nameText->setPos(x - nameText->boundingRect().width() / 2, y + 22);
    }

    // --------------------------
    // 绘制外围合作者节点（蓝色）
    // 多环螺旋布局：根据节点数量自适应环数、半径和节点大小
    // --------------------------
    int ringCount = list.size() - coreCount;

    // 根据节点总数动态调整参数
    const int baseRadius    = 140;
    const int ringSpacing   = (ringCount > 200) ? 48 : (ringCount > 100) ? 52 : 58;
    const double nodeArc    = (ringCount > 200) ? 0.22 : (ringCount > 100) ? 0.25 : 0.30;
    const int nodesPerRing  = qMax(8, static_cast<int>(2.0 * M_PI / nodeArc));
    const int totalRings    = (ringCount + nodesPerRing - 1) / nodesPerRing;

    for (int i = 0; i < ringCount; i++) {
        QString name = list[coreCount + i].second;

        // 直接计算第几环和环内位置
        int ring = i / nodesPerRing;
        int posInRing = i % nodesPerRing;

        // 该环的实际节点数
        int nodesThisRing = nodesPerRing;
        if (ring == totalRings - 1) {
            int rem = ringCount - ring * nodesPerRing;
            if (rem > 0) nodesThisRing = rem;
        }

        // 角度：均匀分布，奇偶环交错偏移避免堆叠
        double angleOffset = (ring % 2 == 0) ? 0.0 : (M_PI / qMax(nodesThisRing, 1));
        double angle = angleOffset + (2.0 * M_PI * posInRing) / qMax(nodesThisRing, 1);

        // 半径
        int r = baseRadius + ring * ringSpacing;

        double xd = centerX + r * std::cos(angle);
        double yd = centerY + r * std::sin(angle);
        int x = static_cast<int>(std::round(xd));
        int y = static_cast<int>(std::round(yd));

        pos[name] = QPointF(x, y);

        // 节点大小随环数缩小
        int nodeRadius = qMax(6, 10 - ring / 2);
        int nodeSize = nodeRadius * 2;
        double opacity = (ring <= 6) ? 0.85 : qMax(0.35, 0.85 - (ring - 6) * 0.06);

        QGraphicsEllipseItem *nodeItem =
            graphScene->addEllipse(x - nodeRadius, y - nodeRadius, nodeSize, nodeSize,
                                   QPen(Qt::darkGray), QBrush(QColor(100, 200, 255, static_cast<int>(opacity * 255))));
        nodeItem->setData(0, name);
        nodeItem->setFlag(QGraphicsItem::ItemIsSelectable);
        nodeItem->setCursor(Qt::PointingHandCursor);

        // 外环文字逐渐缩小，超远端（>10环）不显示
        if (ring <= 10) {
            int fontSize = qMax(5, 9 - ring);
            QFont labelFont("Arial", fontSize);
            QGraphicsTextItem *nameText = graphScene->addText(name);
            nameText->setDefaultTextColor(Qt::black);
            nameText->setFont(labelFont);
            nameText->setPos(x + nodeRadius + 2, y - nodeRadius);
        }
    }

    // 根据实际内容动态调整场景大小（包围所有节点）
    int maxRadius = baseRadius + (totalRings - 1) * ringSpacing + 80;
    int sceneLeft   = centerX - maxRadius - 50;
    int sceneTop    = centerY - maxRadius - 100;
    int sceneWidth  = (centerX + maxRadius + 50) - sceneLeft;
    int sceneHeight = (centerY + maxRadius + 120) - sceneTop;
    graphScene->setSceneRect(sceneLeft, sceneTop, sceneWidth, sceneHeight);

    // 重置视图变换（缩放/平移），确保完整显示
    graphView->setTransform(QTransform());
    graphView->ensureVisible(graphScene->sceneRect());

    // --------------------------
    // 绘制合作边（按次数区分颜色）
    // --------------------------
    for (auto& e : m_tempEdges) {
        if (!pos.contains(e.a1) || !pos.contains(e.a2)) continue;

        QPointF p1 = pos[e.a1];
        QPointF p2 = pos[e.a2];

        QPen pen;
        // 合作≥4次：红色粗线
        if (e.cooperateNum >= 4) {
            pen.setColor(Qt::red);
            pen.setWidth(2);
        }
        // 合作2-3次：蓝色
        else if (e.cooperateNum >= 2) {
            pen.setColor(Qt::blue);
            pen.setWidth(1);
        }
        // 合作1次：灰色
        else {
            pen.setColor(Qt::lightGray);
            pen.setWidth(1);
        }
        graphScene->addLine(p1.x(), p1.y(), p2.x(), p2.y(), pen);
    }

    // --------------------------
    // 绘制图例说明
    // --------------------------
    graphScene->addRect(20, 20, 160, 90, QPen(Qt::black), QColor(255, 255, 255, 230));
    graphScene->addEllipse(30, 30, 15, 10, QPen(), Qt::red);
    QGraphicsTextItem *strongText = graphScene->addText("强关联 ≥4次");
    strongText->setDefaultTextColor(Qt::black);
    strongText->setPos(55, 25);
    graphScene->addEllipse(30, 50, 15, 10, QPen(), Qt::blue);
    QGraphicsTextItem *normalText = graphScene->addText("一般关联 2-3次");
    normalText->setDefaultTextColor(Qt::black);
    normalText->setPos(55, 45);
    graphScene->addEllipse(30, 70, 15, 10, QPen(), Qt::lightGray);
    QGraphicsTextItem *weakText = graphScene->addText("弱关联 1次");
    weakText->setDefaultTextColor(Qt::black);
    weakText->setPos(55, 65);

    if (!m_tempNodes.isEmpty()) {
        showAuthorDetail(m_tempNodes.first().name);
    }
}

// -----------------------------------------------------------------------------
// 绘制论文关键词统计柱状图
// -----------------------------------------------------------------------------
// 绘制论文关键词统计柱状图（根治切换残留bug 永久无左侧乱码）
// 绘制论文关键词统计柱状图（根治切换残留+零报错）
void MainWindow::drawGraphicsBarChart()
{
    if (!barChartPlot || m_db.size() == 0) return;

    barChartPlot->clearPlottables();
    barChartPlot->clearGraphs();
    barChartPlot->clearItems(); // 关键：清空所有旧文字，彻底解决切换残留

    const YearKeywordTop yearlyKeywords = m_stats.yearly_hot_keywords(m_db, 10);
    if (yearlyKeywords.empty()) return;

    std::string selectedYear;
    if (keywordYearCombo != nullptr && !keywordYearCombo->currentText().isEmpty()) {
        selectedYear = keywordYearCombo->currentText().toStdString();
    } else {
        auto latestYearIt = std::max_element(
            yearlyKeywords.begin(),
            yearlyKeywords.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
        selectedYear = latestYearIt->first;
    }

    auto selectedYearIt = yearlyKeywords.find(selectedYear);
    if (selectedYearIt == yearlyKeywords.end()) return;
    const std::vector<KeywordStat>& keywords = selectedYearIt->second;

    int showCount = qMin(10, static_cast<int>(keywords.size()));
    if (showCount == 0) return;

    // 3. 准备绘图数据
    QVector<double> xValues, yValues;
    QStringList labels;
    for (int i = 0; i < showCount; i++)
    {
        xValues.append(i + 1);
        yValues.append(static_cast<double>(keywords[static_cast<size_t>(i)].count));
        labels.append(QString::fromStdString(keywords[static_cast<size_t>(i)].keyword));
    }

    // 4. 创建柱状图
    QCPBars *bars = new QCPBars(barChartPlot->xAxis, barChartPlot->yAxis);
    bars->setData(xValues, yValues);
    bars->setBrush(QColor(0, 160, 230));
    bars->setPen(QPen(Qt::GlobalColor::black));

    // 5. 柱子上方显示数量
    for (int i = 0; i < showCount; i++)
    {
        QCPItemText *textLabel = new QCPItemText(barChartPlot);
        textLabel->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        textLabel->position->setCoords(xValues[i], yValues[i] + 2);
        textLabel->setText(QString::number(yValues[i]));
        textLabel->setFont(QFont("Arial", 9));
        textLabel->setColor(Qt::GlobalColor::black);
    }

    // 6. 坐标轴标签（修正笔误）
    barChartPlot->xAxis->setLabel(QString("关键词 (%1)").arg(QString::fromStdString(selectedYearIt->first)));
    barChartPlot->yAxis->setLabel("论文数量");

    // 底部关键词标签
    double maxY = *std::max_element(yValues.begin(), yValues.end());
    for (int i = 0; i < showCount; i++)
    {
        QCPItemText *kwText = new QCPItemText(barChartPlot);
        kwText->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
        kwText->position->setCoords(xValues[i], 0 - maxY * 0.12);
        kwText->setText(labels[i]);
        kwText->setFont(QFont("Arial", 8));
        kwText->setColor(Qt::GlobalColor::black);
        kwText->setRotation(30);
    }

    barChartPlot->xAxis->setRange(0, showCount + 1);
    barChartPlot->yAxis->setRange(0, maxY * 1.2);

    barChartPlot->replot();
}

void MainWindow::populateKeywordYearCombo()
{
    if (keywordYearCombo == nullptr || m_db.size() == 0) {
        return;
    }

    const QString previousYear = keywordYearCombo->currentText();
    keywordYearCombo->blockSignals(true);
    keywordYearCombo->clear();

    const YearKeywordTop yearlyKeywords = m_stats.yearly_hot_keywords(m_db, 10);
    std::vector<std::string> years;
    years.reserve(yearlyKeywords.size());
    for (const auto& item : yearlyKeywords) {
        if (!item.first.empty() && item.first != MISSING_STRING) {
            years.push_back(item.first);
        }
    }
    std::sort(years.begin(), years.end());

    for (const std::string& year : years) {
        keywordYearCombo->addItem(QString::fromStdString(year));
    }

    if (!previousYear.isEmpty()) {
        const int previousIndex = keywordYearCombo->findText(previousYear);
        if (previousIndex >= 0) {
            keywordYearCombo->setCurrentIndex(previousIndex);
        }
    } else if (keywordYearCombo->count() > 0) {
        keywordYearCombo->setCurrentIndex(keywordYearCombo->count() - 1);
    }

    keywordYearCombo->blockSignals(false);
}

// 绘制：发文量前100作者 横向降序排名柱状图（名字不重叠、美观清晰）
// 绘制：发文量前100作者 横向降序排名柱状图（兼容Qt CustomPlot 零报错）
// 发文量前100作者 纯净横向排名图
// 标准Qt横向排名柱状图 终极干净版 无任何错乱
// 纵向作者发文排名柱状图 零错乱 所有作者必有柱子
// --------------------------
// 显示作者发文量排名表格
// --------------------------
void MainWindow::showAuthorRankTable(bool desc)
{
    if (m_db.size() == 0 || authorTable == nullptr) return;

    std::vector<AuthorStat> authorRank = m_stats.top_authors(m_db, 100);

    if (!desc) {
        std::reverse(authorRank.begin(), authorRank.end());
    }

    authorTable->setUpdatesEnabled(false);
    authorTable->setSortingEnabled(false);
    authorTable->clearContents();
    const int showCount = static_cast<int>(authorRank.size());
    authorTable->setRowCount(showCount);

    for (int i = 0; i < showCount; i++) {
        auto *rankItem = new QTableWidgetItem(QString::number(i + 1));
        auto *authorItem = new QTableWidgetItem(QString::fromStdString(authorRank[static_cast<size_t>(i)].author));
        auto *countItem = new QTableWidgetItem(QString::number(authorRank[static_cast<size_t>(i)].paper_count));
        rankItem->setTextAlignment(Qt::AlignCenter);
        countItem->setTextAlignment(Qt::AlignCenter);
        authorTable->setItem(i, 0, rankItem);
        authorTable->setItem(i, 1, authorItem);
        authorTable->setItem(i, 2, countItem);
    }

    authorTable->horizontalHeader()->setStretchLastSection(true);
    authorTable->setColumnWidth(0, 60);    // 排名列
    authorTable->setColumnWidth(1, 420);   // 作者姓名列 大幅加宽
    authorTable->setColumnWidth(2, 120);   // 累计发文量列 缩小一点
    authorTable->setUpdatesEnabled(true);
}

void MainWindow::showCliqueStatistics()
{
    if (cliqueTable == nullptr) {
        return;
    }

    const std::vector<std::string> counts = m_authorGraph.countCliquesByOrder();
    if (counts.empty()) {
        cliqueTable->setRowCount(0);
        cliqueTable->insertRow(0);
        cliqueTable->setItem(0, 0, new QTableWidgetItem("-"));
        cliqueTable->setItem(0, 1, new QTableWidgetItem("缺少聚团统计，请重新运行 index_builder.exe 构建索引"));
        cliqueTable->item(0, 0)->setTextAlignment(Qt::AlignCenter);
        cliqueTable->item(0, 1)->setTextAlignment(Qt::AlignCenter);
        cliqueTable->resizeRowsToContents();
        return;
    }

    const size_t maxDisplayedCliqueOrder = std::min<size_t>(counts.size() - 1, 7);
    cliqueTable->setRowCount(0);
    cliqueTable->setUpdatesEnabled(false);

    for (size_t order = 1; order <= maxDisplayedCliqueOrder; ++order) {
        const int row = cliqueTable->rowCount();
        cliqueTable->insertRow(row);
        cliqueTable->setItem(row, 0, new QTableWidgetItem(QString::number(static_cast<qulonglong>(order))));
        const std::string count = counts[order];
        cliqueTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(count)));
        cliqueTable->item(row, 0)->setTextAlignment(Qt::AlignCenter);
        cliqueTable->item(row, 1)->setTextAlignment(Qt::AlignCenter);
    }
    cliqueTable->setUpdatesEnabled(true);

    cliqueTable->resizeRowsToContents();
}

// --------------------------
// 显示作者发文量排名表格
// --------------------------

// -----------------------------------------------------------------------------
// 点击搜索结果列表项：在状态栏显示提示
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// 点击搜索结果列表项：弹出论文详情 + 可点击链接
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// 点击搜索结果列表项：弹出论文详情 + 可点击链接
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// 点击搜索结果列表项：弹出论文详情 + 可点击链接
// -----------------------------------------------------------------------------
void MainWindow::onResultCellClick(int row, int)
{
    QTableWidgetItem *item = resultTable->item(row, 0);
    if (item == nullptr) {
        return;
    }

    const QString key = item->data(Qt::UserRole).toString();
    const XmlValue* article = m_db.find_by_key(key.toStdString());
    if (article == nullptr) {
        QMessageBox::warning(this, "论文详情", "无法找到该论文记录");
        return;
    }

    showPaperDetails(paperFromXmlValue(*article));
}

void MainWindow::showPaperDetails(const PaperData& paper)
{
    QString info = QString(
                       "标题：%1\n\n"
                       "作者：%2\n"
                       "年份：%3\n"
                       "期刊：%4\n"
                       "卷号：%5\n"
                       "月份：%6\n"
                       "链接：%7")
                       .arg(paper.title.isEmpty() ? "无" : paper.title)
                       .arg(paper.author.isEmpty() ? "无" : paper.author)
                       .arg(paper.year.isEmpty() ? "无" : paper.year)
                       .arg(paper.journal.isEmpty() ? "无" : paper.journal)
                       .arg(paper.volume.isEmpty() ? "无" : paper.volume)
                       .arg(paper.month.isEmpty() ? "无" : paper.month)
                       .arg(paper.eeLink.isEmpty() ? "无" : paper.eeLink);

    QMessageBox msgBox;
    msgBox.setWindowTitle("论文完整详细信息");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(info);

    QPushButton *openLinkBtn = msgBox.addButton("打开文献DOI网页", QMessageBox::ActionRole);

    connect(openLinkBtn, &QPushButton::clicked, this, [=](){
        if(!paper.eeLink.isEmpty())
        {
            QDesktopServices::openUrl(QUrl(paper.eeLink));
        }
        else
        {
            QMessageBox::information(this, "提示", "暂无可用文献链接");
        }
    });

    msgBox.addButton(QMessageBox::Ok);
    msgBox.exec();

    // 原有合作关系图逻辑不变
    if(!searchTargetAuthor.isEmpty())
    {
        filterAuthorData(searchTargetAuthor);
        drawCooperationGraph();
    }
}

void MainWindow::showAllCooperationGraph()
{
    const int totalCoauthors = m_allCoauthorEdges.size();
    if (totalCoauthors == 0 || m_tempNodes.isEmpty()) {
        return;
    }
    if (totalCoauthors > kMaxFullGraphCoauthors) {
        QMessageBox::information(this,
                                 "合作者过多",
                                 QString("该作者共有 %1 位合作者，图中最多支持显示 %2 位。请在作者合作列表中分页查看全部合作者。")
                                     .arg(totalCoauthors)
                                     .arg(kMaxFullGraphCoauthors));
        return;
    }
    rebuildGraphSubset(true);
    drawCooperationGraph();
}

// -----------------------------------------------------------------------------
// 事件过滤器：Ctrl+滚轮缩放关系图
// -----------------------------------------------------------------------------
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 只处理 graphView 视口的滚轮事件
    if (obj == graphView->viewport() && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent*>(event);
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            const double factor = (wheelEvent->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
            graphView->scale(factor, factor);
            return true; // 已消费，不传递
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// -----------------------------------------------------------------------------
// 双击合作列表查看合作论文详情
// -----------------------------------------------------------------------------
void MainWindow::onCoauthorListDoubleClick(int row, int /*column*/)
{
    const int edgeIndex = m_coauthorPage * kCoauthorPageSize + row;
    if (row < 0 || edgeIndex < 0 || edgeIndex >= m_allCoauthorEdges.size()) return;

    QString centerAuthor = m_tempNodes.isEmpty() ? QString() : m_tempNodes.first().name;
    if (centerAuthor.isEmpty()) return;

    const auto& edge = m_allCoauthorEdges[edgeIndex];
    const QString coauthorName = (edge.a1 == centerAuthor) ? edge.a2 : edge.a1;

    // 复用已有的 showAuthorDetail（会弹窗展示合作论文列表 + 双击打开链接）
    showAuthorDetail(coauthorName);
}

// -----------------------------------------------------------------------------
// 统一清空关系图
// -----------------------------------------------------------------------------
void MainWindow::clearGraphView()
{
    m_tempNodes.clear();
    m_tempEdges.clear();
    m_allCoauthorEdges.clear();
    m_graphShowsAllCoauthors = false;
    m_coauthorPage = 0;
    searchTargetAuthor.clear();
    graphScene->clear();
    graphScene->setSceneRect(0, 0, 1200, 800); // 恢复默认场景大小
    graphView->setTransform(QTransform());       // 重置缩放/平移
    clearAuthorDetail();
    updateCoauthorListTable();
    if (graphSummaryLabel != nullptr) {
        graphSummaryLabel->setText("请先搜索作者以查看合作关系");
    }
    if (showAllGraphBtn != nullptr) {
        showAllGraphBtn->setEnabled(false);
        showAllGraphBtn->setText("显示全部图");
    }

    QGraphicsTextItem* tipText = graphScene->addText("请输入你要查看合作关系的作者姓名");
    tipText->setPos(150, 250);
    tipText->setDefaultTextColor(Qt::gray);
    QFont tipFont = tipText->font();
    tipFont.setPointSize(14);
    tipText->setFont(tipFont);
}

//界面搭建：搜索框、按钮、结果列表、双标签页（关系图 + 统计图）
//XML 解析：读取论文、作者、年份、统计合作次数
//搜索功能：支持作者 / 标题 / 关键词多条件组合搜索
//关系图绘制：中心辐射式作者合作关系图，按合作次数着色
//统计图表：论文标题高频关键词 TOP10 柱状图
//交互体验：清空按钮、防重复点击、点击提示、错误弹窗
