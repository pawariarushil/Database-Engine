#pragma once
#include "common.h"
#include <vector>
#include <memory>
#include <optional>

// ---------------------------------------------------------------------------
// In-memory B+ Tree index: int64_t key -> RowId.
//
// Used to index each table's primary key so equality lookups (and the
// executor's WHERE pk = X fast path) run in O(log n) instead of a full table
// scan. The tree is rebuilt from a table scan whenever a table is loaded
// from disk, rather than being separately persisted — this keeps the
// on-disk format simple at the cost of a linear rebuild on startup.
//
// Note: delete() removes the key from its leaf but does not perform
// merge/redistribution rebalancing. The tree remains correct (search still
// works) but node occupancy is not kept minimally bounded after heavy
// deletion — a documented simplification for a project of this scope.
// ---------------------------------------------------------------------------
class BPlusTree {
public:
    explicit BPlusTree(int order = 4); // order = max children per internal node
    ~BPlusTree();

    void insert(int64_t key, RowId rid);
    bool remove(int64_t key);
    std::optional<RowId> search(int64_t key) const;
    // Inclusive range scan, results appended in ascending key order.
    void rangeScan(int64_t lo, int64_t hi, std::vector<RowId>& out) const;
    size_t size() const { return count_; }

private:
    struct Node {
        bool isLeaf;
        std::vector<int64_t> keys;
        // internal: children.size() == keys.size() + 1
        std::vector<Node*> children;
        // leaf: values.size() == keys.size(), plus a linked-list pointer
        std::vector<RowId> values;
        Node* next = nullptr; // leaf chain, for range scans
        Node* parent = nullptr;
        explicit Node(bool leaf) : isLeaf(leaf) {}
    };

    Node* root_;
    int order_;      // max children for internal nodes
    int leafMax_;    // max keys per leaf
    size_t count_ = 0;

    Node* findLeaf(int64_t key) const;
    void insertInLeaf(Node* leaf, int64_t key, RowId rid);
    void splitLeaf(Node* leaf);
    void insertInParent(Node* left, int64_t key, Node* right);
    void splitInternal(Node* node);
    void destroy(Node* n);
};
