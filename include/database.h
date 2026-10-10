#pragma once
#include "table.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Owns every Table and persists the schema catalog so the database can be
// reopened later with all tables and their data intact. The catalog is a
// small plain-text file (one line per table); the actual row data lives in
// per-table page files managed by Table/DiskManager.
class Database {
public:
    explicit Database(const std::string& dataDir);
    ~Database();

    void createTable(const std::string& name, const std::vector<Column>& columns);
    Table* getTable(const std::string& name);
    bool hasTable(const std::string& name) const;
    std::vector<std::string> tableNames() const;

private:
    std::string dataDir_;
    std::string catalogPath_;
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;

    void loadCatalog();
    void appendCatalogEntry(const std::string& name, const std::vector<Column>& columns);
    std::string tableFilePath(const std::string& name) const;
};
