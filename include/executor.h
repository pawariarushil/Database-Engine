#pragma once
#include "ast.h"
#include "database.h"
#include <optional>
#include <string>
#include <vector>

struct QueryResult {
    bool isSelect = false;
    std::vector<std::string> columnNames;
    std::vector<Row> rows;
    std::string message;     // status message for non-SELECT statements
    bool usedIndex = false;  // true if the primary-key index served the query
};

// Walks a parsed Statement and carries it out against a Database: validates
// values against the table schema, evaluates WHERE clauses, and (when
// possible) short-circuits full scans via the primary key's B+Tree index.
class Executor {
public:
    explicit Executor(Database& db) : db_(db) {}
    QueryResult execute(Statement* stmt);

private:
    Database& db_;

    QueryResult executeCreateTable(CreateTableStmt* stmt);
    QueryResult executeInsert(InsertStmt* stmt);
    QueryResult executeSelect(SelectStmt* stmt);
    QueryResult executeDelete(DeleteStmt* stmt);
    QueryResult executeUpdate(UpdateStmt* stmt);

    Value coerceSingleValue(const Column& col, const Value& raw) const;
    Row coerceRow(const Schema& schema, const std::vector<Value>& raw) const;

    static int compareValues(const Value& a, const Value& b);
    bool evaluateCondition(const Schema& schema, const Row& row, const Condition& cond) const;
    bool evaluateWhere(const Schema& schema, const Row& row, const WhereClause& where) const;

    // Fast path: if WHERE is a single "pk = literal" condition, use the
    // B+Tree index instead of a full scan. Returns nullopt if not applicable.
    std::optional<std::vector<std::pair<RowId, Row>>> tryIndexScan(Table* table, const WhereClause& where) const;
};
