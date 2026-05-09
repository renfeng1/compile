# MiniPascal Visual Compiler

MiniPascal is a C++17 + Qt Widgets course-design compiler system for a small Pascal-like language. It implements lexical analysis, recursive-descent parsing, semantic analysis, quadruple generation, optimization, target instruction generation, and a small VM execution platform.

## Features

- Source editor and one-click compilation in Qt Widgets.
- Token, keyword, delimiter, identifier, constant, and symbol tables.
- Syntax tree text view, quadruples, optimized quadruples, target instructions, VM trace, and output.
- Language support: `program`, `var`, `begin/end`, assignment, `if/then/else`, `while/do`, arrays, `read`, `write`, arithmetic, relational, and logical expressions.
- Core compiler is independent from Qt and can be tested with plain `g++`.

## Build Core Tests

```powershell
New-Item -ItemType Directory -Force build
g++ -std=c++17 -Isrc tests/core_tests.cpp src/core/Compiler.cpp src/core/Lexer.cpp src/core/Parser.cpp src/core/Optimizer.cpp src/core/Target.cpp -o build/core_tests.exe
.\build\core_tests.exe
```

## Build Qt GUI

The local machine currently has Qt 5.15.2 from Anaconda. If `qmake` points to a MSVC spec, install Visual Studio Build Tools or switch to a matching Qt MinGW kit.

```powershell
qmake MiniPascal.pro
mingw32-make
```

If qmake reports an MSVC/MinGW mismatch, use Qt Creator and select a kit whose compiler matches the Qt libraries.

On this machine, `qmake -spec win32-g++` can generate a Makefile, but `mingw32-make` currently stops at `libQt5Widgets_conda.a` because the detected Anaconda Qt kit is not a matching MinGW development kit. The core compiler and GUI sources are still syntax-checked separately; install a matching Qt MinGW kit or MSVC Build Tools to produce the GUI executable.

## Example

Open `examples/sample.minipas` or `examples/extended.minipas` in the GUI, then click Compile.
