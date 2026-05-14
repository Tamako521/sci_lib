#if 0
#include "common/database.hpp"
#include "analysis/statistics_analyzer.hpp"
#include "common/parse_result.hpp"
#include "graph/AuthorGraph.hpp"  // 引入图算法模块
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// 综合演示：包含 F3、F4 (组员) 以及全面升级的 F2、F6 (你负责的部分)
static int run_system_demo() {
    Database db;
    std::cout << "[INFO] 正在加载文献数据库...\n";
    // 如果跑真实数据，请修改为实际的完整路径
    if (db.load("D:\\dblp.xml") != ParseResult::OK) {
        throw std::runtime_error("数据加载出现错误，请检查 XML 文件或路径！");
    }
    std::cout << "[INFO] 数据库加载完成，共载入 " << db.size() << " 条记录。\n";

    // =========================================================
    // 第二部分： (F2 合作图与路径搜索 & F6 聚团分析)
    // =========================================================
    AuthorGraph graph;
    std::cout << "\n========== F2 & F6: 构建无向带权作者合作图 ==========\n";
    graph.buildGraph(db);
    std::cout << "[INFO] 合作图构建完毕，已计算所有合作权重！\n";

    // 1. 预设测试用例：静态演示
    std::string start_author = "Jiawei Han";
    std::string target_author = "Philip S. Yu";

    std::cout << "\n========== F2 静态测试：寻找黄金搭档 (带权重) ==========\n";
    // 修复：重命名为 test_coauthors 避免冲突
    std::vector<std::pair<std::string, int>> test_coauthors = graph.queryCoauthors(target_author);

    size_t display_limit = std::min<size_t>(test_coauthors.size(), 15);
    for (size_t i = 0; i < display_limit; ++i) {
        std::cout << " - " << test_coauthors[i].first << " (共合作 " << test_coauthors[i].second << " 篇)\n";
    }
    if (test_coauthors.size() > display_limit) {
        std::cout << "   ... (总计 " << test_coauthors.size() << " 人)\n";
    }

    std::cout << "\n========== F2 静态测试：最短合作路径 (BFS) ==========\n";
    // 修复：重命名为 test_path 避免冲突
    std::vector<std::string> test_path = graph.findShortestPath(start_author, target_author);
    if (test_path.empty()) {
        std::cout << "未找到 [" << start_author << "] 与 [" << target_author << "] 之间的合作联系。\n";
    }
    else {
        std::cout << "路径：";
        for (size_t i = 0; i < test_path.size(); ++i) {
            std::cout << test_path[i] << (i == test_path.size() - 1 ? "" : " -> ");
        }
        std::cout << "\n(学术分隔度: " << test_path.size() - 1 << " 级连接)\n";
    }

    // 2. F2 交互搜索循环
    std::string search_name;
    std::cout << "\n>>>>>> F2 交互式相关搜索启动 <<<<<<\n";
    while (true) {
        std::cout << "\n请输入要查询的作者姓名 (回车跳过, 输入 'quit' 退出): ";
        if (!std::getline(std::cin, search_name) || search_name == "quit") break;
        if (search_name.empty()) break;

        std::cout << "\n[搜索结果] 正在查询作者: " << search_name << " ...\n";

        // 修复：重命名为 search_coauthors 避免与外部变量冲突
        auto search_coauthors = graph.queryCoauthors(search_name);
        if (search_coauthors.empty()) {
            std::cout << "  (未找到该作者或无合作关系)\n";
        }
        else {
            std::cout << "核心合作者 (按合作次数排序):\n";
            for (const auto& pair : search_coauthors) {
                std::cout << " - " << std::left << std::setw(30) << pair.first
                    << "合作次数: " << pair.second << "\n";
            }

            std::string json_file = search_name + "_network.json";
            if (graph.exportAuthorNetworkJSON(search_name, json_file)) {
                std::cout << "\n[提示] 已将关系网导出至 " << json_file << "\n";
            }
        }
    }

    // 3. 模式选择循环
    std::string choice;
    while (true) {
        std::cout << "\n==============================================\n";
        std::cout << "请选择分析模式：\n";
        std::cout << "1. 单人深度分析 (相关搜索 + 最短路径 + 局部聚团)\n";
        std::cout << "2. 全网全量分析 (全局聚团阶数统计 - 可能较慢)\n";
        std::cout << "0. 退出系统\n";
        std::cout << "==============================================\n";
        std::cout << "您的选择: ";
        if (!std::getline(std::cin, choice) || choice == "0" || choice == "quit") break;

        if (choice == "1") {
            std::string name, target;
            std::cout << "请输入学者姓名: ";
            std::getline(std::cin, name);
            if (name.empty()) continue;

            graph.showRelevantSearch(name);

            std::cout << "\n[最短路径] 请输入目标学者进行寻路: ";
            std::getline(std::cin, target);
            if (!target.empty()) {
                // 修复：重命名为 result_path 避免冲突
                auto result_path = graph.findShortestPath(name, target);
                if (result_path.empty()) std::cout << "未找到路径。\n";
                else {
                    std::cout << "路径: ";
                    for (size_t i = 0; i < result_path.size(); ++i) {
                        std::cout << result_path[i] << (i == result_path.size() - 1 ? "" : " -> ");
                    }
                    std::cout << "\n";
                }
            }
            graph.printLocalCliqueStatistics(name);
        }
        else if (choice == "2") {
            std::cout << "\n[警告] 正在启动全网聚团分析，是否继续? (y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            if (confirm == "y" || confirm == "Y") {
                graph.printCliqueStatistics();
            }
        }
    }

    return 0;
}

int main() {
    system("chcp 65001 > nul");
    try {
        return run_system_demo();
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "[ERROR] 未知异常\n";
        return EXIT_FAILURE;
    }
}
#endif