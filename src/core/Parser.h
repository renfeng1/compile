#ifndef MINIPAS_PARSER_H
#define MINIPAS_PARSER_H

#include "core/Compiler.h"

#include <map>
#include <memory>

namespace minipas {

struct ParseResult {
    std::vector<Symbol> symbols;
    std::string astText;
    std::vector<Quad> quads;
    std::vector<Diagnostic> diagnostics;
};

class Parser {
public:
    Parser(std::vector<Token> tokens,
           std::vector<Constant> constants,
           std::vector<TableEntry> identifiers);

    ParseResult parse();

private:
    struct AstNode {
        explicit AstNode(std::string text) : label(std::move(text)) {}
        std::string label;
        std::vector<std::unique_ptr<AstNode>> children;
    };

    struct TypeInfo {
        std::string name = "error";
        std::string base = "error";
        int length = 0;
        int arrayLength = 0;
    };

    struct Expr {
        std::string place;
        std::string type = "error";
        std::unique_ptr<AstNode> node;
    };

    const Token &peek(int offset = 0) const;
    const Token &previous() const;
    bool isAtEnd() const;
    const Token &advance();
    bool checkKeyword(const std::string &lexeme) const;
    bool checkDelimiter(const std::string &lexeme) const;
    bool check(TokenKind kind) const;
    bool matchKeyword(const std::string &lexeme);
    bool matchDelimiter(const std::string &lexeme);
    bool match(TokenKind kind);
    Token consumeKeyword(const std::string &lexeme, const std::string &message);
    Token consumeDelimiter(const std::string &lexeme, const std::string &message);
    Token consume(TokenKind kind, const std::string &message);

    std::unique_ptr<AstNode> parseProgram();
    std::unique_ptr<AstNode> parseVarSection();
    TypeInfo parseType();
    std::unique_ptr<AstNode> parseCompoundStatement();
    std::unique_ptr<AstNode> parseStatement();
    std::unique_ptr<AstNode> parseAssignment();
    std::unique_ptr<AstNode> parseIf();
    std::unique_ptr<AstNode> parseWhile();
    std::unique_ptr<AstNode> parseRead();
    std::unique_ptr<AstNode> parseWrite();

    Expr parseExpression();
    Expr parseOr();
    Expr parseAnd();
    Expr parseRelation();
    Expr parseAdditive();
    Expr parseMultiplicative();
    Expr parseUnary();
    Expr parsePrimary();

    Expr makeBinary(const std::string &op, Expr left, Expr right,
                    int line, int column);
    Expr makeUnary(const std::string &op, Expr expr, int line, int column);

    Symbol *findSymbol(const std::string &name);
    const Symbol *findSymbol(const std::string &name) const;
    Symbol *declareSymbol(const Token &token, const TypeInfo &type, const std::string &category);
    std::string symbolPlace(const Symbol &symbol) const;
    std::string newTemp(const std::string &type);
    int emit(const std::string &op, const std::string &arg1,
             const std::string &arg2, const std::string &result);
    int nextQuadIndex() const;
    void patch(int quadIndex, const std::string &result);

    bool isNumeric(const std::string &type) const;
    bool isBoolean(const std::string &type) const;
    bool canAssign(const std::string &target, const std::string &source) const;
    std::string baseTypeOf(const Symbol &symbol) const;
    int lengthOf(const std::string &type) const;
    void addDiagnostic(Severity severity, const std::string &message,
                       int line, int column);
    std::string renderAst(const AstNode &node, int depth = 0) const;

    std::vector<Token> tokens_;
    std::vector<Constant> constants_;
    std::vector<TableEntry> identifiers_;
    size_t current_ = 0;
    ParseResult result_;
    std::map<std::string, size_t> symbolByName_;
    int nextSymbolIndex_ = 1;
    int nextTempIndex_ = 1;
    int nextAddress_ = 0;
};

} // namespace minipas

#endif // MINIPAS_PARSER_H
