#include "AuthorGraph.hpp"
#include "common/database.hpp"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <fstream>
#include <map>

namespace {
bool isValidAuthor(const std::string& author) {
    return author != MISSING_STRING;
}

std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (char ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        switch (ch) {
        case '\"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (uch < 0x20) {
                static constexpr char hex[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped += hex[uch >> 4];
                escaped += hex[uch & 0x0f];
            }
            else {
                escaped += ch;
            }
            break;
        }
    }

    return escaped;
}
}

// ==========================================
// 1. 图的构建与边管理
// ==========================================
void AuthorGraph::buildGraph(const Database& db) {
    adjacencyList.clear();
    const auto& records = db.all();

    for (const auto& article : records) {
        std::vector<std::string> valid_authors;
        valid_authors.reserve(article.author_count());

        for (size_t i = 0; i < article.author_count(); ++i) {
            const std::string& author = article.author_at(i);
            if (isValidAuthor(author)) {
                valid_authors.push_back(author);
            }
        }

        // 两两建边
        for (size_t i = 0; i < valid_authors.size(); ++i) {
            for (size_t j = i + 1; j < valid_authors.size(); ++j) {
                addEdge(valid_authors[i], valid_authors[j]);
            }
        }
    }
}

void AuthorGraph::addEdge(const std::string& author1, const std::string& author2) {
    if (author1 == author2) return; // 避免自己和自己合作(自环)
    // 【升级 1 核心】每次合作，边权重 +1
    adjacencyList[author1][author2]++;
    adjacencyList[author2][author1]++;
}

