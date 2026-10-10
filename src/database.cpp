#include "database.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

Database::Database(const std::string& dataDir) : dataDir_(dataDir) {
    fs::create_directories(dataDir_);
    catalogPath_ = dataDir_ + "/catalog.txt";
    loadCatalog();
}

Database::~Database() {
    for (auto& [name, table] : tables_) table->flush();
}

std::string Database::tableFilePath(const std::string& name) const {
    return dataDir_ + "/" + name + ".tbl";
}

// Catalog line format:
//   table_name|col1:TYPE:PK,col2:TYPE,col3:TYPE
static std::string columnTypeCode(ColumnType t) {
    switch (t) {
        case ColumnType::INT: return "INT";
        case ColumnType::FLOAT: return "FLOAT";
        case ColumnType::TEXT: return "TEXT";
    }
    return "TEXT";
}

static ColumnType parseColumnTypeCode(const std::string& s) {
    if (s == "INT") return ColumnType::INT;
    if (s == "FLOAT") return ColumnType::FLOAT;
    return ColumnType::TEXT;
}

void Database::appendCatalogEntry(const std::string& name, const std::vector<Column>& columns) {
    std::ofstream out(catalogPath_, std::ios::app);
    out << name << "|";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i) out << ",";
        out << columns[i].name << ":" << columnTypeCode(columns[i].type);
        if (columns[i].isPrimaryKey) out << ":PK";
    }
    out << "\n";
}

void Database::loadCatalog() {
    std::ifstream in(catalogPath_);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        size_t bar = line.find('|');
        if (bar == std::string::npos) continue;
        std::string name = line.substr(0, bar);
        std::string rest = line.substr(bar + 1);

        Schema schema;
        std::istringstream colsStream(rest);
        std::string colDef;
        while (std::getline(colsStream, colDef, ',')) {
            std::istringstream fieldStream(colDef);
            std::string colName, colType, pkFlag;
            std::getline(fieldStream, colName, ':');
            std::getline(fieldStream, colType, ':');
            std::getline(fieldStream, pkFlag, ':');
            Column c;
            c.name = colName;
            c.type = parseColumnTypeCode(colType);
            c.isPrimaryKey = (pkFlag == "PK");
            schema.columns.push_back(c);
        }
        tables_[name] = std::make_unique<Table>(name, schema, tableFilePath(name));
    }
}

void Database::createTable(const std::string& name, const std::vector<Column>& columns) {
    if (hasTable(name)) throw DbException("Table '" + name + "' already exists");
    if (columns.empty()) throw DbException("CREATE TABLE requires at least one column");
    Schema schema;
    schema.columns = columns;
    auto table = std::make_unique<Table>(name, schema, tableFilePath(name));
    tables_[name] = std::move(table);
    appendCatalogEntry(name, columns);
}

Table* Database::getTable(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) throw DbException("Table '" + name + "' does not exist");
    return it->second.get();
}

bool Database::hasTable(const std::string& name) const {
    return tables_.find(name) != tables_.end();
}

std::vector<std::string> Database::tableNames() const {
    std::vector<std::string> names;
    for (auto& [name, table] : tables_) names.push_back(name);
    return names;
}
