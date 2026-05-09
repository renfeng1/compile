#include "core/Parser.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace minipas {
namespace {

const Token kEndToken{TokenKind::End, "$", "$", 0, 1, 1, ""};

std::string toLower(std::string text) {
    for (char &ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

bool isComparison(const std::string &op) {
    return op == "=" || op == "<>" || op == "<" || op == "<=" || op == ">" || op == ">=";
}

} // namespace

Parser::Parser(std::vector<Token> tokens,
               std::vector<Constant> constants,
               std::vector<TableEntry> identifiers)
    : tokens_(std::move(tokens)),
      constants_(std::move(constants)),
      identifiers_(std::move(identifiers)) {
    for (const auto &entry : identifiers_) {
        nextSymbolIndex_ = std::max(nextSymbolIndex_, entry.index + 1);
    }
}

ParseResult Parser::parse() {
    auto root = parseProgram();
    result_.astText = root ? renderAst(*root) : "";
    for (auto &symbol : result_.symbols) {
        if (symbol.category == "program") {
            symbol.length = nextAddress_;
        }
    }
    return std::move(result_);
}

const Token &Parser::peek(int offset) const {
    const size_t index = current_ + static_cast<size_t>(offset);
    if (index >= tokens_.size()) {
        return kEndToken;
    }
    return tokens_[index];
}

const Token &Parser::previous() const {
    if (current_ == 0 || current_ - 1 >= tokens_.size()) {
        return kEndToken;
    }
    return tokens_[current_ - 1];
}

bool Parser::isAtEnd() const {
    return peek().kind == TokenKind::End;
}

const Token &Parser::advance() {
    if (!isAtEnd()) {
        ++current_;
    }
    return previous();
}

bool Parser::checkKeyword(const std::string &lexeme) const {
    return peek().kind == TokenKind::Keyword && peek().lexeme == lexeme;
}

bool Parser::checkDelimiter(const std::string &lexeme) const {
    return peek().kind == TokenKind::Delimiter && peek().lexeme == lexeme;
}

bool Parser::check(TokenKind kind) const {
    return peek().kind == kind;
}

bool Parser::matchKeyword(const std::string &lexeme) {
    if (!checkKeyword(lexeme)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::matchDelimiter(const std::string &lexeme) {
    if (!checkDelimiter(lexeme)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::match(TokenKind kind) {
    if (!check(kind)) {
        return false;
    }
    advance();
    return true;
}

Token Parser::consumeKeyword(const std::string &lexeme, const std::string &message) {
    if (checkKeyword(lexeme)) {
        return advance();
    }
    addDiagnostic(Severity::Error, message, peek().line, peek().column);
    return peek();
}

Token Parser::consumeDelimiter(const std::string &lexeme, const std::string &message) {
    if (checkDelimiter(lexeme)) {
        return advance();
    }
    addDiagnostic(Severity::Error, message, peek().line, peek().column);
    return peek();
}

Token Parser::consume(TokenKind kind, const std::string &message) {
    if (check(kind)) {
        return advance();
    }
    addDiagnostic(Severity::Error, message, peek().line, peek().column);
    return peek();
}

std::unique_ptr<Parser::AstNode> Parser::parseProgram() {
    auto node = std::make_unique<AstNode>("Program");
    consumeKeyword("program", "expected 'program' at the beginning of source");
    Token name = consume(TokenKind::Identifier, "expected program identifier");
    TypeInfo programType{"program", "program", 0, 0};
    declareSymbol(name, programType, "program");
    node->children.push_back(std::make_unique<AstNode>("program " + name.lexeme));
    emit("program", symbolPlace(*findSymbol(name.lexeme)), "_", "_");

    if (checkKeyword("var")) {
        node->children.push_back(parseVarSection());
    }
    node->children.push_back(parseCompoundStatement());
    consumeDelimiter(".", "expected '.' after program body");
    emit("end", symbolPlace(*findSymbol(name.lexeme)), "_", "_");
    return node;
}

std::unique_ptr<Parser::AstNode> Parser::parseVarSection() {
    auto node = std::make_unique<AstNode>("VarSection");
    consumeKeyword("var", "expected 'var'");
    while (check(TokenKind::Identifier)) {
        std::vector<Token> names;
        names.push_back(consume(TokenKind::Identifier, "expected identifier in declaration"));
        while (matchDelimiter(",")) {
            names.push_back(consume(TokenKind::Identifier, "expected identifier after ','"));
        }
        consumeDelimiter(":", "expected ':' in declaration");
        TypeInfo type = parseType();
        consumeDelimiter(";", "expected ';' after declaration");

        std::ostringstream label;
        label << "declare ";
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) {
                label << ", ";
            }
            label << names[i].lexeme;
            declareSymbol(names[i], type, "variable");
        }
        label << " : " << type.name;
        node->children.push_back(std::make_unique<AstNode>(label.str()));
    }
    return node;
}

Parser::TypeInfo Parser::parseType() {
    if (matchKeyword("array")) {
        consumeDelimiter("[", "expected '[' after 'array'");
        Token sizeToken = consume(TokenKind::Constant, "expected array size");
        int arrayLength = 0;
        try {
            arrayLength = std::stoi(sizeToken.lexeme);
        } catch (...) {
            arrayLength = 0;
        }
        if (arrayLength <= 0 || sizeToken.valueType != "integer") {
            addDiagnostic(Severity::Error, "array size must be a positive integer constant",
                          sizeToken.line, sizeToken.column);
            arrayLength = 1;
        }
        consumeDelimiter("]", "expected ']' after array size");
        consumeKeyword("of", "expected 'of' in array type");
        TypeInfo base = parseType();
        if (base.arrayLength > 0) {
            addDiagnostic(Severity::Error, "nested arrays are not supported", previous().line, previous().column);
        }
        TypeInfo type;
        type.base = base.base;
        type.arrayLength = arrayLength;
        type.length = base.length * arrayLength;
        std::ostringstream name;
        name << "array[" << arrayLength << "] of " << base.base;
        type.name = name.str();
        return type;
    }

    if (matchKeyword("integer")) {
        return {"integer", "integer", lengthOf("integer"), 0};
    }
    if (matchKeyword("real")) {
        return {"real", "real", lengthOf("real"), 0};
    }
    if (matchKeyword("char")) {
        return {"char", "char", lengthOf("char"), 0};
    }
    if (matchKeyword("bool")) {
        return {"bool", "bool", lengthOf("bool"), 0};
    }
    addDiagnostic(Severity::Error, "expected a type", peek().line, peek().column);
    return {"error", "error", 0, 0};
}

std::unique_ptr<Parser::AstNode> Parser::parseCompoundStatement() {
    auto node = std::make_unique<AstNode>("CompoundStatement");
    consumeKeyword("begin", "expected 'begin'");
    while (!isAtEnd() && !checkKeyword("end")) {
        node->children.push_back(parseStatement());
        if (matchDelimiter(";")) {
            continue;
        }
        if (!checkKeyword("end")) {
            addDiagnostic(Severity::Error, "expected ';' between statements",
                          peek().line, peek().column);
            if (!isAtEnd()) {
                advance();
            }
        }
    }
    consumeKeyword("end", "expected 'end'");
    return node;
}

std::unique_ptr<Parser::AstNode> Parser::parseStatement() {
    if (check(TokenKind::Identifier)) {
        return parseAssignment();
    }
    if (checkKeyword("begin")) {
        return parseCompoundStatement();
    }
    if (checkKeyword("if")) {
        return parseIf();
    }
    if (checkKeyword("while")) {
        return parseWhile();
    }
    if (checkKeyword("read")) {
        return parseRead();
    }
    if (checkKeyword("write")) {
        return parseWrite();
    }
    addDiagnostic(Severity::Error, "expected a statement", peek().line, peek().column);
    if (!isAtEnd()) {
        advance();
    }
    return std::make_unique<AstNode>("ErrorStatement");
}

std::unique_ptr<Parser::AstNode> Parser::parseAssignment() {
    Token name = consume(TokenKind::Identifier, "expected assignment target");
    auto node = std::make_unique<AstNode>("Assign " + name.lexeme);
    Symbol *symbol = findSymbol(name.lexeme);
    std::string targetType = symbol ? baseTypeOf(*symbol) : "error";
    std::string targetPlace = symbol ? symbolPlace(*symbol) : name.lexeme;
    const int targetArrayLength = symbol ? symbol->arrayLength : 0;

    if (!symbol) {
        addDiagnostic(Severity::Error, "undeclared identifier '" + name.lexeme + "'", name.line, name.column);
    }

    bool arrayWrite = false;
    std::string indexPlace;
    if (matchDelimiter("[")) {
        arrayWrite = true;
        Expr index = parseExpression();
        indexPlace = index.place;
        node->children.push_back(std::move(index.node));
        consumeDelimiter("]", "expected ']' after array index");
        if (symbol && targetArrayLength == 0) {
            addDiagnostic(Severity::Error, "identifier '" + name.lexeme + "' is not an array",
                          name.line, name.column);
        }
        if (index.type != "integer") {
            addDiagnostic(Severity::Error, "array index must be integer", name.line, name.column);
        }
    } else if (symbol && targetArrayLength > 0) {
        addDiagnostic(Severity::Error, "array assignment requires an index", name.line, name.column);
    }

    consumeDelimiter(":=", "expected ':=' in assignment");
    Expr value = parseExpression();
    if (!canAssign(targetType, value.type)) {
        addDiagnostic(Severity::Error, "cannot assign " + value.type + " to " + targetType,
                      name.line, name.column);
    }
    node->children.push_back(std::move(value.node));
    if (arrayWrite) {
        emit("[]=", value.place, indexPlace, targetPlace);
    } else {
        emit(":=", value.place, "_", targetPlace);
    }
    return node;
}

std::unique_ptr<Parser::AstNode> Parser::parseIf() {
    auto node = std::make_unique<AstNode>("If");
    Token token = consumeKeyword("if", "expected 'if'");
    Expr condition = parseExpression();
    if (!isBoolean(condition.type)) {
        addDiagnostic(Severity::Error, "if condition must be bool", token.line, token.column);
    }
    node->children.push_back(std::move(condition.node));
    consumeKeyword("then", "expected 'then' after if condition");
    const int falseJump = emit("jfalse", condition.place, "_", "0");
    node->children.push_back(parseStatement());
    if (matchKeyword("else")) {
        const int endJump = emit("j", "_", "_", "0");
        patch(falseJump, std::to_string(nextQuadIndex()));
        auto elseNode = std::make_unique<AstNode>("Else");
        elseNode->children.push_back(parseStatement());
        node->children.push_back(std::move(elseNode));
        patch(endJump, std::to_string(nextQuadIndex()));
    } else {
        patch(falseJump, std::to_string(nextQuadIndex()));
    }
    return node;
}

std::unique_ptr<Parser::AstNode> Parser::parseWhile() {
    auto node = std::make_unique<AstNode>("While");
    Token token = consumeKeyword("while", "expected 'while'");
    const int loopStart = nextQuadIndex();
    Expr condition = parseExpression();
    if (!isBoolean(condition.type)) {
        addDiagnostic(Severity::Error, "while condition must be bool", token.line, token.column);
    }
    node->children.push_back(std::move(condition.node));
    consumeKeyword("do", "expected 'do' after while condition");
    const int falseJump = emit("jfalse", condition.place, "_", "0");
    node->children.push_back(parseStatement());
    emit("j", "_", "_", std::to_string(loopStart));
    patch(falseJump, std::to_string(nextQuadIndex()));
    return node;
}

std::unique_ptr<Parser::AstNode> Parser::parseRead() {
    auto node = std::make_unique<AstNode>("Read");
    consumeKeyword("read", "expected 'read'");
    consumeDelimiter("(", "expected '(' after read");
    do {
        Token name = consume(TokenKind::Identifier, "expected identifier in read");
        Symbol *symbol = findSymbol(name.lexeme);
        if (!symbol) {
            addDiagnostic(Severity::Error, "undeclared identifier '" + name.lexeme + "'", name.line, name.column);
            continue;
        }
        std::string place = symbolPlace(*symbol);
        if (matchDelimiter("[")) {
            Expr index = parseExpression();
            consumeDelimiter("]", "expected ']' after array index");
            emit("readarr", "_", index.place, place);
        } else {
            emit("read", "_", "_", place);
        }
        node->children.push_back(std::make_unique<AstNode>(name.lexeme));
    } while (matchDelimiter(","));
    consumeDelimiter(")", "expected ')' after read arguments");
    return node;
}

std::unique_ptr<Parser::AstNode> Parser::parseWrite() {
    auto node = std::make_unique<AstNode>("Write");
    consumeKeyword("write", "expected 'write'");
    consumeDelimiter("(", "expected '(' after write");
    do {
        Expr expr = parseExpression();
        emit("write", expr.place, "_", "_");
        node->children.push_back(std::move(expr.node));
    } while (matchDelimiter(","));
    consumeDelimiter(")", "expected ')' after write arguments");
    return node;
}

Parser::Expr Parser::parseExpression() {
    return parseOr();
}

Parser::Expr Parser::parseOr() {
    Expr expr = parseAnd();
    while (matchKeyword("or")) {
        Token op = previous();
        Expr right = parseAnd();
        expr = makeBinary("or", std::move(expr), std::move(right), op.line, op.column);
    }
    return expr;
}

Parser::Expr Parser::parseAnd() {
    Expr expr = parseRelation();
    while (matchKeyword("and")) {
        Token op = previous();
        Expr right = parseRelation();
        expr = makeBinary("and", std::move(expr), std::move(right), op.line, op.column);
    }
    return expr;
}

Parser::Expr Parser::parseRelation() {
    Expr expr = parseAdditive();
    while (checkDelimiter("=") || checkDelimiter("<>") || checkDelimiter("<") ||
           checkDelimiter("<=") || checkDelimiter(">") || checkDelimiter(">=")) {
        Token op = advance();
        Expr right = parseAdditive();
        expr = makeBinary(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }
    return expr;
}

Parser::Expr Parser::parseAdditive() {
    Expr expr = parseMultiplicative();
    while (checkDelimiter("+") || checkDelimiter("-")) {
        Token op = advance();
        Expr right = parseMultiplicative();
        expr = makeBinary(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }
    return expr;
}

Parser::Expr Parser::parseMultiplicative() {
    Expr expr = parseUnary();
    while (checkDelimiter("*") || checkDelimiter("/")) {
        Token op = advance();
        Expr right = parseUnary();
        expr = makeBinary(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }
    return expr;
}

Parser::Expr Parser::parseUnary() {
    if (matchDelimiter("-")) {
        Token op = previous();
        Expr expr = parseUnary();
        return makeUnary("uminus", std::move(expr), op.line, op.column);
    }
    if (matchKeyword("not")) {
        Token op = previous();
        Expr expr = parseUnary();
        return makeUnary("not", std::move(expr), op.line, op.column);
    }
    return parsePrimary();
}

Parser::Expr Parser::parsePrimary() {
    if (match(TokenKind::Constant)) {
        Token token = previous();
        auto node = std::make_unique<AstNode>("const " + token.lexeme);
        return {"C" + std::to_string(token.index), token.valueType, std::move(node)};
    }
    if (match(TokenKind::Identifier)) {
        Token token = previous();
        auto node = std::make_unique<AstNode>("id " + token.lexeme);
        Symbol *symbol = findSymbol(token.lexeme);
        if (!symbol) {
            addDiagnostic(Severity::Error, "undeclared identifier '" + token.lexeme + "'",
                          token.line, token.column);
            return {token.lexeme, "error", std::move(node)};
        }
        std::string place = symbolPlace(*symbol);
        std::string type = baseTypeOf(*symbol);
        const int arrayLength = symbol->arrayLength;
        if (matchDelimiter("[")) {
            Expr index = parseExpression();
            node->children.push_back(std::move(index.node));
            consumeDelimiter("]", "expected ']' after array index");
            if (arrayLength == 0) {
                addDiagnostic(Severity::Error, "identifier '" + token.lexeme + "' is not an array",
                              token.line, token.column);
            }
            if (index.type != "integer") {
                addDiagnostic(Severity::Error, "array index must be integer", token.line, token.column);
            }
            std::string temp = newTemp(type);
            emit("=[]", place, index.place, temp);
            return {temp, type, std::move(node)};
        }
        if (arrayLength > 0) {
            addDiagnostic(Severity::Error, "array value requires an index", token.line, token.column);
        }
        return {place, type, std::move(node)};
    }
    if (matchDelimiter("(")) {
        Expr expr = parseExpression();
        consumeDelimiter(")", "expected ')' after expression");
        return expr;
    }

    addDiagnostic(Severity::Error, "expected expression", peek().line, peek().column);
    if (!isAtEnd()) {
        advance();
    }
    return {"_", "error", std::make_unique<AstNode>("ErrorExpression")};
}

Parser::Expr Parser::makeBinary(const std::string &op, Expr left, Expr right,
                                int line, int column) {
    std::string type = "error";
    if (op == "and" || op == "or") {
        if (!isBoolean(left.type) || !isBoolean(right.type)) {
            addDiagnostic(Severity::Error, "logical operator requires bool operands", line, column);
        }
        type = "bool";
    } else if (isComparison(op)) {
        if (!(isNumeric(left.type) && isNumeric(right.type)) && left.type != right.type) {
            addDiagnostic(Severity::Error, "comparison operands are not compatible", line, column);
        }
        type = "bool";
    } else {
        if (!isNumeric(left.type) || !isNumeric(right.type)) {
            addDiagnostic(Severity::Error, "arithmetic operator requires numeric operands", line, column);
        }
        type = (left.type == "real" || right.type == "real" || op == "/") ? "real" : "integer";
    }

    std::string temp = newTemp(type);
    emit(op, left.place, right.place, temp);
    auto node = std::make_unique<AstNode>("op " + op);
    node->children.push_back(std::move(left.node));
    node->children.push_back(std::move(right.node));
    return {temp, type, std::move(node)};
}

Parser::Expr Parser::makeUnary(const std::string &op, Expr expr, int line, int column) {
    std::string type = expr.type;
    if (op == "not" && !isBoolean(expr.type)) {
        addDiagnostic(Severity::Error, "'not' requires bool operand", line, column);
        type = "bool";
    }
    if (op == "uminus" && !isNumeric(expr.type)) {
        addDiagnostic(Severity::Error, "unary '-' requires numeric operand", line, column);
    }
    std::string temp = newTemp(type);
    emit(op, expr.place, "_", temp);
    auto node = std::make_unique<AstNode>("op " + op);
    node->children.push_back(std::move(expr.node));
    return {temp, type, std::move(node)};
}

Symbol *Parser::findSymbol(const std::string &name) {
    const auto found = symbolByName_.find(toLower(name));
    if (found == symbolByName_.end()) {
        return nullptr;
    }
    return &result_.symbols[found->second];
}

const Symbol *Parser::findSymbol(const std::string &name) const {
    const auto found = symbolByName_.find(toLower(name));
    if (found == symbolByName_.end()) {
        return nullptr;
    }
    return &result_.symbols[found->second];
}

Symbol *Parser::declareSymbol(const Token &token, const TypeInfo &type, const std::string &category) {
    if (token.kind != TokenKind::Identifier) {
        return nullptr;
    }
    const std::string key = toLower(token.lexeme);
    if (symbolByName_.count(key) > 0) {
        addDiagnostic(Severity::Error, "duplicate declaration of '" + token.lexeme + "'",
                      token.line, token.column);
        return findSymbol(token.lexeme);
    }
    Symbol symbol;
    symbol.index = category == "temp" ? nextSymbolIndex_++ : token.index;
    symbol.name = token.lexeme;
    symbol.type = type.name;
    symbol.category = category;
    symbol.address = category == "program" ? 0 : nextAddress_;
    symbol.length = type.length;
    symbol.arrayLength = type.arrayLength;
    symbol.level = 0;
    if (category != "program") {
        nextAddress_ += std::max(0, type.length);
    }
    symbolByName_[key] = result_.symbols.size();
    result_.symbols.push_back(symbol);
    return &result_.symbols.back();
}

std::string Parser::symbolPlace(const Symbol &symbol) const {
    if (symbol.category == "temp") {
        return symbol.name;
    }
    return "I" + std::to_string(symbol.index);
}

std::string Parser::newTemp(const std::string &type) {
    const std::string name = "t" + std::to_string(nextTempIndex_++);
    TypeInfo info{type, type, lengthOf(type), 0};
    Token token{TokenKind::Identifier, name, "i", nextSymbolIndex_, 0, 0, ""};
    declareSymbol(token, info, "temp");
    return name;
}

int Parser::emit(const std::string &op, const std::string &arg1,
                 const std::string &arg2, const std::string &result) {
    const int index = nextQuadIndex();
    result_.quads.push_back({index, op, arg1, arg2, result, 0});
    return index;
}

int Parser::nextQuadIndex() const {
    return static_cast<int>(result_.quads.size()) + 1;
}

void Parser::patch(int quadIndex, const std::string &result) {
    if (quadIndex <= 0 || quadIndex > static_cast<int>(result_.quads.size())) {
        return;
    }
    result_.quads[static_cast<size_t>(quadIndex - 1)].result = result;
}

bool Parser::isNumeric(const std::string &type) const {
    return type == "integer" || type == "real";
}

bool Parser::isBoolean(const std::string &type) const {
    return type == "bool";
}

bool Parser::canAssign(const std::string &target, const std::string &source) const {
    if (target == "error" || source == "error") {
        return true;
    }
    return target == source || (target == "real" && source == "integer");
}

std::string Parser::baseTypeOf(const Symbol &symbol) const {
    if (symbol.arrayLength == 0) {
        return symbol.type;
    }
    const std::string marker = " of ";
    const size_t pos = symbol.type.find(marker);
    return pos == std::string::npos ? symbol.type : symbol.type.substr(pos + marker.size());
}

int Parser::lengthOf(const std::string &type) const {
    if (type == "integer") {
        return 4;
    }
    if (type == "real") {
        return 8;
    }
    if (type == "char" || type == "bool") {
        return 1;
    }
    return 0;
}

void Parser::addDiagnostic(Severity severity, const std::string &message,
                           int line, int column) {
    result_.diagnostics.push_back({severity, "parser", message, line, column});
}

std::string Parser::renderAst(const AstNode &node, int depth) const {
    std::ostringstream out;
    out << std::string(static_cast<size_t>(depth * 2), ' ') << "- " << node.label << "\n";
    for (const auto &child : node.children) {
        if (child) {
            out << renderAst(*child, depth + 1);
        }
    }
    return out.str();
}

} // namespace minipas
