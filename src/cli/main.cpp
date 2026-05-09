#include "core/Compiler.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open source file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void printSection(const std::string &title) {
    std::cout << "\n========== " << title << " ==========\n";
}

void printTables(const minipas::CompilationResult &result) {
    printSection("Keyword Table");
    for (const auto &entry : result.keywordTable) {
        std::cout << entry.index << "\t" << entry.name << "\n";
    }

    printSection("Delimiter Table");
    for (const auto &entry : result.delimiterTable) {
        std::cout << entry.index << "\t" << entry.name << "\n";
    }

    printSection("Identifier Table");
    for (const auto &entry : result.identifiers) {
        std::cout << "I" << entry.index << "\t" << entry.name << "\n";
    }

    printSection("Constant Table");
    for (const auto &constant : result.constants) {
        std::cout << "C" << constant.index << "\t"
                  << constant.literal << "\t"
                  << constant.type << "\t"
                  << constant.value << "\n";
    }
}

void printTokens(const minipas::CompilationResult &result) {
    printSection("Token Sequence");
    for (const auto &token : result.tokens) {
        if (token.kind == minipas::TokenKind::End) {
            continue;
        }
        std::cout << "(" << token.table << "," << token.index << ")"
                  << "\t" << token.lexeme
                  << "\t" << minipas::tokenKindName(token.kind)
                  << "\tline " << token.line << ":" << token.column << "\n";
    }
}

void printSymbols(const minipas::CompilationResult &result) {
    printSection("Symbol Table / Activation Record");
    std::cout << "Index\tName\tType\tCategory\tAddress\tLength\tArray\n";
    for (const auto &symbol : result.symbols) {
        std::cout << symbol.index << "\t"
                  << symbol.name << "\t"
                  << symbol.type << "\t"
                  << symbol.category << "\t"
                  << symbol.address << "\t"
                  << symbol.length << "\t"
                  << symbol.arrayLength << "\n";
    }
}

void printQuads(const std::string &title, const std::vector<minipas::Quad> &quads) {
    printSection(title);
    std::cout << "#\tBlock\t(op,arg1,arg2,result)\n";
    for (const auto &quad : quads) {
        std::cout << quad.index << "\t" << quad.block << "\t("
                  << quad.op << ", "
                  << quad.arg1 << ", "
                  << quad.arg2 << ", "
                  << quad.result << ")\n";
    }
}

void printTarget(const minipas::CompilationResult &result) {
    printSection("Target Code / MiniASM");
    for (const auto &instruction : result.targetCode) {
        std::cout << instruction.index << "\t" << instruction.op;
        if (!instruction.a.empty()) std::cout << "\t" << instruction.a;
        if (!instruction.b.empty()) std::cout << "\t" << instruction.b;
        if (!instruction.c.empty()) std::cout << "\t" << instruction.c;
        std::cout << "\n";
    }
}

void printVm(const minipas::CompilationResult &result) {
    printSection("VM Trace");
    for (const auto &line : result.vmTrace) {
        std::cout << line << "\n";
    }

    printSection("Output");
    for (const auto &line : result.vmOutput) {
        std::cout << line << "\n";
    }

    printSection("Final Memory");
    for (const auto &cell : result.finalMemory) {
        std::cout << cell.name << "\t" << cell.type << "\t" << cell.value << "\n";
    }
}

void printDiagnostics(const minipas::CompilationResult &result) {
    printSection("Diagnostics");
    if (result.diagnostics.empty()) {
        std::cout << "no diagnostics\n";
        return;
    }
    for (const auto &diagnostic : result.diagnostics) {
        std::cout << minipas::severityName(diagnostic.severity)
                  << "\t" << diagnostic.stage
                  << "\t" << diagnostic.line << ":" << diagnostic.column
                  << "\t" << diagnostic.message << "\n";
    }
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  minipas_cli <source.minipas> [--no-opt] [--no-run]\n\n"
              << "Example:\n"
              << "  minipas_cli examples/sample.minipas\n";
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    minipas::CompileOptions options;
    std::string path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-opt") {
            options.enableOptimization = false;
        } else if (arg == "--no-run") {
            options.runVm = false;
        } else if (path.empty()) {
            path = arg;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    try {
        const std::string source = readFile(path);
        minipas::Compiler compiler;
        const minipas::CompilationResult result = compiler.compile(source, options);

        std::cout << "MiniPascal CLI compiler\n";
        std::cout << "Source: " << path << "\n";
        std::cout << "Status: " << (result.success ? "success" : "failed") << "\n";

        printTokens(result);
        printTables(result);
        printSymbols(result);
        printSection("AST");
        std::cout << result.astText;
        printQuads("Quadruples", result.quads);
        printQuads("Optimized Quadruples", result.optimizedQuads);
        printTarget(result);
        printVm(result);
        printDiagnostics(result);

        return result.success ? 0 : 2;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
