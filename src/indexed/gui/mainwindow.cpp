// 引入主窗口头文件，包含类定义、控件、结构体声明
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
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
    authorInput->setPlaceholderText("输入作者名搜索");
    QPushButton *clearAuthorBtn = new QPushButton("清空");
    authorSearchLayout->addWidget(authorLabel);
    authorSearchLayout->addWidget(authorInput);
    authorSearchLayout->addWidget(clearAuthorBtn);
    connect(clearAuthorBtn, &QPushButton::clicked, this, [=](){
        authorInput->clear();
        resultTable->setRowCount(0);
        graphScene->clear();   // 清空合作关系图
        searchTargetAuthor.clear();
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
        graphScene->clear();   // 清空合作关系图
        searchTargetAuthor.clear();
    });

    // 1.3 关键词搜索行
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
        graphScene->clear();   // 清空合作关系图
        searchTargetAuthor.clear();
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
        graphScene->clear();   // 清空合作关系图
        searchTargetAuthor.clear();
    });

    // 卷号搜索行
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
        graphScene->clear();   // 清空合作关系图
        searchTargetAuthor.clear();
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
        graphScene->clear();   // 清空合作关系图
        searchTargetAuthor.clear();
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
    connect(authorInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
            graphScene->clear();   // 清空合作关系图
            searchTargetAuthor.clear();
        }
    });
    connect(titleInput, &QLineEdit::textChanged, this, [=](){
        if(authorInput->text().trimmed().isEmpty() &&
            titleInput->text().trimmed().isEmpty() &&
            keywordInput->text().trimmed().isEmpty() &&
            journalInput->text().trimmed().isEmpty() &&
            volumeInput->text().trimmed().isEmpty() &&
            yearInput->text().trimmed().isEmpty())
        {
            resultTable->setRowCount(0);
            graphScene->clear();   // 清空合作关系图
            searchTargetAuthor.clear();
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
            graphScene->clear();   // 清空合作关系图
            searchTargetAuthor.clear();
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
            graphScene->clear();   // 清空合作关系图
            searchTargetAuthor.clear();
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
            graphScene->clear();   // 清空合作关系图
            searchTargetAuthor.clear();
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
            graphScene->clear();   // 清空合作关系图
            searchTargetAuthor.clear();
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

    // 下方双标签
    searchBottomTab = new QTabWidget;
    searchBottomTab->addTab(resultTable, "文献搜索结果");
    searchBottomTab->addTab(graphView, "作者合作关系图");
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
    QHBoxLayout *cliqueButtonLayout = new QHBoxLayout;
    cliqueAnalyzeBtn = new QPushButton("统计各阶完全子图");
    cliqueButtonLayout->addWidget(cliqueAnalyzeBtn);
    cliqueButtonLayout->addStretch();
    cliqueLayout->addLayout(cliqueButtonLayout);

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
    connect(cliqueAnalyzeBtn, &QPushButton::clicked, this, &MainWindow::onCliqueAnalyzeClick);

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

    // 加载索引
#ifdef LITERATURE_DATA_DIR
    loadDblpXml(QStringLiteral(LITERATURE_DATA_DIR) + "/data/index");
#else
    loadDblpXml(QCoreApplication::applicationDirPath() + "/data/index");
#endif
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
        applyRecordIds(m_db.author_records_contains(toStdString(authorKey)));
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
        return;
    }

    // ===================== 只有作者有内容，才画关系图；作者为空 → 清空图 =====================
    if(searchTargetAuthor.isEmpty())
    {
        graphScene->clear(); // 没搜作者，关系图清空空白
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

    const std::filesystem::path indexPath = filePath.toStdString();
    if (!indexed::Database::has_index(indexPath) || !m_db.open(indexPath)) {
        QMessageBox::warning(this, "索引缺失", "未检测到索引，请先运行 index_builder.exe 构建索引。");
        return;
    }

    m_search = std::make_unique<indexed::SearchEngine>(&m_db);
    m_authorGraph.buildGraph(m_db);

    const auto authorCounts = m_db.author_paper_counts();
    m_nodes.reserve(static_cast<int>(authorCounts.size()));
    for (const auto& item : authorCounts) {
        m_nodes.append({QString::fromStdString(item.first), static_cast<int>(item.second)});
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
}

// -----------------------------------------------------------------------------
// 筛选目标作者的关联数据（只保留该作者 + 合作者）
// -----------------------------------------------------------------------------
void MainWindow::filterAuthorData(const QString &targetAuthor)
{
    m_tempNodes.clear(); // 清空临时节点
    m_tempEdges.clear(); // 清空临时边

    QString canonicalAuthor;
    int targetPaperCount = 0;
    for (const auto& node : m_nodes) {
        if (node.name.toLower() == targetAuthor.toLower()) {
            canonicalAuthor = node.name;
            targetPaperCount = node.paperCount;
            break;
        }
    }

    if (canonicalAuthor.isEmpty()) {
        for (const auto& node : m_nodes) {
            if (node.name.contains(targetAuthor, Qt::CaseInsensitive)) {
                canonicalAuthor = node.name;
                targetPaperCount = node.paperCount;
                break;
            }
        }
    }

    if (canonicalAuthor.isEmpty()) return; // 没找到作者 → 直接返回

    m_tempNodes.append({canonicalAuthor, targetPaperCount});

    const std::vector<std::pair<std::string, int>> coauthors =
        m_authorGraph.queryCoauthors(canonicalAuthor.toStdString());

    constexpr int maxDisplayedCoauthors = 30;
    const int displayCount = qMin(maxDisplayedCoauthors, static_cast<int>(coauthors.size()));
    for (int i = 0; i < displayCount; ++i) {
        const auto& coauthor = coauthors[static_cast<size_t>(i)];
        const QString name = QString::fromStdString(coauthor.first);
        int paperCount = 0;
        for (const auto& node : m_nodes) {
            if (node.name == name) {
                paperCount = node.paperCount;
                break;
            }
        }
        m_tempNodes.append({name, paperCount});
        m_tempEdges.append({canonicalAuthor, name, coauthor.second});
    }
}

// -----------------------------------------------------------------------------
// 绘制作者合作关系图（中心辐射图）
// -----------------------------------------------------------------------------
void MainWindow::drawCooperationGraph()
{
    graphScene->clear(); // 清空画布
    // 无数据 → 显示提示
    if (m_tempNodes.isEmpty()) {
        QGraphicsTextItem* tipText = graphScene->addText("未找到该作者的合作关系，请重新搜索");
        tipText->setPos(200, 200);
        tipText->setDefaultTextColor(Qt::gray);
        return;
    }

    graphScene->setBackgroundBrush(Qt::white); // 白色背景

    graphScene->addText(QString("当前仅显示合作次数最高的前 %1 位合作者").arg(qMax(0, m_tempNodes.size() - 1)))
        ->setPos(20, 120);

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

    // 中心坐标
    int centerX = 450;
    int centerY = 300;
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
        graphScene->addEllipse(x - 18, y - 18, 36, 36, QPen(Qt::black, 2), QBrush(QColor(255, 165, 0)));
        // 画作者名字
        QGraphicsTextItem *text = graphScene->addText(targetAuthor);
        text->setPos(x - text->boundingRect().width()/2, y + 22);
    }

    // --------------------------
    // 绘制外围合作者节点（蓝色）
    // --------------------------
    int ringCount = list.size() - coreCount;
    for (int i = 0; i < ringCount; i++) {
        QString name = list[coreCount + i].second;
        double angle = (2.0 * M_PI * i) / ringCount; // 均匀分布角度
        int r = 150 + (i % 3) * 30; // 半径

        // 计算坐标
        double xd = centerX + r * std::cos(angle);
        double yd = centerY + r * std::sin(angle);
        int x = static_cast<int>(std::round(xd));
        int y = static_cast<int>(std::round(yd));

        pos[name] = QPointF(x, y);

        // 画合作者节点
        graphScene->addEllipse(x - 10, y - 10, 20, 20, QPen(Qt::darkGray), QBrush(QColor(100, 200, 255, 180)));
        QGraphicsTextItem *text = graphScene->addText(name);
        text->setPos(x - text->boundingRect().width()/2, y + 15);
    }

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
    graphScene->addText("强关联 ≥4次")->setPos(55, 25);
    graphScene->addEllipse(30, 50, 15, 10, QPen(), Qt::blue);
    graphScene->addText("一般关联 2-3次")->setPos(55, 45);
    graphScene->addEllipse(30, 70, 15, 10, QPen(), Qt::lightGray);
    graphScene->addText("弱关联 1次")->setPos(55, 65);
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
    if (m_db.size() == 0) return;

    std::vector<AuthorStat> authorRank = m_stats.top_authors(m_db, 100);

    if (!desc) {
        std::reverse(authorRank.begin(), authorRank.end());
    }

    // 2. 清空表格
    authorTable->setRowCount(0);
    int showCount = static_cast<int>(authorRank.size());

    // 3. 填充表格
    for (int i = 0; i < showCount; i++) {
        int row = authorTable->rowCount();
        authorTable->insertRow(row);

        // 排名
        authorTable->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
        // 作者名
        authorTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(authorRank[static_cast<size_t>(i)].author)));
        // 发文量
        authorTable->setItem(row, 2, new QTableWidgetItem(QString::number(authorRank[static_cast<size_t>(i)].paper_count)));

        // 居中
        authorTable->item(row, 0)->setTextAlignment(Qt::AlignCenter);
        authorTable->item(row, 2)->setTextAlignment(Qt::AlignCenter);
    }

    // 列宽自适应
    authorTable->horizontalHeader()->setStretchLastSection(true);
    // 设置表格列宽：作者姓名列加宽，发文量列收窄
    authorTable->setColumnWidth(0, 60);    // 排名列
    authorTable->setColumnWidth(1, 420);   // 作者姓名列 大幅加宽
    authorTable->setColumnWidth(2, 120);   // 累计发文量列 缩小一点
}

