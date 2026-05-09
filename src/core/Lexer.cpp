#include "core/Lexer.h"

#include <cctype>
#include <sstream>

namespace minipas {
namespace {

const std::vector<TableEntry> kKeywords = {
    {1, "program", "keyword"}, {2, "var", "keyword"},
    {3, "integer", "keyword"}, {4, "real", "keyword"},
    {5, "char", "keyword"}, {6, "begin", "keyword"},
    {7, "end", "keyword"}, {8, "bool", "keyword"},
    {9, "array", "keyword"}, {10, "of", "keyword"},
    {11, "if", "keyword"}, {12, "then", "keyword"},
    {13, "else", "keyword"}, {14, "while", "keyword"},
    {15, "do", "keyword"}, {16, "read", "keyword"},
    {17, "write", "keyword"}, {18, "and", "keyword"},
    {19, "or", "keyword"}, {20, "not", "keyword"}
};

const std::vector<TableEntry> kDelimiters = {
    {1, ",", "delimiter"}, {2, ":", "delimiter"},
    {3, ";", "delimiter"}, {4, ":=", "delimiter"},
    {5, "*", "delimiter"}, {6, "/", "delimiter"},
    {7, "+", "delimiter"}, {8, "-", "delimiter"},
    {9, ".", "delimiter"}, {10, "(", "delimiter"},
    {11, ")", "delimiter"}, {12, "[", "delimiter"},
    {13, "]", "delimiter"}, {14, "=", "delimiter"},
    {15, "<>", "delimiter"}, {16, "<", "delimiter"},
    {17, "<=", "delimiter"}, {18, ">", "delimiter"},
    {19, ">=", "delimiter"}
};

std::map<std::string, int> toMap(const std::vector<TableEntry> &entries) {
    std::map<std::string, int> map;
    for (const auto &entry : entries) {
        map[entry.name] = entry.index;
    }
    return map;
}

std::string lower(std::string text) {
    for (char &ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierPart(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<TableEntry> Lexer::keywordTable() {
    return kKeywords;
}

std::vector<TableEntry> Lexer::delimiterTable() {
    return kDelimiters;
}

LexResult Lexer::scan() {
    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            break;
        }

        const char ch = peek();
        if (isIdentifierStart(ch)) {
            result_.tokens.push_back(scanIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            result_.tokens.push_back(scanNumber());
        } else if (ch == '"') {
            result_.tokens.push_back(scanString());
        } else if (ch == '\'') {
            result_.tokens.push_back(scanChar());
        } else {
            result_.tokens.push_back(scanDelimiter());
        }
    }

    result_.tokens.push_back(makeToken(TokenKind::End, "$", "$", 0, line_, column_));
    return result_;
}

char Lexer::peek(int offset) const {
    const size_t index = pos_ + static_cast<size_t>(offset);
    if (index >= source_.size()) {
        return '\0';
    }
    return source_[index];
}

char Lexer::advance() {
    if (isAtEnd()) {
        return '\0';
    }
    const char ch = source_[pos_++];
    if (ch == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return ch;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespaceAndComments() {
    bool changed = true;
    while (changed && !isAtEnd()) {
        changed = false;
        while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
            changed = true;
        }
        if (peek() == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            changed = true;
        }
        if (peek() == '{') {
            const int startLine = line_;
            const int startColumn = column_;
            advance();
            while (!isAtEnd() && peek() != '}') {
                advance();
            }
            if (isAtEnd()) {
                addDiagnostic(Severity::Error, "unterminated block comment", startLine, startColumn);
            } else {
                advance();
            }
            changed = true;
        }
    }
}

void Lexer::addDiagnostic(Severity severity, const std::string &message, int line, int column) {
    result_.diagnostics.push_back({severity, "lexer", message, line, column});
}

Token Lexer::makeToken(TokenKind kind, const std::string &lexeme, const std::string &table,
                       int index, int line, int column, const std::string &valueType) {
    return {kind, lexeme, table, index, line, column, valueType};
}

Token Lexer::scanIdentifierOrKeyword() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;
    while (isIdentifierPart(peek())) {
        lexeme.push_back(advance());
    }

    const std::string normalized = lower(lexeme);
    if (normalized == "true" || normalized == "false") {
        const int idx = constantIndex(normalized, "bool", normalized);
        return makeToken(TokenKind::Constant, normalized, "c", idx, startLine, startColumn, "bool");
    }

    const auto keywords = toMap(kKeywords);
    const auto found = keywords.find(normalized);
    if (found != keywords.end()) {
        return makeToken(TokenKind::Keyword, normalized, "k", found->second, startLine, startColumn);
    }

    const int idx = identifierIndex(lexeme);
    return makeToken(TokenKind::Identifier, lexeme, "i", idx, startLine, startColumn);
}

Token Lexer::scanNumber() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        lexeme.push_back(advance());
    }

    bool isReal = false;
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        isReal = true;
        lexeme.push_back(advance());
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            lexeme.push_back(advance());
        }
    }

