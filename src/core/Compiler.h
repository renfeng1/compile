#ifndef MINIPAS_COMPILER_H
#define MINIPAS_COMPILER_H

#include <string>
#include <vector>

namespace minipas {

enum class TokenKind {
    Keyword,
    Identifier,
    Delimiter,
    Constant,
    End,
    Invalid
};

enum class Severity {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    Severity severity = Severity::Info;
    std::string stage;
    std::string message;
    int line = 1;
    int column = 1;
};

struct TableEntry {
    int index = 0;
    std::string name;
    std::string category;
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string lexeme;
    std::string table;
    int index = 0;
    int line = 1;
    int column = 1;
    std::string valueType;
};

struct Constant {
    int index = 0;
    std::string literal;
    std::string type;
    std::string value;
};

struct Symbol {
    int index = 0;
    std::string name;
    std::string type;
    std::string category;
    int address = 0;
    int length = 0;
    int arrayLength = 0;
    int level = 0;
};

struct Quad {
    int index = 0;
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
    int block = 0;
};

struct TargetInstruction {
    int index = 0;
    std::string op;
    std::string a;
    std::string b;
    std::string c;
};

struct MemoryCell {
    std::string name;
    std::string type;
    std::string value;
};

struct CompileOptions {
    bool enableOptimization = true;
    bool runVm = true;
};

struct CompilationResult {
    bool success = false;
    std::vector<TableEntry> keywordTable;
    std::vector<TableEntry> delimiterTable;
    std::vector<Token> tokens;
    std::vector<TableEntry> identifiers;
    std::vector<Constant> constants;
    std::vector<Symbol> symbols;
    std::string astText;
    std::vector<Quad> quads;
    std::vector<Quad> optimizedQuads;
    std::vector<TargetInstruction> targetCode;
    std::vector<std::string> vmTrace;
    std::vector<std::string> vmOutput;
    std::vector<MemoryCell> finalMemory;
    std::vector<Diagnostic> diagnostics;
};

class Compiler {
public:
    CompilationResult compile(const std::string &source,
                              const CompileOptions &options = CompileOptions()) const;
};

std::string tokenKindName(TokenKind kind);
std::string severityName(Severity severity);

} // namespace minipas

#endif // MINIPAS_COMPILER_H
