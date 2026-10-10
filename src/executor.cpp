#include "executor.h"
#include <algorithm>

QueryResult Executor::execute(Statement* stmt) {
    switch (stmt->type) {
        case StmtType::CREATE_TABLE: return executeCreateTable(static_cast<CreateTableStmt*>(stmt));
        case StmtType::INSERT:       return executeInsert(static_cast<InsertStmt*>(stmt));
        case StmtType::SELECT:       return executeSelect(static_cast<SelectStmt*>(stmt));
        case StmtType::DELETE:       return executeDelete(static_cast<DeleteStmt*>(stmt));
        case StmtType::UPDATE:       return executeUpdate(static_cast<UpdateStmt*>(stmt));
    }
    throw DbException("Unknown statement type");
}

Value Executor::coerceSingleValue(const Column& col, const Value& raw) const {
    switch (col.type) {
        case ColumnType::INT:
            if (!std::holds_alternative<int64_t>(raw))
                throw DbException("Type mismatch: column '" + col.name + "' expects INT");
            return raw;
        case ColumnType::FLOAT:
            if (std::holds_alternative<int64_t>(raw)) return Value(static_cast<double>(std::get<int64_t>(raw)));
            if (std::holds_alternative<double>(raw)) return raw;
            throw DbException("Type mismatch: column '" + col.name + "' expects FLOAT");
        case ColumnType::TEXT:
            if (!std::holds_alternative<std::string>(raw))
                throw DbException("Type mismatch: column '" + col.name + "' expects TEXT");
            return raw;
    }
    throw DbException("Unknown column type");
}

Row Executor::coerceRow(const Schema& schema, const std::vector<Value>& raw) const {
    if (raw.size() != schema.columns.size())
        throw DbException("Expected " + std::to_string(schema.columns.size()) + " values, got " +
                           std::to_string(raw.size()));
    Row row;
    row.reserve(raw.size());
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        row.push_back(coerceSingleValue(schema.columns[i], raw[i]));
    }
    return row;
}

int Executor::compareValues(const Value& a, const Value& b) {
    if (std::holds_alternative<std::string>(a) || std::holds_alternative<std::string>(b)) {
        std::string as = std::holds_alternative<std::string>(a) ? std::get<std::string>(a) : valueToString(a);
        std::string bs = std::holds_alternative<std::string>(b) ? std::get<std::string>(b) : valueToString(b);
        if (as < bs) return -1;
        if (as > bs) return 1;
        return 0;
    }
    double ad = std::holds_alternative<int64_t>(a) ? static_cast<double>(std::get<int64_t>(a)) : std::get<double>(a);
    double bd = std::holds_alternative<int64_t>(b) ? static_cast<double>(std::get<int64_t>(b)) : std::get<double>(b);
    if (ad < bd) return -1;
    if (ad > bd) return 1;
    return 0;
}

bool Executor::evaluateCondition(const Schema& schema, const Row& row, const Condition& cond) const {
    int idx = schema.indexOf(cond.column);
    if (idx < 0) throw DbException("Unknown column '" + cond.column + "' in WHERE clause");
    int cmp = compareValues(row[idx], cond.literal);
    switch (cond.op) {
        case CompareOp::EQ:  return cmp == 0;
        case CompareOp::NEQ: return cmp != 0;
        case CompareOp::LT:  return cmp < 0;
        case CompareOp::LTE: return cmp <= 0;
        case CompareOp::GT:  return cmp > 0;
        case CompareOp::GTE: return cmp >= 0;
    }
    return false;
}

bool Executor::evaluateWhere(const Schema& schema, const Row& row, const WhereClause& where) const {
    if (where.empty()) return true;
    bool result = evaluateCondition(schema, row, where.conditions[0]);
    for (size_t i = 1; i < where.conditions.size(); ++i) {
        bool next = evaluateCondition(schema, row, where.conditions[i]);
        LogicOp op = where.joins[i - 1];
        result = (op == LogicOp::AND) ? (result && next) : (result || next);
    }
    return result;
}