    const std::string type = isReal ? "real" : "integer";
    const int idx = constantIndex(lexeme, type, lexeme);
    return makeToken(TokenKind::Constant, lexeme, "c", idx, startLine, startColumn, type);
}

Token Lexer::scanString() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;
    std::string value;
    lexeme.push_back(advance());
    while (!isAtEnd() && peek() != '"') {
        char ch = advance();
        if (ch == '\\' && !isAtEnd()) {
            char escaped = advance();
            lexeme.push_back('\\');
            lexeme.push_back(escaped);
            value.push_back(escaped == 'n' ? '\n' : escaped);
        } else {
            lexeme.push_back(ch);
            value.push_back(ch);
        }
    }
    if (isAtEnd()) {
        addDiagnostic(Severity::Error, "unterminated string literal", startLine, startColumn);
    } else {
        lexeme.push_back(advance());
    }
    const int idx = constantIndex(lexeme, "string", value);
    return makeToken(TokenKind::Constant, lexeme, "c", idx, startLine, startColumn, "string");
}

Token Lexer::scanChar() {
    const int startLine = line_;
    const int startColumn = column_;
    std::string lexeme;
    std::string value;
    lexeme.push_back(advance());
    if (isAtEnd()) {
        addDiagnostic(Severity::Error, "unterminated char literal", startLine, startColumn);
    } else {
        char ch = advance();
        if (ch == '\\' && !isAtEnd()) {
            char escaped = advance();
            lexeme.push_back('\\');
            lexeme.push_back(escaped);
            value.push_back(escaped == 'n' ? '\n' : escaped);
        } else {
            lexeme.push_back(ch);
            value.push_back(ch);
        }
    }
    if (peek() == '\'') {
        lexeme.push_back(advance());
    } else {
        addDiagnostic(Severity::Error, "char literal must contain exactly one character", startLine, startColumn);
    }
    const int idx = constantIndex(lexeme, "char", value);
    return makeToken(TokenKind::Constant, lexeme, "c", idx, startLine, startColumn, "char");
}

Token Lexer::scanDelimiter() {
    const int startLine = line_;
    const int startColumn = column_;
    const auto delimiters = toMap(kDelimiters);

    std::string two;
    two.push_back(peek());
    two.push_back(peek(1));
    const auto twoFound = delimiters.find(two);
    if (twoFound != delimiters.end()) {
        advance();
        advance();
        return makeToken(TokenKind::Delimiter, two, "p", twoFound->second, startLine, startColumn);
    }

    std::string one(1, advance());
    const auto oneFound = delimiters.find(one);
    if (oneFound != delimiters.end()) {
        return makeToken(TokenKind::Delimiter, one, "p", oneFound->second, startLine, startColumn);
    }

    std::ostringstream message;
    message << "unknown character '" << one << "'";
    addDiagnostic(Severity::Error, message.str(), startLine, startColumn);
    return makeToken(TokenKind::Invalid, one, "?", 0, startLine, startColumn);
}

int Lexer::identifierIndex(const std::string &name) {
    const auto found = identifierMap_.find(name);
    if (found != identifierMap_.end()) {
        return found->second;
    }
    const int index = static_cast<int>(identifierMap_.size()) + 1;
    identifierMap_[name] = index;
    result_.identifiers.push_back({index, name, "identifier"});
    return index;
}

int Lexer::constantIndex(const std::string &literal, const std::string &type, const std::string &value) {
    const std::string key = type + ":" + literal;
    const auto found = constantMap_.find(key);
    if (found != constantMap_.end()) {
        return found->second;
    }
    const int index = static_cast<int>(constantMap_.size()) + 1;
    constantMap_[key] = index;
    result_.constants.push_back({index, literal, type, value});
    return index;
}

} // namespace minipas
