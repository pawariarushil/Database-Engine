#pragma once
#include "common.h"
#include "storage.h"
#include "bptree.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// A single table: schema + a page-based heap file (via DiskManager /
// BufferPoolManager) + an in-memory B+Tree index on the primary key
// (if one is declared). Row layout on disk is a fixed binary encoding
// derived from the schema (see serializeRow/deserializeRow in table.cpp).
class Table {
public:
    Table(std::string name, Schema schema, const std::string& filePath, size_t bufferPoolSize = 16);

    const Schema& schema() const { return schema_; }
    const std::string& name() const { return name_; }
    int32_t primaryKeyIndex() const { return pkColIdx_; }

    // Inserts a row (already type-checked/coerced against schema). Enforces
    // primary key uniqueness via the index. Returns the row's physical id.
    RowId insertRow(const Row& row);

    // Deletes the row at rid (tombstones the slot, removes it from the
    // index). Returns false if the slot was already empty/deleted.
    bool deleteRow(RowId rid);

    // Full heap scan: returns every live (non-deleted) row with its RowId.
    std::vector<std::pair<RowId, Row>> scanAll();

    // O(log n) point lookup via the primary key index. Returns nullopt if
    // there is no primary key on this table, or the key isn't present.
    std::optional<std::pair<RowId, Row>> lookupByPrimaryKey(int64_t key);

    void flush();

private:
    std::string name_;
    Schema schema_;
    std::string filePath_;
    std::unique_ptr<DiskManager> disk_;
    std::unique_ptr<BufferPoolManager> bufferPool_;
    std::unique_ptr<BPlusTree> index_;
    int32_t pkColIdx_ = -1;

    std::string serializeRow(const Row& row) const;
    Row deserializeRow(const std::string& bytes) const;
    void rebuildIndexFromDisk();
};