std::optional<std::vector<std::pair<RowId, Row>>> Executor::tryIndexScan(Table* table, const WhereClause& where) const {
    int pkIdx = table->primaryKeyIndex();
    if (pkIdx < 0) return std::nullopt;
    if (where.conditions.size() != 1) return std::nullopt; // only single-condition equality qualifies
    const Condition& c = where.conditions[0];
    if (c.op != CompareOp::EQ) return std::nullopt;
    if (c.column != table->schema().columns[pkIdx].name) return std::nullopt;

    int64_t key;
    try {
        key = valueAsInt64(c.literal);
    } catch (...) {
        return std::nullopt;
    }
    std::vector<std::pair<RowId, Row>> out;
    auto found = table->lookupByPrimaryKey(key);
    if (found) out.push_back(*found);
    return out;
}

QueryResult Executor::executeCreateTable(CreateTableStmt* stmt) {
    db_.createTable(stmt->tableName, stmt->columns);
    QueryResult r;
    r.message = "Table '" + stmt->tableName + "' created (" + std::to_string(stmt->columns.size()) + " columns).";
    return r;
}

QueryResult Executor::executeInsert(InsertStmt* stmt) {
    Table* table = db_.getTable(stmt->tableName);
    Row row = coerceRow(table->schema(), stmt->values);
    RowId rid = table->insertRow(row);
    QueryResult r;
    r.message = "1 row inserted into '" + stmt->tableName + "' (page " + std::to_string(rid.pageId) +
                ", slot " + std::to_string(rid.slotId) + ").";
    return r;
}

QueryResult Executor::executeSelect(SelectStmt* stmt) {
    Table* table = db_.getTable(stmt->tableName);
    const Schema& schema = table->schema();

    std::vector<int> projectIdx;
    std::vector<std::string> colNames;
    if (stmt->columns.empty()) {
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            projectIdx.push_back(static_cast<int>(i));
            colNames.push_back(schema.columns[i].name);
        }
    } else {
        for (auto& name : stmt->columns) {
            int idx = schema.indexOf(name);
            if (idx < 0) throw DbException("Unknown column '" + name + "' in SELECT list");
            projectIdx.push_back(idx);
            colNames.push_back(name);
        }
    }

    QueryResult r;
    r.isSelect = true;
    r.columnNames = colNames;

    auto indexResult = tryIndexScan(table, stmt->where);
    std::vector<std::pair<RowId, Row>> candidates;
    if (indexResult) {
        r.usedIndex = true;
        candidates = std::move(*indexResult);
    } else {
        candidates = table->scanAll();
    }

    for (auto& [rid, row] : candidates) {
        if (!evaluateWhere(schema, row, stmt->where)) continue;
        Row projected;
        projected.reserve(projectIdx.size());
        for (int idx : projectIdx) projected.push_back(row[idx]);
        r.rows.push_back(std::move(projected));
    }
    r.message = std::to_string(r.rows.size()) + " row(s) returned.";
    return r;
}

QueryResult Executor::executeDelete(DeleteStmt* stmt) {
    Table* table = db_.getTable(stmt->tableName);
    const Schema& schema = table->schema();

    auto indexResult = tryIndexScan(table, stmt->where);
    std::vector<std::pair<RowId, Row>> candidates = indexResult ? std::move(*indexResult) : table->scanAll();

    int deleted = 0;
    for (auto& [rid, row] : candidates) {
        if (!evaluateWhere(schema, row, stmt->where)) continue;
        if (table->deleteRow(rid)) deleted++;
    }
    QueryResult r;
    r.message = std::to_string(deleted) + " row(s) deleted from '" + stmt->tableName + "'.";
    return r;
}

QueryResult Executor::executeUpdate(UpdateStmt* stmt) {
    Table* table = db_.getTable(stmt->tableName);
    const Schema& schema = table->schema();

    auto indexResult = tryIndexScan(table, stmt->where);
    std::vector<std::pair<RowId, Row>> candidates = indexResult ? std::move(*indexResult) : table->scanAll();

    int updated = 0;
    for (auto& [rid, row] : candidates) {
        if (!evaluateWhere(schema, row, stmt->where)) continue;
        Row newRow = row;
        for (auto& assign : stmt->assignments) {
            int idx = schema.indexOf(assign.column);
            if (idx < 0) throw DbException("Unknown column '" + assign.column + "' in SET clause");
            newRow[idx] = coerceSingleValue(schema.columns[idx], assign.value);
        }
        table->deleteRow(rid);      // physically relocate: simplest correct way to
        table->insertRow(newRow);   // handle size changes & primary-key index updates
        updated++;
    }
    QueryResult r;
    r.message = std::to_string(updated) + " row(s) updated in '" + stmt->tableName + "'.";
    return r;
}
