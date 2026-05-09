#ifndef MINIPAS_LEXER_H
#define MINIPAS_LEXER_H

#include "core/Compiler.h"

#include <map>
#include <string>

namespace minipas {

struct LexResult {
    std::vector<Token> tokens;
    std::vector<TableEntry> identifiers;
    std::vector<Constant> constants;
    std::vector<Diagnostic> diagnostics;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    LexResult scan();

    static std::vector<TableEntry> keywordTable();
    static std::vector<TableEntry> delimiterTable();

private:
    char peek(int offset = 0) const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespaceAndComments();
    void addDiagnostic(Severity severity, const std::string &message, int line, int column);
    Token makeToken(TokenKind kind, const std::string &lexeme, const std::string &table,
                    int index, int line, int column, const std::string &valueType = "");
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanChar();
    Token scanDelimiter();
    int identifierIndex(const std::string &name);
    int constantIndex(const std::string &literal, const std::string &type, const std::string &value);

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    LexResult result_;
    std::map<std::string, int> identifierMap_;
    std::map<std::string, int> constantMap_;
};

} // namespace minipas

#endif // MINIPAS_LEXER_H
