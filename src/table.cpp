#include "table.h"
#include <cstring>

Table::Table(std::string name, Schema schema, const std::string& filePath, size_t bufferPoolSize)
    : name_(std::move(name)), schema_(std::move(schema)), filePath_(filePath) {
    disk_ = std::make_unique<DiskManager>(filePath_);
    bufferPool_ = std::make_unique<BufferPoolManager>(disk_.get(), bufferPoolSize);
    pkColIdx_ = schema_.primaryKeyIndex();
    index_ = std::make_unique<BPlusTree>(4);
    if (pkColIdx_ >= 0 && disk_->numPages() > 0) {
        rebuildIndexFromDisk();
    }
}

void Table::rebuildIndexFromDisk() {
    for (auto& [rid, row] : scanAll()) {
        int64_t key = valueAsInt64(row[pkColIdx_]);
        index_->insert(key, rid);
    }
}

std::string Table::serializeRow(const Row& row) const {
    if (row.size() != schema_.columns.size())
        throw DbException("Row has " + std::to_string(row.size()) + " values but table '" +
                           name_ + "' has " + std::to_string(schema_.columns.size()) + " columns");
    std::string out;
    for (size_t i = 0; i < schema_.columns.size(); ++i) {
        const Column& col = schema_.columns[i];
        const Value& v = row[i];
        switch (col.type) {
            case ColumnType::INT: {
                int64_t iv = std::get<int64_t>(v);
                out.append(reinterpret_cast<const char*>(&iv), sizeof(iv));
                break;
            }
            case ColumnType::FLOAT: {
                double d = std::holds_alternative<double>(v)
                               ? std::get<double>(v)
                               : static_cast<double>(std::get<int64_t>(v));
                out.append(reinterpret_cast<const char*>(&d), sizeof(d));
                break;
            }
            case ColumnType::TEXT: {
                const std::string& s = std::get<std::string>(v);
                int32_t len = static_cast<int32_t>(s.size());
                out.append(reinterpret_cast<const char*>(&len), sizeof(len));
                out.append(s);
                break;
            }
        }
    }
    return out;
}

Row Table::deserializeRow(const std::string& bytes) const {
    Row row;
    size_t pos = 0;
    for (const auto& col : schema_.columns) {
        switch (col.type) {
            case ColumnType::INT: {
                int64_t iv;
                std::memcpy(&iv, bytes.data() + pos, sizeof(iv));
                pos += sizeof(iv);
                row.emplace_back(iv);
                break;
            }
            case ColumnType::FLOAT: {
                double d;
                std::memcpy(&d, bytes.data() + pos, sizeof(d));
                pos += sizeof(d);
                row.emplace_back(d);
                break;
            }
            case ColumnType::TEXT: {
                int32_t len;
                std::memcpy(&len, bytes.data() + pos, sizeof(len));
                pos += sizeof(len);
                std::string s(bytes.data() + pos, len);
                pos += len;
                row.emplace_back(std::move(s));
                break;
            }
        }
    }
    return row;
}

RowId Table::insertRow(const Row& row) {
    std::string bytes = serializeRow(row);
    if (bytes.size() + 16 > PAGE_SIZE)
        throw DbException("Row too large to fit in a single " + std::to_string(PAGE_SIZE) + "-byte page");

    // First-fit scan over existing pages, else allocate a new one.
    int32_t numPages = disk_->numPages();
    Page* page = nullptr;
    int32_t pageId = -1;
    for (int32_t pid = 0; pid < numPages; ++pid) {
        Page* p = bufferPool_->fetchPage(pid);
        if (p->freeSpaceBytes() >= bytes.size()) {
            page = p;
            pageId = pid;
            break;
        }
        bufferPool_->unpinPage(pid, false);
    }
    if (!page) {
        page = bufferPool_->newPage(&pageId);
    }

    int32_t slotId = page->insertRecord(bytes.data(), static_cast<int32_t>(bytes.size()));
    bufferPool_->unpinPage(pageId, true);
    if (slotId < 0) throw DbException("insertRecord failed unexpectedly on page " + std::to_string(pageId));

    RowId rid{pageId, slotId};

    if (pkColIdx_ >= 0) {
        int64_t key = valueAsInt64(row[pkColIdx_]);
        try {
            index_->insert(key, rid);
        } catch (const DbException&) {
            // Roll back the physical insert so the row isn't orphaned on disk.
            Page* p = bufferPool_->fetchPage(pageId);
            p->deleteRecord(slotId);
            bufferPool_->unpinPage(pageId, true);
            throw;
        }
    }
    return rid;
}

bool Table::deleteRow(RowId rid) {
    Page* page = bufferPool_->fetchPage(rid.pageId);
    std::string bytes;
    bool exists = page->readRecord(rid.slotId, bytes);
    if (!exists) {
        bufferPool_->unpinPage(rid.pageId, false);
        return false;
    }
    page->deleteRecord(rid.slotId);
    bufferPool_->unpinPage(rid.pageId, true);

    if (pkColIdx_ >= 0) {
        Row row = deserializeRow(bytes);
        int64_t key = valueAsInt64(row[pkColIdx_]);
        index_->remove(key);
    }
    return true;
}

std::vector<std::pair<RowId, Row>> Table::scanAll() {
    std::vector<std::pair<RowId, Row>> results;
    int32_t numPages = disk_->numPages();
    for (int32_t pid = 0; pid < numPages; ++pid) {
        Page* page = bufferPool_->fetchPage(pid);
        int32_t slots = page->numSlots();
        for (int32_t sid = 0; sid < slots; ++sid) {
            std::string bytes;
            if (page->readRecord(sid, bytes)) {
                results.emplace_back(RowId{pid, sid}, deserializeRow(bytes));
            }
        }
        bufferPool_->unpinPage(pid, false);
    }
    return results;
}

std::optional<std::pair<RowId, Row>> Table::lookupByPrimaryKey(int64_t key) {
    if (pkColIdx_ < 0) return std::nullopt;
    auto ridOpt = index_->search(key);
    if (!ridOpt) return std::nullopt;
    RowId rid = *ridOpt;
    Page* page = bufferPool_->fetchPage(rid.pageId);
    std::string bytes;
    bool exists = page->readRecord(rid.slotId, bytes);
    bufferPool_->unpinPage(rid.pageId, false);
    if (!exists) return std::nullopt;
    return std::make_pair(rid, deserializeRow(bytes));
}

void Table::flush() { bufferPool_->flushAll(); }
