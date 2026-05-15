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
    src/common/database.cpp
)

set(COMMON_HEADERS
    src/index/index_format.hpp
    src/common/string_pool.hpp
    src/common/xml_value.hpp
    src/common/database.hpp
)

# 3. 构建库：同时传入源文件和头文件
add_library(common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

# 包含目录设置保持不变
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# 分析库: analysis
# ============================================================================
set(ANALYSIS_SOURCES
    src/analysis/statistics_analyzer.cpp
)

set(ANALYSIS_HEADERS
    src/analysis/statistics_analyzer.hpp
)

add_library(analysis STATIC ${ANALYSIS_SOURCES} ${ANALYSIS_HEADERS})
target_include_directories(analysis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(analysis PUBLIC common)

# ============================================================================
# Qt GUI 可执行文件: sci_lib
# ============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PrintSupport)

set(GUI_SOURCES
    main.cpp
    src/gui/mainwindow.cpp
    src/gui/mainwindow.h
    src/gui/qcustomplot.cpp
    src/gui/qcustomplot.h
    src/search/search_engine.cpp
    src/search/search_engine.hpp
    src/graph/AuthorGraph.cpp
    src/graph/AuthorGraph.hpp
)

add_executable(sci_lib ${GUI_SOURCES})
target_include_directories(sci_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)
target_compile_definitions(sci_lib PRIVATE
    LITERATURE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(sci_lib PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    analysis
)

add_executable(index_builder
    src/index/index_builder_main.cpp
    src/index/index_builder.cpp
    src/index/index_builder.hpp
    src/index/index_writer.cpp
    src/index/index_writer.hpp
    src/index/index_format.hpp
    src/common/string_pool.cpp
    src/common/string_pool.hpp
    src/common/xml_value.cpp
    src/common/xml_value.hpp
    src/common/database.cpp
    src/common/database.hpp
)
target_include_directories(index_builder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

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
