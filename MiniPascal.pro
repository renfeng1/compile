QT += widgets

CONFIG += c++17 console
TEMPLATE = app
TARGET = MiniPascalCompiler

INCLUDEPATH += src

SOURCES += \
    src/core/Compiler.cpp \
    src/core/Lexer.cpp \
    src/core/Optimizer.cpp \
    src/core/Parser.cpp \
    src/core/Target.cpp \
    src/gui/MainWindow.cpp \
    src/gui/main.cpp

HEADERS += \
    src/core/Compiler.h \
    src/core/Lexer.h \
    src/core/Optimizer.h \
    src/core/Parser.h \
    src/core/Target.h \
    src/gui/MainWindow.h
