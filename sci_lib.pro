QT += core gui widgets printsupport

CONFIG += c++17
CONFIG -= app_bundle

TARGET = sci_lib
TEMPLATE = app

DEFINES += LITERATURE_DATA_DIR=\\\"$$PWD\\\"

INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/indexed/gui

SOURCES += \
    main.cpp \
    src/indexed/gui/mainwindow.cpp \
    src/indexed/gui/qcustomplot.cpp \
    src/indexed/common/string_pool.cpp \
    src/indexed/common/xml_value.cpp \
    src/indexed/common/serializer.cpp \
    src/indexed/common/database.cpp \
    src/indexed/analysis/statistics_analyzer.cpp \
    src/indexed/search/search_engine.cpp \
    src/indexed/graph/AuthorGraph.cpp

HEADERS += \
    src/indexed/gui/mainwindow.h \
    src/indexed/gui/qcustomplot.h \
    src/index/index_format.hpp \
    src/indexed/common/string_pool.hpp \
    src/indexed/common/xml_value.hpp \
    src/indexed/common/serializer.hpp \
    src/indexed/common/database.hpp \
    src/indexed/analysis/statistics_analyzer.hpp \
    src/indexed/search/search_engine.hpp \
    src/indexed/graph/AuthorGraph.hpp

OTHER_FILES += \
    CMakeLists.txt \
    docs/*
