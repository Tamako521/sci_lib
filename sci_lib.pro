QT += core gui widgets printsupport

CONFIG += c++17
CONFIG -= app_bundle

TARGET = sci_lib
TEMPLATE = app

DEFINES += LITERATURE_DATA_DIR=\\\"$$PWD\\\"

INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/gui

SOURCES += \
    main.cpp \
    src/gui/mainwindow.cpp \
    src/gui/qcustomplot.cpp \
    src/common/string_pool.cpp \
    src/common/xml_value.cpp \
    src/common/database.cpp \
    src/analysis/statistics_analyzer.cpp \
    src/search/search_engine.cpp \
    src/graph/AuthorGraph.cpp

HEADERS += \
    src/gui/mainwindow.h \
    src/gui/qcustomplot.h \
    src/index/index_format.hpp \
    src/common/string_pool.hpp \
    src/common/xml_value.hpp \
    src/common/database.hpp \
    src/analysis/statistics_analyzer.hpp \
    src/search/search_engine.hpp \
    src/graph/AuthorGraph.hpp

OTHER_FILES += \
    CMakeLists.txt \
    docs/*