void MainWindow::onCliqueAnalyzeClick()
{
    showCliqueStatistics();
}

void MainWindow::showCliqueStatistics()
{
    if (cliqueTable == nullptr || cliqueAnalyzeBtn == nullptr) {
        return;
    }

    cliqueAnalyzeBtn->setEnabled(false);
    cliqueAnalyzeBtn->setText("统计中...");
    cliqueTable->setRowCount(0);

    const std::vector<std::uint64_t> counts = m_authorGraph.countCliquesByOrder();

    for (size_t order = 1; order < counts.size(); ++order) {
        if (counts[order] == 0) {
            continue;
        }
        const int row = cliqueTable->rowCount();
        cliqueTable->insertRow(row);
        cliqueTable->setItem(row, 0, new QTableWidgetItem(QString::number(static_cast<qulonglong>(order))));
        cliqueTable->setItem(row, 1, new QTableWidgetItem(QString::number(static_cast<qulonglong>(counts[order]))));
        cliqueTable->item(row, 0)->setTextAlignment(Qt::AlignCenter);
        cliqueTable->item(row, 1)->setTextAlignment(Qt::AlignCenter);
    }

    cliqueTable->resizeRowsToContents();
    cliqueAnalyzeBtn->setText("统计各阶完全子图");
    cliqueAnalyzeBtn->setEnabled(true);
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
//核心功能总结
//界面搭建：搜索框、按钮、结果列表、双标签页（关系图 + 统计图）
//XML 解析：读取论文、作者、年份、统计合作次数
//搜索功能：支持作者 / 标题 / 关键词多条件组合搜索
//关系图绘制：中心辐射式作者合作关系图，按合作次数着色
//统计图表：论文标题高频关键词 TOP10 柱状图
//交互体验：清空按钮、防重复点击、点击提示、错误弹窗
