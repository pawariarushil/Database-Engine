#include "storage.h"
#include "common.h"
#include <cassert>

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------
void Page::reset() {
    std::memset(data_.data(), 0, PAGE_SIZE);
    setNumSlots(0);
    setFreeSpaceEnd(static_cast<int32_t>(PAGE_SIZE));
    dirty = false;
}

int32_t Page::numSlots() const {
    int32_t v;
    std::memcpy(&v, data_.data(), sizeof(v));
    return v;
}
void Page::setNumSlots(int32_t n) { std::memcpy(data_.data(), &n, sizeof(n)); }

int32_t Page::freeSpaceEnd() const {
    int32_t v;
    std::memcpy(&v, data_.data() + 4, sizeof(v));
    return v;
}
void Page::setFreeSpaceEnd(int32_t off) { std::memcpy(data_.data() + 4, &off, sizeof(off)); }

Page::SlotInfo Page::getSlot(int32_t slotId) const {
    size_t base = HEADER_SIZE + slotId * SLOT_SIZE;
    SlotInfo s;
    std::memcpy(&s.offset, data_.data() + base, sizeof(int32_t));
    std::memcpy(&s.length, data_.data() + base + 4, sizeof(int32_t));
    return s;
}
void Page::setSlot(int32_t slotId, int32_t offset, int32_t length) {
    size_t base = HEADER_SIZE + slotId * SLOT_SIZE;
    std::memcpy(data_.data() + base, &offset, sizeof(int32_t));
    std::memcpy(data_.data() + base + 4, &length, sizeof(int32_t));
}

size_t Page::freeSpaceBytes() const {
    size_t slotArrayEnd = HEADER_SIZE + static_cast<size_t>(numSlots()) * SLOT_SIZE;
    size_t recordAreaStart = static_cast<size_t>(freeSpaceEnd());
    if (recordAreaStart < slotArrayEnd + SLOT_SIZE) return 0; // no room for even one more slot
    return recordAreaStart - slotArrayEnd - SLOT_SIZE; // reserve room for a new slot entry too
}

int32_t Page::insertRecord(const char* bytes, int32_t len) {
    if (freeSpaceBytes() < static_cast<size_t>(len)) return -1;
    int32_t newEnd = freeSpaceEnd() - len;
    std::memcpy(data_.data() + newEnd, bytes, len);
    setFreeSpaceEnd(newEnd);

    int32_t slotId = numSlots();
    setSlot(slotId, newEnd, len);
    setNumSlots(slotId + 1);
    dirty = true;
    return slotId;
}

void Page::deleteRecord(int32_t slotId) {
    if (slotId < 0 || slotId >= numSlots()) return;
    SlotInfo s = getSlot(slotId);
    setSlot(slotId, s.offset, -1); // tombstone
    dirty = true;
}

bool Page::readRecord(int32_t slotId, std::string& out) const {
    if (slotId < 0 || slotId >= numSlots()) return false;
    SlotInfo s = getSlot(slotId);
    if (s.length < 0) return false; // deleted
    out.assign(data_.data() + s.offset, s.length);
    return true;
}

// ---------------------------------------------------------------------------
// DiskManager
// ---------------------------------------------------------------------------
DiskManager::DiskManager(const std::string& filePath) : path_(filePath) {
    // Open for read/write, create if missing.
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        std::ofstream create(path_, std::ios::binary);
        create.close();
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    }
    if (!file_.is_open()) throw DbException("Could not open table file: " + path_);

    file_.seekg(0, std::ios::end);
    std::streamoff size = file_.tellg();
    numPages_ = static_cast<int32_t>(size / PAGE_SIZE);
}

DiskManager::~DiskManager() {
    if (file_.is_open()) { file_.flush(); file_.close(); }
}

void DiskManager::readPage(int32_t pageId, char* out) {
    if (pageId < 0 || pageId >= numPages_) throw DbException("readPage: invalid page id " + std::to_string(pageId));
    file_.seekg(static_cast<std::streamoff>(pageId) * PAGE_SIZE);
    file_.read(out, PAGE_SIZE);
    if (!file_) throw DbException("readPage: I/O error on page " + std::to_string(pageId));
}

