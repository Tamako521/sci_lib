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
    src/indexed/common/string_pool.cpp \
    src/indexed/common/xml_value.cpp \
    src/indexed/common/database.cpp \
    src/indexed/common/serializer.cpp

HEADERS += \
    src/index/index_builder.hpp \
    src/index/index_writer.hpp \
    src/index/index_format.hpp \
    src/indexed/common/string_pool.hpp \
    src/indexed/common/xml_value.hpp \
    src/indexed/common/database.hpp \
    src/indexed/common/serializer.hpp
