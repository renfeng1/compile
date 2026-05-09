#include "core/Compiler.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

using namespace minipas;

namespace {

bool hasError(const CompilationResult &result) {
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                       [](const Diagnostic &d) { return d.severity == Severity::Error; });
}

const MemoryCell *findMemory(const CompilationResult &result, const std::string &name) {
    for (const auto &cell : result.finalMemory) {
        if (cell.name == name) {
            return &cell;
        }
    }
    return nullptr;
}

bool optimizedContainsLiteral(const CompilationResult &result, const std::string &literal) {
    for (const auto &quad : result.optimizedQuads) {
        if (quad.arg1 == literal || quad.arg2 == literal || quad.result == literal) {
            return true;
        }
    }
    return false;
}

void printDiagnostics(const CompilationResult &result) {
    for (const auto &diagnostic : result.diagnostics) {
        std::cerr << severityName(diagnostic.severity) << " "
                  << diagnostic.stage << " "
                  << diagnostic.line << ":" << diagnostic.column << " "
                  << diagnostic.message << "\n";
    }
}

void pptSampleCompilesAndRuns() {
    const std::string source =
        "program example\n"
        "var a,b: integer;\n"
        "begin\n"
        "  a := 2;\n"
        "  b := 2 * 5 + a\n"
        "end.";

    Compiler compiler;
    CompilationResult result = compiler.compile(source);
    if (hasError(result)) {
        printDiagnostics(result);
    }
    assert(result.success);
    assert(!result.tokens.empty());
    assert(!result.symbols.empty());
    assert(!result.quads.empty());
    assert(!result.optimizedQuads.empty());
    assert(optimizedContainsLiteral(result, "10"));
    const MemoryCell *a = findMemory(result, "a");
    const MemoryCell *b = findMemory(result, "b");
    assert(a && a->value == "2");
    assert(b && b->value == "12");
}

void whileArrayAndWriteWork() {
    const std::string source =
        "program loops\n"
        "var i,sum: integer;\n"
        "    nums: array[3] of integer;\n"
        "begin\n"
        "  i := 0;\n"
        "  sum := 0;\n"
        "  nums[0] := 1;\n"
        "  nums[1] := 2;\n"
        "  nums[2] := 3;\n"
        "  while i < 3 do\n"
        "  begin\n"
        "    sum := sum + nums[i];\n"
        "    i := i + 1\n"
        "  end;\n"
        "  write(sum)\n"
        "end.";

    Compiler compiler;
    CompilationResult result = compiler.compile(source);
    if (hasError(result)) {
        printDiagnostics(result);
    }
    assert(result.success);
    const MemoryCell *sum = findMemory(result, "sum");
    assert(sum && sum->value == "6");
    assert(!result.vmOutput.empty());
    assert(result.vmOutput.back() == "6");
}

void ifElseAndBoolWork() {
    const std::string source =
        "program branch\n"
        "var a,b: integer;\n"
        "    ok: bool;\n"
        "begin\n"
        "  a := 5;\n"
        "  ok := a > 3;\n"
        "  if ok then b := 1 else b := 2;\n"
        "  write(b)\n"
        "end.";

    Compiler compiler;
    CompilationResult result = compiler.compile(source);
    if (hasError(result)) {
        printDiagnostics(result);
    }
    assert(result.success);
    const MemoryCell *b = findMemory(result, "b");
    assert(b && b->value == "1");
    assert(result.vmOutput.back() == "1");
}

void undeclaredIdentifierReportsError() {
    const std::string source =
        "program bad\n"
        "var a: integer;\n"
        "begin\n"
        "  b := 1\n"
        "end.";

    Compiler compiler;
    CompilationResult result = compiler.compile(source);
    assert(!result.success);
    assert(hasError(result));
}

} // namespace

int main() {
    pptSampleCompilesAndRuns();
    whileArrayAndWriteWork();
    ifElseAndBoolWork();
    undeclaredIdentifierReportsError();
    std::cout << "core tests passed\n";
    return 0;
}