// ==========================================
// 2. F2 进阶：寻找黄金搭档 (带权排序)
// ==========================================
std::vector<std::pair<std::string, int>> AuthorGraph::queryCoauthors(const std::string& author) const {
    std::vector<std::pair<std::string, int>> coauthors;
    auto it = adjacencyList.find(author);

    if (it != adjacencyList.end()) {
        // 提取 map 中的所有键值对到 vector 中
        for (const auto& pair : it->second) {
            coauthors.push_back(pair);
        }
        // 【算法应用】使用 Lambda 表达式按 value(合作次数) 降序排序
        std::sort(coauthors.begin(), coauthors.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
    }
    return coauthors;
}

// ==========================================
// 3. F2 扩展：BFS 寻找最短合作路径 (埃尔德什数原理)
// ==========================================
std::vector<std::string> AuthorGraph::findShortestPath(const std::string& start, const std::string& end) const {
    // 边界检查：如果起点或终点不在图里，直接返回空
    if (adjacencyList.find(start) == adjacencyList.end() || adjacencyList.find(end) == adjacencyList.end()) {
        return {};
    }
    if (start == end) return { start }; // 同一个人

    // 追踪路径的哈希表：Key 是当前节点，Value 是它的“父节点”(从哪里走过来的)
    std::unordered_map<std::string, std::string> parent;
    std::queue<std::string> q; // BFS 的核心队列

    q.push(start);
    parent[start] = ""; // 起点没有父节点，设为空字符串

    bool found = false;
    while (!q.empty()) {
        std::string current = q.front();
        q.pop();

        // 找到终点，提前结束搜索
        if (current == end) {
            found = true;
            break;
        }

        // 遍历当前节点的所有邻居 (合作者)
        for (const auto& neighbor_pair : adjacencyList.at(current)) {
            const std::string& neighbor = neighbor_pair.first;

            // 如果该邻居没被访问过 (不在 parent map 中)
            if (parent.find(neighbor) == parent.end()) {
                parent[neighbor] = current; // 记录是从 current 走到 neighbor 的
                q.push(neighbor);           // 将邻居推入队列等待下一层扩散
            }
        }
    }

    if (!found) return {}; // 如果队列空了还没找到，说明两人处于不连通的子图中

    // 【算法难点】路径回溯：从终点一步步反向推回起点
    std::vector<std::string> path;
    std::string curr = end;
    while (curr != "") {
        path.push_back(curr);
        curr = parent[curr];
    }
    // 因为是从后往前推的，需要将整个数组反转
    std::reverse(path.begin(), path.end());

    return path; // 返回顺序: Start -> 节点A -> 节点B -> End
}

// ==========================================
// 4. 架构协同：导出标准 JSON 供 F7 (前端/可视化) 使用
// ==========================================
bool AuthorGraph::exportAuthorNetworkJSON(const std::string& center_author, const std::string& filepath) const {
    auto it = adjacencyList.find(center_author);
    if (it == adjacencyList.end()) return false;

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    const std::string escaped_center = escapeJsonString(center_author);

    // 按照 D3.js / ECharts 的标准格式构造 JSON
    out << "{\n  \"nodes\": [\n";

    // 写入中心节点 (category 0 表示核心)
    out << "    {\"id\": \"" << escaped_center << "\", \"category\": 0, \"value\": 20}";

    // 写入周边合作者节点 (category 1 表示合作者，value 设为合作次数)
    for (const auto& pair : it->second) {
        out << ",\n    {\"id\": \"" << escapeJsonString(pair.first) << "\", \"category\": 1, \"value\": " << pair.second << "}";
    }

    out << "\n  ],\n  \"links\": [\n";

    // 写入连线 (边)，权重 weight 为合作次数
    bool first_edge = true;
    for (const auto& pair : it->second) {
        if (!first_edge) out << ",\n";
        out << "    {\"source\": \"" << escaped_center << "\", \"target\": \"" << escapeJsonString(pair.first)
            << "\", \"weight\": " << pair.second << "}";
        first_edge = false;
    }
    out << "\n  ]\n}\n";

    out.close();
    return true;
}

// ==========================================
// 5. F6 聚团分析 (Bron-Kerbosch，需适配带权图的 Map 结构)
// ==========================================

void AuthorGraph::printCliqueStatistics() const {
    std::vector<std::vector<std::string>> cliques = findCliques();
    std::unordered_map<size_t, int> cliqueSizeCount;
    for (const auto& clique : cliques) {
        cliqueSizeCount[clique.size()]++;
    }
    std::cout << "\n========== F6 极大聚团统计 ==========\n";
    if (cliqueSizeCount.empty()) {
        std::cout << "当前图谱中未发现有效的合作聚团。\n";
        return;
    }
    std::vector<std::pair<size_t, int>> sortedStats(cliqueSizeCount.begin(), cliqueSizeCount.end());
    std::sort(sortedStats.begin(), sortedStats.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    std::cout << std::left << std::setw(15) << "聚团阶数(人数)" << std::right << std::setw(15) << "聚团个数" << '\n';
    std::cout << std::string(30, '-') << '\n';
    for (const auto& pair : sortedStats) {
        std::cout << std::left << std::setw(15) << pair.first << std::right << std::setw(15) << pair.second << '\n';
    }
    std::cout << "------------------------------\n";
    std::cout << "总计极大聚团数量: " << cliques.size() << '\n';
}

void AuthorGraph::showRelevantSearch(const std::string& author_name) const {
    auto it = adjacencyList.find(author_name);
    if (it == adjacencyList.end()) {
        std::cout << "[F2] 未找到作者: " << author_name << " 的相关信息。\n";
        return;
    }

    // 1. 获取直接合作者并排序 (黄金搭档)
    auto direct = queryCoauthors(author_name);

    std::cout << "\n>>>> F2 相关搜索结果: " << author_name << " <<<<\n";
    std::cout << "[核心合作圈]\n";
    std::cout << std::left << std::setw(30) << "学者姓名" << "合作频率\n";
    std::cout << std::string(45, '-') << "\n";

    for (size_t i = 0; i < std::min<size_t>(direct.size(), 10); ++i) {
        std::cout << std::left << std::setw(30) << direct[i].first
            << "★ " << direct[i].second << "\n";
    }

    // 2. 二阶推荐 (寻找“朋友的朋友”)
    // 逻辑：如果 A-B 合作，B-C 合作，但 A-C 没合作过，那么 C 是 A 的潜在相关学者
    std::map<std::string, int> recommendations;
    const auto& my_friends = it->second;

    for (const auto& friend_pair : my_friends) {
        const std::string& friend_name = friend_pair.first;
        if (adjacencyList.count(friend_name)) {
            for (const auto& fof_pair : adjacencyList.at(friend_name)) {
                const std::string& fof_name = fof_pair.first;
                // 排除自己和已有的直接合作者
                if (fof_name != author_name && !my_friends.count(fof_name)) {
                    recommendations[fof_name]++;
                }
            }
        }
    }

    if (!recommendations.empty()) {
        std::cout << "\n[你可能感兴趣的学者 (潜在关联)]\n";
        std::vector<std::pair<std::string, int>> sorted_rec(recommendations.begin(), recommendations.end());
        std::sort(sorted_rec.begin(), sorted_rec.end(), [](auto& a, auto& b) { return a.second > b.second; });

        for (size_t i = 0; i < std::min<size_t>(sorted_rec.size(), 5); ++i) {
            std::cout << " 💡 " << sorted_rec[i].first
                << " (通过 " << sorted_rec[i].second << " 位共同好友关联)\n";
        }
    }
}
// ==========================================
// F6 高阶算法：带 Pivot (轴点) 优化的 Bron-Kerbosch
// 复杂度大幅度降低，有效应对稀疏大图
// ==========================================
void AuthorGraph::bronKerboschPivot(std::unordered_set<std::string>& R,
    std::unordered_set<std::string>& P,
    std::unordered_set<std::string>& X,
    std::vector<std::vector<std::string>>& cliques) const {
    if (P.empty() && X.empty()) {
        if (R.size() > 2) { // 学术聚团通常认为 >= 3 人才有分析意义，可大幅过滤无用数据
            cliques.push_back(std::vector<std::string>(R.begin(), R.end()));
        }
        return;
    }

    // 1. 选择 Pivot (轴点 u)：从 P U X 中选择在 P 中拥有最多邻居的节点
    std::string pivot = "";
    size_t max_degree_in_P = 0;

    auto check_pivot = [&](const std::string& node) {
        size_t degree = 0;
        if (adjacencyList.count(node)) {
            for (const auto& neighbor_pair : adjacencyList.at(node)) {
                if (P.count(neighbor_pair.first)) degree++;
            }
        }
        if (pivot.empty() || degree > max_degree_in_P) {
            pivot = node;
            max_degree_in_P = degree;
        }
        };
    for (const auto& v : P) check_pivot(v);
    for (const auto& v : X) check_pivot(v);

    // 2. 核心优化：只遍历 P 中不属于 pivot 邻居的节点 ( P \ N(u) )
    std::unordered_set<std::string> P_minus_Nu;
    const auto pivot_it = adjacencyList.find(pivot);

    for (const auto& v : P) {
        // 如果 v 不是 pivot 的合作者
        if (pivot_it == adjacencyList.end() || pivot_it->second.count(v) == 0) {
            P_minus_Nu.insert(v);
        }
    }

    // 3. 递归下降
    for (const auto& v : P_minus_Nu) {
        R.insert(v);
        std::unordered_set<std::string> new_P;
        std::unordered_set<std::string> new_X;

        auto it = adjacencyList.find(v);
        if (it != adjacencyList.end()) {
            for (const auto& neighbor_pair : it->second) {
                const std::string& neighbor = neighbor_pair.first;
                if (P.count(neighbor)) new_P.insert(neighbor);
                if (X.count(neighbor)) new_X.insert(neighbor);
            }
        }
        bronKerboschPivot(R, new_P, new_X, cliques);

        R.erase(v);
        P.erase(v);
        X.insert(v);
    }
}

// ==========================================
// F6 业务降维：只在指定作者的二阶关系网内找聚团
// ==========================================
std::vector<std::vector<std::string>> AuthorGraph::findLocalCliques(const std::string& center_author) const {
    std::vector<std::vector<std::string>> cliques;
    if (adjacencyList.find(center_author) == adjacencyList.end()) return cliques;

    std::unordered_set<std::string> R, P, X;

    // 构建局部图：把核心作者和他的所有直接合作者放入候选集 P
    P.insert(center_author);
    for (const auto& pair : adjacencyList.at(center_author)) {
        P.insert(pair.first); // 把所有一度合作者拉入群聊
    }

    // 在这个几百人的小圈子里算聚团，耗时接近 0 毫秒！
    bronKerboschPivot(R, P, X, cliques);
    return cliques;
}

void AuthorGraph::printLocalCliqueStatistics(const std::string& center_author) const {
    std::cout << "\n========== F6 局部聚团分析：[" << center_author << "] 的核心学术圈 ==========\n";
    std::vector<std::vector<std::string>> cliques = findLocalCliques(center_author);

    if (cliques.empty()) {
        std::cout << "该学者的关系网中没有发现 3 人以上的学术聚团。\n";
        return;
    }

    // 按聚团大小降序排列
    std::sort(cliques.begin(), cliques.end(), [](const auto& a, const auto& b) {
        return a.size() > b.size();
        });

    std::cout << "[INFO] 在其关系网中发现了 " << cliques.size() << " 个核心学术圈。\n\n";

    // 只打印最大的 5 个聚团，避免刷屏
    size_t display_limit = std::min<size_t>(cliques.size(), 5);
    for (size_t i = 0; i < display_limit; ++i) {
        std::cout << "【" << cliques[i].size() << "人核心群】: ";
        for (size_t j = 0; j < cliques[i].size(); ++j) {
            std::cout << cliques[i][j] << (j == cliques[i].size() - 1 ? "" : " | ");
        }
        std::cout << "\n";
    }
}
// ==========================================
// F6: 全局极大聚团识别 (极致优化版)
// 引入 K-Core 剪枝 与 顶点度数排序 (Degree Ordering)
// ==========================================
// ==========================================
// F6: 全局极大聚团识别 (OpenMP 多线程极致优化版)
// ==========================================
std::vector<std::vector<std::string>> AuthorGraph::findCliques() const {
    std::vector<std::vector<std::string>> global_cliques;

    // 1. K-Core 剪枝 与 提取
    std::vector<std::pair<std::string, int>> ordered_nodes;
    ordered_nodes.reserve(adjacencyList.size());
    for (const auto& pair : adjacencyList) {
        if (pair.second.size() >= 2) {
            ordered_nodes.push_back({ pair.first, static_cast<int>(pair.second.size()) });
        }
    }

    // 2. 核心优化：按度数升序排序 (简易退化序)
    std::sort(ordered_nodes.begin(), ordered_nodes.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    const int node_count = static_cast<int>(ordered_nodes.size());

    // 【并行解耦关键】：记录每个节点在排序后的位置索引
    std::unordered_map<std::string, int> node_order;
    for (int i = 0; i < node_count; ++i) {
        node_order[ordered_nodes[static_cast<size_t>(i)].first] = i;
    }

    // 3. 多线程并行计算开启
    // 告诉编译器：以下代码由 CPU 的所有核心并发执行
#ifdef _OPENMP
#pragma omp parallel
#endif
    {
        // 每个线程拥有一个局部的 cliques 容器，避免多线程争抢写入导致崩溃
        std::vector<std::vector<std::string>> local_cliques;

        // schedule(dynamic) 表示动态分配任务，哪个核心算得快，就多分配点任务
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
        for (int i = 0; i < node_count; ++i) {
            const std::string& v = ordered_nodes[static_cast<size_t>(i)].first;

            std::unordered_set<std::string> R = { v };
            std::unordered_set<std::string> new_P;
            std::unordered_set<std::string> new_X;

            // 独立构建 P 和 X，不依赖全局变量
            for (const auto& neighbor_pair : adjacencyList.at(v)) {
                const std::string& neighbor = neighbor_pair.first;
                auto it = node_order.find(neighbor);
                if (it != node_order.end()) {
                    if (it->second > i) {
                        // 排名在当前节点之后的邻居，放入 P (候选)
                        new_P.insert(neighbor);
                    }
                    else if (it->second < i) {
                        // 排名在当前节点之前的邻居，放入 X (排除，防止找回重复的聚团)
                        new_X.insert(neighbor);
                    }
                }
            }

            // 各个线程调用递归算法，操作的都是栈内存里的局部变量，绝对安全
            bronKerboschPivot(R, new_P, new_X, local_cliques);
        }

        // 【加锁规约】：等线程各自算完后，排队把自己的局部结果塞进全局大数组
#ifdef _OPENMP
#pragma omp critical
#endif
        {
            global_cliques.insert(global_cliques.end(), local_cliques.begin(), local_cliques.end());
        }
    }

    return global_cliques;
}
