#ifndef AUTHOR_GRAPH_HPP
#define AUTHOR_GRAPH_HPP

#include <string>
#include <vector>
#include <unordered_map>

class Database;

class AuthorGraph {
private:
    // 无向带权图 (Weighted Undirected Graph)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> adjacencyList;

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

};

#endif // AUTHOR_GRAPH_HPP
