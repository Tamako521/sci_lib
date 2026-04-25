cmake_minimum_required(VERSION 3.16)
project(sci_lib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#警告即错误
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -finput-charset=UTF-8)
elseif(MSVC)
    add_compile_options(/W4 /WX /utf-8)
endif()

# ============================================================================
# 公共库: common
# ============================================================================
# 1. 先收集所有的源文件 (.cpp)
set(COMMON_SOURCES
    src/common/string_pool.cpp
    src/common/xml_value.cpp
    src/common/xml_parser.cpp
    src/common/serializer.cpp
    src/common/database.cpp
)

# 2. 显式收集所有的头文件 (.hpp)，让它们在 VS 中显示
set(COMMON_HEADERS
    src/common/string_pool.hpp
    src/common/xml_value.hpp
    src/common/xml_parser.hpp
    src/common/serializer.hpp
    src/common/database.hpp
    src/common/parse_result.hpp
    src/common/process_resporter.hpp
    # 如果有其他头文件，继续加在这里
)

# 3. 构建库：同时传入源文件和头文件
add_library(common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

# 包含目录设置保持不变
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# 可执行文件: sci_lib (测试入口)
# ============================================================================
add_executable(sci_lib test/test1.cpp)
target_link_libraries(sci_lib PRIVATE common)

# ============================================================================
# 测试 (可选)
# ============================================================================
# option(ENABLE_TESTS "Build tests" OFF)
# if(ENABLE_TESTS)
#     enable_testing()
#     add_subdirectory(test)
# endif()

# ============================================================================
#自动将sci_lib设为启动项目
# ============================================================================
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT sci_lib)
