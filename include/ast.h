#pragma once
#include "common.h"
#include <memory>
#include <string>
#include <vector>

enum class StmtType { CREATE_TABLE, INSERT, SELECT, DELETE, UPDATE };

enum class CompareOp { EQ, NEQ, LT, LTE, GT, GTE };
enum class LogicOp { AND, OR, NONE };

// A single "column OP literal" comparison, e.g. age > 21
struct Condition {
    std::string column;
    CompareOp op;
    Value literal;
};

// WHERE clause modeled as a flat left-to-right chain of comparisons joined by
// AND/OR (no parentheses/precedence — deliberately simple, documented in README).
struct WhereClause {
    std::vector<Condition> conditions;
    std::vector<LogicOp> joins; // joins.size() == conditions.size() - 1

    bool empty() const { return conditions.empty(); }
};

struct Statement {
    StmtType type;
    virtual ~Statement() = default;
};

struct CreateTableStmt : Statement {
    CreateTableStmt() { type = StmtType::CREATE_TABLE; }
    std::string tableName;
    std::vector<Column> columns;
};

struct InsertStmt : Statement {
    InsertStmt() { type = StmtType::INSERT; }
    std::string tableName;
    std::vector<Value> values;
};

struct SelectStmt : Statement {
    SelectStmt() { type = StmtType::SELECT; }
    std::string tableName;
    std::vector<std::string> columns; // empty => SELECT *
    WhereClause where;
};

struct DeleteStmt : Statement {
    DeleteStmt() { type = StmtType::DELETE; }
    std::string tableName;
    WhereClause where;
};

struct UpdateAssignment {
    std::string column;
    Value value;
};

struct UpdateStmt : Statement {
    UpdateStmt() { type = StmtType::UPDATE; }
    std::string tableName;
    std::vector<UpdateAssignment> assignments;
    WhereClause where;
};
