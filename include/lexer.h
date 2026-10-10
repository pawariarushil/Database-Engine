#pragma once
#include <string>
#include <vector>

enum class TokenType { IDENT, NUMBER, STRING, SYMBOL, END };

struct Token {
    TokenType type;
    std::string text;   // raw text (numbers/strings keep original text too)
    int pos;             // position in source, for error messages
};

// A hand-written scanner for a small SQL dialect. Produces a flat token
// stream; keyword recognition is left to the parser (keeps the lexer dumb
// and reusable).
class Lexer {
public:
    explicit Lexer(const std::string& src);
    std::vector<Token> tokenize();

private:
    const std::string& src_;
    size_t i_ = 0;

    char peek() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespaceAndComments();

    Token lexNumber();
    Token lexString();
    Token lexIdent();
    Token lexSymbol();
};
