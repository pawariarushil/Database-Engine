#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

constexpr size_t PAGE_SIZE = 4096;
constexpr int32_t INVALID_PAGE_ID = -1;

// ---------------------------------------------------------------------------
// Page: a fixed-size 4KB block, laid out as a "slotted page":
//
//   [ header: num_slots(4) | free_space_end(4) ]
//   [ slot 0: offset(4) len(4) ][ slot 1: offset(4) len(4) ] ...  (grows down)
//   .......................... free space ..........................
//   [ record data ] [ record data ] ...                              (grows up
//                                                       from the end of page)
//
// Deleted records are tombstoned by setting len = -1 in their slot rather
// than physically compacting the page (simple, avoids shifting offsets).
// ---------------------------------------------------------------------------
class Page {
public:
    Page() { data_.assign(PAGE_SIZE, 0); reset(); }

    char* raw() { return data_.data(); }
    const char* raw() const { return data_.data(); }

    void reset();

    int32_t numSlots() const;
    void setNumSlots(int32_t n);

    int32_t freeSpaceEnd() const;      // byte offset where record data currently starts
    void setFreeSpaceEnd(int32_t off);

    // Returns byte offset of free space available for a new record.
    size_t freeSpaceBytes() const;

    struct SlotInfo { int32_t offset; int32_t length; };
    SlotInfo getSlot(int32_t slotId) const;
    void setSlot(int32_t slotId, int32_t offset, int32_t length);

    // Inserts raw bytes into the page, returns the slot id, or -1 if no room.
    int32_t insertRecord(const char* bytes, int32_t len);
    // Marks a slot as deleted (tombstone). Space is not reclaimed/compacted.
    void deleteRecord(int32_t slotId);
    // Reads raw bytes for a slot; returns false if slot is empty/deleted.
    bool readRecord(int32_t slotId, std::string& out) const;

    bool dirty = false;
    int32_t pageId = INVALID_PAGE_ID;

private:
    std::vector<char> data_;
    static constexpr size_t HEADER_SIZE = 8;
    static constexpr size_t SLOT_SIZE = 8;
};

// Reads/writes fixed-size pages to a backing file, one file per table.
class DiskManager {
public:
    explicit DiskManager(const std::string& filePath);
    ~DiskManager();

    void readPage(int32_t pageId, char* out);
    void writePage(int32_t pageId, const char* in);
    int32_t allocatePage();     // extends the file by one page, returns new id
    int32_t numPages() const { return numPages_; }

private:
    std::fstream file_;
    std::string path_;
    int32_t numPages_ = 0;
};

// A simple LRU buffer pool caching Page objects backed by a DiskManager.
// Pages are pinned while in use by a caller and unpinned (with a dirty flag)
// when the caller is done; only unpinned pages are eligible for eviction.
class BufferPoolManager {
public:
    BufferPoolManager(DiskManager* disk, size_t poolSize);
    ~BufferPoolManager();

    Page* fetchPage(int32_t pageId);   // pin count++
    Page* newPage(int32_t* outPageId); // allocate + pin
    void unpinPage(int32_t pageId, bool isDirty);
    void flushPage(int32_t pageId);
    void flushAll();

private:
    DiskManager* disk_;
    size_t poolSize_;
    std::vector<Page*> frames_;
    std::unordered_map<int32_t, size_t> pageTable_; // pageId -> frame index
    std::vector<int32_t> pinCount_;
    std::list<size_t> lruList_;                       // frame indices, front = most-recently-used
    std::unordered_map<size_t, std::list<size_t>::iterator> lruMap_;
    std::vector<bool> freeFrame_;

    int findFreeOrEvictFrame();
    void touchLru(size_t frame);
};
