QT -= core gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = index_builder
TEMPLATE = app

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/index/index_builder_main.cpp \
    src/index/index_builder.cpp \
    src/index/index_writer.cpp \
    src/common/string_pool.cpp \
    src/common/xml_value.cpp \
    src/common/database.cpp

HEADERS += \
    src/index/index_builder.hpp \
    src/index/index_writer.hpp \
    src/index/index_format.hpp \
    src/common/string_pool.hpp \
    src/common/xml_value.hpp \
    src/common/database.hpp
