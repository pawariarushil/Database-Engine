#pragma once
#include "ast.h"
#include "lexer.h"
#include <memory>
#include <vector>

// Recursive-descent parser for a small SQL subset:
//   CREATE TABLE name (col TYPE [PRIMARY KEY], ...)
//   INSERT INTO name VALUES (v1, v2, ...)
//   SELECT col1, col2 | * FROM name [WHERE cond [AND|OR cond]*]
//   DELETE FROM name [WHERE ...]
//   UPDATE name SET col=val [, col=val]* [WHERE ...]
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<Statement> parseStatement();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    const Token& peek() const;
    const Token& advance();
    bool check(TokenType t, const std::string& text = "") const;
    bool matchKeyword(const std::string& kw);   // case-insensitive keyword match, consumes if matched
    void expectSymbol(const std::string& sym);
    Token expectIdent();
    std::string upper(const std::string& s) const;

    std::unique_ptr<Statement> parseCreateTable();
    std::unique_ptr<Statement> parseInsert();
    std::unique_ptr<Statement> parseSelect();
    std::unique_ptr<Statement> parseDelete();
    std::unique_ptr<Statement> parseUpdate();

    WhereClause parseWhereClause();
    Condition parseCondition();
    Value parseLiteral();
    ColumnType parseColumnType();
};