void DiskManager::writePage(int32_t pageId, const char* in) {
    if (pageId < 0) throw DbException("writePage: invalid page id");
    file_.seekp(static_cast<std::streamoff>(pageId) * PAGE_SIZE);
    file_.write(in, PAGE_SIZE);
    file_.flush();
    if (!file_) throw DbException("writePage: I/O error on page " + std::to_string(pageId));
    if (pageId >= numPages_) numPages_ = pageId + 1;
}

int32_t DiskManager::allocatePage() {
    int32_t newId = numPages_;
    std::vector<char> zero(PAGE_SIZE, 0);
    writePage(newId, zero.data()); // extends file and bumps numPages_
    return newId;
}

// ---------------------------------------------------------------------------
// BufferPoolManager (LRU)
// ---------------------------------------------------------------------------
BufferPoolManager::BufferPoolManager(DiskManager* disk, size_t poolSize)
    : disk_(disk), poolSize_(poolSize) {
    frames_.resize(poolSize_, nullptr);
    pinCount_.resize(poolSize_, 0);
    freeFrame_.assign(poolSize_, true);
    for (size_t i = 0; i < poolSize_; ++i) frames_[i] = new Page();
}

BufferPoolManager::~BufferPoolManager() {
    flushAll();
    for (auto* p : frames_) delete p;
}

void BufferPoolManager::touchLru(size_t frame) {
    auto it = lruMap_.find(frame);
    if (it != lruMap_.end()) lruList_.erase(it->second);
    lruList_.push_front(frame);
    lruMap_[frame] = lruList_.begin();
}

int BufferPoolManager::findFreeOrEvictFrame() {
    for (size_t i = 0; i < poolSize_; ++i)
        if (freeFrame_[i]) return static_cast<int>(i);

    // Evict least-recently-used frame with pin count 0.
    for (auto it = lruList_.rbegin(); it != lruList_.rend(); ++it) {
        size_t frame = *it;
        if (pinCount_[frame] == 0) {
            Page* p = frames_[frame];
            if (p->dirty) disk_->writePage(p->pageId, p->raw());
            pageTable_.erase(p->pageId);
            lruMap_.erase(frame);
            lruList_.erase(std::next(it).base());
            return static_cast<int>(frame);
        }
    }
    return -1; // pool exhausted (all pages pinned)
}

Page* BufferPoolManager::fetchPage(int32_t pageId) {
    auto it = pageTable_.find(pageId);
    if (it != pageTable_.end()) {
        size_t frame = it->second;
        pinCount_[frame]++;
        touchLru(frame);
        return frames_[frame];
    }

    int frame = findFreeOrEvictFrame();
    if (frame < 0) throw DbException("BufferPoolManager: pool exhausted, cannot fetch page " + std::to_string(pageId));

    freeFrame_[frame] = false;
    Page* p = frames_[frame];
    p->reset();
    disk_->readPage(pageId, p->raw());
    p->pageId = pageId;
    p->dirty = false;
    pageTable_[pageId] = static_cast<size_t>(frame);
    pinCount_[frame] = 1;
    touchLru(frame);
    return p;
}

Page* BufferPoolManager::newPage(int32_t* outPageId) {
    int32_t pageId = disk_->allocatePage();
    int frame = findFreeOrEvictFrame();
    if (frame < 0) throw DbException("BufferPoolManager: pool exhausted, cannot allocate new page");

    freeFrame_[frame] = false;
    Page* p = frames_[frame];
    p->reset();
    p->pageId = pageId;
    p->dirty = true;
    pageTable_[pageId] = static_cast<size_t>(frame);
    pinCount_[frame] = 1;
    touchLru(frame);
    *outPageId = pageId;
    return p;
}

void BufferPoolManager::unpinPage(int32_t pageId, bool isDirty) {
    auto it = pageTable_.find(pageId);
    if (it == pageTable_.end()) return;
    size_t frame = it->second;
    if (isDirty) frames_[frame]->dirty = true;
    if (pinCount_[frame] > 0) pinCount_[frame]--;
}

void BufferPoolManager::flushPage(int32_t pageId) {
    auto it = pageTable_.find(pageId);
    if (it == pageTable_.end()) return;
    Page* p = frames_[it->second];
    if (p->dirty) {
        disk_->writePage(p->pageId, p->raw());
        p->dirty = false;
    }
}

void BufferPoolManager::flushAll() {
    for (auto& kv : pageTable_) flushPage(kv.first);
}
