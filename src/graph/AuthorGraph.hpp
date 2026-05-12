#ifndef AUTHOR_GRAPH_HPP
#define AUTHOR_GRAPH_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class Database;

class AuthorGraph {
private:
    // 无向带权图 (Weighted Undirected Graph)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> adjacencyList;

    // 【新增】F6高阶：使用 Pivot (轴点) 优化的 Bron-Kerbosch 算法
    void bronKerboschPivot(std::unordered_set<std::string>& R,
        std::unordered_set<std::string>& P,
        std::unordered_set<std::string>& X,
        std::vector<std::vector<std::string>>& cliques) const;

public:
    AuthorGraph() = default;
    ~AuthorGraph() = default;

    // 构建全局图结构
    void buildGraph(const Database& db);

    // 内部辅助：添加一条带权无向边
    void addEdge(const std::string& author1, const std::string& author2);

    // 查询合作作者
    std::vector<std::pair<std::string, int>> queryCoauthors(const std::string& author) const;
    void showRelevantSearch(const std::string& author_name) const;

    // 广度优先搜索 (BFS)
    std::vector<std::string> findShortestPath(const std::string& start_author, const std::string& end_author) const;

    // 导出指定作者的关系网络为 JSON
    bool exportAuthorNetworkJSON(const std::string& center_author, const std::string& filepath) const;

    // F6: 全局极大聚团识别 (非常耗时)
    std::vector<std::vector<std::string>> findCliques() const;
    void printCliqueStatistics() const;

    // 【新增】F6高阶：局部学术圈聚团识别 (瞬间完成)
    std::vector<std::vector<std::string>> findLocalCliques(const std::string& center_author) const;
    void printLocalCliqueStatistics(const std::string& center_author) const;
};

#endif // AUTHOR_GRAPH_HPP
