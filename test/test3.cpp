#if 0

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <sstream>
#include <limits>

#include "src/common/database.hpp"
#include "src/analysis/statistics_analyzer.hpp"
#include "src/search/search_engine.hpp"

/**
 * @brief 打印使用帮助
 */
void print_help() {
    std::cout << "\n"
              << "========================================\n"
              << "      sci_lib 论文搜索系统\n"
              << "========================================\n"
              << "\n"
              << "支持的命令：\n"
              << "\n"
              << "  F1 - 基本搜索功能\n"
              << "  ─────────────────────────────────────\n"
              << "  author:<作者名>    搜索指定作者的所有论文\n"
              << "                    例如: author:John Smith\n"
              << "\n"
              << "  title:<完整标题>   搜索指定标题的论文\n"
              << "                    例如: title:Database Systems\n"
              << "\n"
              << "  key:<DBLP key>     通过DBLP唯一标识搜索\n"
              << "                    例如: key:journals/tods/Smith05\n"
              << "\n"
              << "  F5 - 部分匹配搜索功能\n"
              << "  ─────────────────────────────────────\n"
              << "  <关键词>            模糊搜索题目中包含该关键词的论文\n"
              << "                    例如: database\n"
              << "\n"
              << "  keywords:<kw1,kw2> 多关键词搜索（逗号分隔）\n"
              << "                    例如: keywords:machine,learning\n"
              << "\n"
              << "  其他命令：\n"
              << "  ─────────────────────────────────────\n"
              << "  help                显示帮助信息\n"
              << "  quit / exit         退出程序\n"
              << "  stats               显示数据库统计信息\n"
              << "\n"
              << "========================================\n";
}

/**
 * @brief 打印数据库统计信息
 */
void print_stats(const Database& db) {
    std::cout << "\n"
              << "========================================\n"
              << "      数据库统计信息\n"
              << "========================================\n"
              << "  论文总数: " << db.size() << "\n"
              << "========================================\n";
}

/**
 * @brief 解析并执行搜索命令
 */
std::vector<SearchResult> parse_and_search(const std::string& cmd, 
                                          const SearchEngine& engine) {
    std::string trimmed = cmd;
    
    // 去除首尾空格
    while (!trimmed.empty() && std::isspace(trimmed.front())) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() && std::isspace(trimmed.back())) {
        trimmed.pop_back();
    }
    
    if (trimmed.empty()) {
        return {};
    }
    
    // 多关键词搜索: keywords:kw1,kw2,kw3
    if (trimmed.rfind("keywords:", 0) == 0) {
        std::string keywords_str = trimmed.substr(9);
        std::vector<std::string> keywords;
        std::stringstream ss(keywords_str);
        std::string kw;
        
        while (std::getline(ss, kw, ',')) {
            // 去除每个关键词的首尾空格
            while (!kw.empty() && std::isspace(kw.front())) kw.erase(kw.begin());
            while (!kw.empty() && std::isspace(kw.back())) kw.pop_back();
            if (!kw.empty()) {
                keywords.push_back(kw);
            }
        }
        
        if (keywords.size() >= 2) {
            return engine.search_by_keywords(keywords, true);  // AND 模式
        } else if (keywords.size() == 1) {
            return engine.search_by_keyword(keywords[0], SearchMode::FUZZY);
        }
        return {};
    }
    
    // 使用搜索引擎的智能搜索
    return engine.smart_search(trimmed);
}

/**
 * @brief 主菜单
 */
void print_menu() {
    std::cout << "\n请输入搜索命令 (输入 help 查看帮助): ";
}

int main() {
    std::cout << "\n========================================\n"
              << "      欢迎使用 sci_lib 论文搜索系统\n"
              << "========================================\n"
              << "\n正在加载数据库...\n";
    
    // 加载数据库
    Database db;
    ParseResult result = db.load("data/dblp.xml");
    
    if (result != ParseResult::OK) {
        ERROR("数据库加载失败: " << parse_result_name(result));
        return 1;
    }
    
    std::cout << "\n数据库加载成功！共 " << db.size() << " 篇论文\n";
    
    // 创建搜索引擎
    SearchEngine engine(&db);
    
    // 主循环
    std::string input;
    while (true) {
        print_menu();
        
        if (!std::getline(std::cin, input)) {
            std::cout << "\n再见！\n";
            break;
        }
        
        // 去除首尾空格
        while (!input.empty() && std::isspace(input.front())) {
            input.erase(input.begin());
        }
        while (!input.empty() && std::isspace(input.back())) {
            input.pop_back();
        }
        
        if (input.empty()) {
            continue;
        }
        
        // 命令处理
        if (input == "help" || input == "?") {
            print_help();
        } else if (input == "quit" || input == "exit" || input == "q") {
            std::cout << "\n再见！\n";
            break;
        } else if (input == "stats" || input == "s") {
            print_stats(db);
        } else {
            // 执行搜索
            auto results = parse_and_search(input, engine);
            
            if (results.empty()) {
                std::cout << "\n未找到匹配的论文，请尝试其他关键词。\n";
                std::cout << "提示：\n";
                std::cout << "  - 使用 'author:姓名' 搜索作者\n";
                std::cout << "  - 使用 'title:标题' 搜索完整标题\n";
                std::cout << "  - 直接输入关键词进行模糊搜索\n";
            } else {
                // 显示结果（限制最多显示100条）
                std::cout << SearchEngine::format_results(results, 100);
            }
        }
    }
    
    return 0;
}


#endif 