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
