#include "bptree.h"
#include <algorithm>

BPlusTree::BPlusTree(int order) : order_(order) {
    if (order_ < 3) order_ = 3;
    leafMax_ = order_ - 1;
    root_ = new Node(true);
}

BPlusTree::~BPlusTree() { destroy(root_); }

void BPlusTree::destroy(Node* n) {
    if (!n) return;
    if (!n->isLeaf) for (auto* c : n->children) destroy(c);
    delete n;
}

BPlusTree::Node* BPlusTree::findLeaf(int64_t key) const {
    Node* cur = root_;
    while (!cur->isLeaf) {
        size_t i = 0;
        while (i < cur->keys.size() && key >= cur->keys[i]) i++;
        cur = cur->children[i];
    }
    return cur;
}

void BPlusTree::insertInLeaf(Node* leaf, int64_t key, RowId rid) {
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    size_t idx = static_cast<size_t>(it - leaf->keys.begin());
    if (it != leaf->keys.end() && *it == key) {
        throw DbException("Duplicate primary key value: " + std::to_string(key));
    }
    leaf->keys.insert(leaf->keys.begin() + idx, key);
    leaf->values.insert(leaf->values.begin() + idx, rid);
}

void BPlusTree::insert(int64_t key, RowId rid) {
    Node* leaf = findLeaf(key);
    insertInLeaf(leaf, key, rid);
    count_++;
    if (static_cast<int>(leaf->keys.size()) > leafMax_) splitLeaf(leaf);
}

void BPlusTree::splitLeaf(Node* leaf) {
    size_t mid = leaf->keys.size() / 2;
    Node* newLeaf = new Node(true);
    newLeaf->keys.assign(leaf->keys.begin() + mid, leaf->keys.end());
    newLeaf->values.assign(leaf->values.begin() + mid, leaf->values.end());
    leaf->keys.resize(mid);
    leaf->values.resize(mid);

    newLeaf->next = leaf->next;
    leaf->next = newLeaf;
    newLeaf->parent = leaf->parent;

    insertInParent(leaf, newLeaf->keys.front(), newLeaf);
}

void BPlusTree::insertInParent(Node* left, int64_t key, Node* right) {
    Node* parent = left->parent;
    if (!parent) {
        Node* newRoot = new Node(false);
        newRoot->keys.push_back(key);
        newRoot->children.push_back(left);
        newRoot->children.push_back(right);
        left->parent = newRoot;
        right->parent = newRoot;
        root_ = newRoot;
        return;
    }

    size_t idx = 0;
    while (idx < parent->children.size() && parent->children[idx] != left) idx++;
    parent->keys.insert(parent->keys.begin() + idx, key);
    parent->children.insert(parent->children.begin() + idx + 1, right);
    right->parent = parent;

    if (static_cast<int>(parent->children.size()) > order_) splitInternal(parent);
}

void BPlusTree::splitInternal(Node* node) {
    size_t mid = node->keys.size() / 2;
    int64_t upKey = node->keys[mid];

    Node* newNode = new Node(false);
    newNode->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    newNode->children.assign(node->children.begin() + mid + 1, node->children.end());
    for (auto* c : newNode->children) c->parent = newNode;

    node->keys.resize(mid);
    node->children.resize(mid + 1);
    newNode->parent = node->parent;

    insertInParent(node, upKey, newNode);
}

std::optional<RowId> BPlusTree::search(int64_t key) const {
    Node* leaf = findLeaf(key);
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it != leaf->keys.end() && *it == key) {
        size_t idx = static_cast<size_t>(it - leaf->keys.begin());
        return leaf->values[idx];
    }
    return std::nullopt;
}

bool BPlusTree::remove(int64_t key) {
    Node* leaf = findLeaf(key);
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it == leaf->keys.end() || *it != key) return false;
    size_t idx = static_cast<size_t>(it - leaf->keys.begin());
    leaf->keys.erase(leaf->keys.begin() + idx);
    leaf->values.erase(leaf->values.begin() + idx);
    count_--;
    return true; // Note: no merge/redistribution rebalancing (see header comment).
}

void BPlusTree::rangeScan(int64_t lo, int64_t hi, std::vector<RowId>& out) const {
    if (lo > hi) return;
    Node* leaf = findLeaf(lo);
    while (leaf) {
        for (size_t i = 0; i < leaf->keys.size(); ++i) {
            if (leaf->keys[i] < lo) continue;
            if (leaf->keys[i] > hi) return;
            out.push_back(leaf->values[i]);
        }
        leaf = leaf->next;
    }
}
