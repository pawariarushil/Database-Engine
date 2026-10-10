#pragma once
#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <stdexcept>
#include <sstream>

// ---------------------------------------------------------------------------
// Core value / schema types shared across the engine
// ---------------------------------------------------------------------------

enum class ColumnType { INT, FLOAT, TEXT };

inline std::string columnTypeToString(ColumnType t) {
    switch (t) {
        case ColumnType::INT: return "INT";
        case ColumnType::FLOAT: return "FLOAT";
        case ColumnType::TEXT: return "TEXT";
    }
    return "?";
}

struct Column {
    std::string name;
    ColumnType type;
    bool isPrimaryKey = false;
};

struct Schema {
    std::vector<Column> columns;

    int indexOf(const std::string& name) const {
        for (size_t i = 0; i < columns.size(); ++i)
            if (columns[i].name == name) return static_cast<int>(i);
        return -1;
    }

    int primaryKeyIndex() const {
        for (size_t i = 0; i < columns.size(); ++i)
            if (columns[i].isPrimaryKey) return static_cast<int>(i);
        return -1;
    }
};

// A single cell value. NULLs are not supported (kept out of scope for size).
using Value = std::variant<int64_t, double, std::string>;

inline std::string valueToString(const Value& v) {
    if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<double>(v)) {
        std::ostringstream oss;
        oss << std::get<double>(v);
        return oss.str();
    }
    return std::get<std::string>(v);
}

using Row = std::vector<Value>;

// Physical location of a row: which page, which slot within that page.
struct RowId {
    int32_t pageId;
    int32_t slotId;
    bool operator==(const RowId& o) const { return pageId == o.pageId && slotId == o.slotId; }
};

// Comparison used by the B+Tree and WHERE-clause evaluation on int64 keys.
inline int64_t valueAsInt64(const Value& v) {
    if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
    if (std::holds_alternative<double>(v)) return static_cast<int64_t>(std::get<double>(v));
    throw std::runtime_error("Value is not numeric, cannot use as indexed key");
}

class DbException : public std::runtime_error {
public:
    explicit DbException(const std::string& msg) : std::runtime_error(msg) {}
};
