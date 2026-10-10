#include "parser.h"
#include "common.h"
#include <algorithm>
#include <cctype>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

std::string Parser::upper(const std::string& s) const {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::toupper(c); });
    return r;
}

const Token& Parser::peek() const { return tokens_[pos_]; }
const Token& Parser::advance() { return tokens_[pos_++]; }

bool Parser::check(TokenType t, const std::string& text) const {
    if (peek().type != t) return false;
    if (!text.empty() && upper(peek().text) != upper(text)) return false;
    return true;
}

bool Parser::matchKeyword(const std::string& kw) {
    if (peek().type == TokenType::IDENT && upper(peek().text) == upper(kw)) {
        advance();
        return true;
    }
    return false;
}

void Parser::expectSymbol(const std::string& sym) {
    if (!check(TokenType::SYMBOL, sym)) {
        throw DbException("Expected '" + sym + "' but got '" + peek().text + "' at position " + std::to_string(peek().pos));
    }
    advance();
}

Token Parser::expectIdent() {
    if (peek().type != TokenType::IDENT) {
        throw DbException("Expected identifier but got '" + peek().text + "' at position " + std::to_string(peek().pos));
    }
    return advance();
}

ColumnType Parser::parseColumnType() {
    Token t = expectIdent();
    std::string u = upper(t.text);
    if (u == "INT" || u == "INTEGER") return ColumnType::INT;
    if (u == "FLOAT" || u == "DOUBLE" || u == "REAL") return ColumnType::FLOAT;
    if (u == "TEXT" || u == "VARCHAR" || u == "STRING" || u == "CHAR") return ColumnType::TEXT;
    throw DbException("Unknown column type '" + t.text + "'");
}

Value Parser::parseLiteral() {
    if (peek().type == TokenType::NUMBER) {
        std::string text = advance().text;
        if (text.find('.') != std::string::npos) {
            return Value(std::stod(text));
        }
        return Value(static_cast<int64_t>(std::stoll(text)));
    }
    if (peek().type == TokenType::STRING) {
        return Value(advance().text);
    }
    throw DbException("Expected literal value but got '" + peek().text + "'");
}

std::unique_ptr<Statement> Parser::parseStatement() {
    if (matchKeyword("CREATE")) return parseCreateTable();
    if (matchKeyword("INSERT")) return parseInsert();
    if (matchKeyword("SELECT")) return parseSelect();
    if (matchKeyword("DELETE")) return parseDelete();
    if (matchKeyword("UPDATE")) return parseUpdate();
    throw DbException("Unrecognized statement starting with '" + peek().text + "'");
}

std::unique_ptr<Statement> Parser::parseCreateTable() {
    if (!matchKeyword("TABLE")) throw DbException("Expected TABLE after CREATE");
    auto stmt = std::make_unique<CreateTableStmt>();
    stmt->tableName = expectIdent().text;
    expectSymbol("(");
    bool first = true;
    while (!check(TokenType::SYMBOL, ")")) {
        if (!first) expectSymbol(",");
        first = false;
        Column col;
        col.name = expectIdent().text;
        col.type = parseColumnType();
        if (matchKeyword("PRIMARY")) {
            if (!matchKeyword("KEY")) throw DbException("Expected KEY after PRIMARY");
            col.isPrimaryKey = true;
        }
        stmt->columns.push_back(col);
    }
    expectSymbol(")");
    if (check(TokenType::SYMBOL, ";")) advance();
    return stmt;
}

std::unique_ptr<Statement> Parser::parseInsert() {
    if (!matchKeyword("INTO")) throw DbException("Expected INTO after INSERT");
    auto stmt = std::make_unique<InsertStmt>();
    stmt->tableName = expectIdent().text;
    if (!matchKeyword("VALUES")) throw DbException("Expected VALUES");
    expectSymbol("(");
    bool first = true;
    while (!check(TokenType::SYMBOL, ")")) {
        if (!first) expectSymbol(",");
        first = false;
        stmt->values.push_back(parseLiteral());
    }
    expectSymbol(")");
    if (check(TokenType::SYMBOL, ";")) advance();
    return stmt;
}

Condition Parser::parseCondition() {
    Condition cond;
    cond.column = expectIdent().text;
    if (!check(TokenType::SYMBOL)) throw DbException("Expected comparison operator");
    std::string op = advance().text;
    if (op == "=") cond.op = CompareOp::EQ;
    else if (op == "!=") cond.op = CompareOp::NEQ;
    else if (op == "<") cond.op = CompareOp::LT;
    else if (op == "<=") cond.op = CompareOp::LTE;
    else if (op == ">") cond.op = CompareOp::GT;
    else if (op == ">=") cond.op = CompareOp::GTE;
    else throw DbException("Unknown comparison operator '" + op + "'");
    cond.literal = parseLiteral();
    return cond;
}

WhereClause Parser::parseWhereClause() {
    WhereClause where;
    where.conditions.push_back(parseCondition());
    while (true) {
        if (matchKeyword("AND")) {
            where.joins.push_back(LogicOp::AND);
            where.conditions.push_back(parseCondition());
        } else if (matchKeyword("OR")) {
            where.joins.push_back(LogicOp::OR);
            where.conditions.push_back(parseCondition());
        } else {
            break;
        }
    }
    return where;
}

std::unique_ptr<Statement> Parser::parseSelect() {
    auto stmt = std::make_unique<SelectStmt>();
    if (check(TokenType::SYMBOL, "*")) {
        advance(); // SELECT * -> empty columns means "all"
    } else {
        stmt->columns.push_back(expectIdent().text);
        while (check(TokenType::SYMBOL, ",")) {
            advance();
            stmt->columns.push_back(expectIdent().text);
        }
    }
    if (!matchKeyword("FROM")) throw DbException("Expected FROM in SELECT");
    stmt->tableName = expectIdent().text;
    if (matchKeyword("WHERE")) {
        stmt->where = parseWhereClause();
    }
    if (check(TokenType::SYMBOL, ";")) advance();
    return stmt;
}

std::unique_ptr<Statement> Parser::parseDelete() {
    if (!matchKeyword("FROM")) throw DbException("Expected FROM after DELETE");
    auto stmt = std::make_unique<DeleteStmt>();
    stmt->tableName = expectIdent().text;
    if (matchKeyword("WHERE")) {
        stmt->where = parseWhereClause();
    }
    if (check(TokenType::SYMBOL, ";")) advance();
    return stmt;
}

std::unique_ptr<Statement> Parser::parseUpdate() {
    auto stmt = std::make_unique<UpdateStmt>();
    stmt->tableName = expectIdent().text;
    if (!matchKeyword("SET")) throw DbException("Expected SET in UPDATE");
    bool first = true;
    while (true) {
        if (!first) {
            if (!check(TokenType::SYMBOL, ",")) break;
            advance();
        }
        first = false;
        UpdateAssignment a;
        a.column = expectIdent().text;
        expectSymbol("=");
        a.value = parseLiteral();
        stmt->assignments.push_back(a);
        if (!check(TokenType::SYMBOL, ",")) break;
    }
    if (matchKeyword("WHERE")) {
        stmt->where = parseWhereClause();
    }
    if (check(TokenType::SYMBOL, ";")) advance();
    return stmt;
}
