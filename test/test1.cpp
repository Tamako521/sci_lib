#include "common/database.hpp"
#include "common/parse_result.hpp"
#include <cstdlib>
#include <exception>
#include <stdexcept>

static int run() {
	Database db;
	if (db.load("D:\\LearnSpaces\\GitSpace\\sci_lib\\dblp.xml") != ParseResult::OK)
		throw std::runtime_error("数据加载出现错误");
	else {
		const std::vector<XmlValue>& val = db.all();
		for (int i = 0; i < 10; i++) {
			std::cout << "第" << i+1 << "组XmlValue：" << std::endl;
			val[i].print_val();
			std::cout << std::endl;
		}
	}
	return 0;
}

int main() {
	try {
		return run();
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
