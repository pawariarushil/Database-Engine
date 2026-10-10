#include "lexer.h"
#include "common.h"
#include <cctype>

Lexer::Lexer(const std::string& src) : src_(src) {}

bool Lexer::isAtEnd() const { return i_ >= src_.size(); }
char Lexer::peek() const { return isAtEnd() ? '\0' : src_[i_]; }
char Lexer::advance() { return src_[i_++]; }

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '-' && i_ + 1 < src_.size() && src_[i_ + 1] == '-') {
            // -- line comment
            while (!isAtEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::lexNumber() {
    int start = static_cast<int>(i_);
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (!isAtEnd() && peek() == '.') {
        advance();
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    return { TokenType::NUMBER, src_.substr(start, i_ - start), start };
}

Token Lexer::lexString() {
    int start = static_cast<int>(i_);
    advance(); // opening quote
    std::string out;
    while (!isAtEnd() && peek() != '\'') {
        out += advance();
    }
    if (isAtEnd()) throw DbException("Unterminated string literal starting at " + std::to_string(start));
    advance(); // closing quote
    return { TokenType::STRING, out, start };
}

Token Lexer::lexIdent() {
    int start = static_cast<int>(i_);
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) advance();
    return { TokenType::IDENT, src_.substr(start, i_ - start), start };
}

Token Lexer::lexSymbol() {
    int start = static_cast<int>(i_);
    char c = advance();
    // two-character operators
    if ((c == '<' || c == '>' || c == '!' || c == '=') && !isAtEnd() && peek() == '=') {
        advance();
        return { TokenType::SYMBOL, std::string(1, c) + "=", start };
    }
    return { TokenType::SYMBOL, std::string(1, c), start };
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (isAtEnd()) break;
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
        } else if (c == '\'') {
            tokens.push_back(lexString());
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdent());
        } else if (std::string("(),;*=<>!.").find(c) != std::string::npos) {
            tokens.push_back(lexSymbol());
        } else {
            throw DbException(std::string("Unexpected character '") + c + "' at position " + std::to_string(i_));
        }
    }
    tokens.push_back({ TokenType::END, "", static_cast<int>(src_.size()) });
    return tokens;
}
