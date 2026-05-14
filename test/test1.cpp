# if 0

#include "common/database.hpp"
#include "analysis/statistics_analyzer.hpp"
#include "common/parse_result.hpp"
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

//测试公共数据层
int test1() {
	Database db;
	if (db.load("D:\\LearnSpaces\\GitSpace\\sci_lib\\dblp.xml") != ParseResult::OK)
		throw std::runtime_error("数据加载出现错误");
	else {
		const std::vector<XmlValue>& val = db.all();
		for (int i = 0; i < 1000; i++) {
			std::cout << "第" << i+1 << "组XmlValue：" << std::endl;
			val[i].print_val();
			std::cout << std::endl;
		}
	}
	return 0;
}

//测试F3和F4功能
static int test2(){
	Database db;
	if (db.load("D:\\LearnSpaces\\GitSpace\\sci_lib\\dblp.xml") != ParseResult::OK)
		throw std::runtime_error("数据加载出现错误");
	else {
		StatisticsAnalyzer sa;
		const std::vector<AuthorStat> author_stats = sa.top_authors(db, 100);
		const YearKeywordTop year_keywords = sa.yearly_hot_keywords(db, 10);

		std::cout << "\n========== F3 作者统计：发文量 Top 100 ==========\n";
		std::cout << std::left
				  << std::setw(8) << "Rank"
				  << std::setw(42) << "Author"
				  << std::right << std::setw(12) << "Papers" << '\n';
		std::cout << std::string(62, '-') << '\n';
		for (size_t i = 0; i < author_stats.size(); ++i) {
			std::cout << std::left
					  << std::setw(8) << (i + 1)
					  << std::setw(42) << author_stats[i].author
					  << std::right << std::setw(12) << author_stats[i].paper_count
					  << '\n';
		}

		std::vector<std::string> years;
		years.reserve(year_keywords.size());
		for (const auto& item : year_keywords) {
			years.push_back(item.first);
		}
		std::sort(years.begin(), years.end());

		std::cout << "\n========== F4 热点分析：每年标题关键词 Top 10 ==========\n";
		for (const std::string& year : years) {
			const auto it = year_keywords.find(year);
			if (it == year_keywords.end()) {
				continue;
			}

			std::cout << "\n[" << year << "]\n";
			std::cout << std::left
					  << std::setw(8) << "Rank"
					  << std::setw(32) << "Keyword"
					  << std::right << std::setw(10) << "Count" << '\n';
			std::cout << std::string(50, '-') << '\n';

			const std::vector<KeywordStat>& keywords = it->second;
			for (size_t i = 0; i < keywords.size(); ++i) {
				std::cout << std::left
						  << std::setw(8) << (i + 1)
						  << std::setw(32) << keywords[i].keyword
						  << std::right << std::setw(10) << keywords[i].count
						  << '\n';
			}
		}
	}
	return 0;
}

int main() {
	try {
		return test2();
	}
	catch (const std::out_of_range& e) {
		ERROR("访问越界: " << e.what());
		return EXIT_FAILURE;
	}
	catch (const std::overflow_error& e) {
		ERROR("数值溢出: " << e.what());
		return EXIT_FAILURE;
	}
	catch (const std::runtime_error& e) {
		ERROR("运行时错误: " << e.what());
		return EXIT_FAILURE;
	}
	catch (const std::exception& e) {
		ERROR("标准异常: " << e.what());
		return EXIT_FAILURE;
	}
	catch (...) {
		ERROR("未知异常");
		return EXIT_FAILURE;
	}
}

#endif