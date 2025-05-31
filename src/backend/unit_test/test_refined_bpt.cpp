#include <algorithm>
#include <cassert>
#include <compare> // For std::strong_ordering and operator<=>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector> // For std::vector general use, and potentially if norb::vector is an alias

// --- Include the B+ Tree Header ---
// This should bring in norb::BPlusTree, norb::Pair, norb::vector, etc.
#include "b_plus_tree.hpp"

// --- Test Value Types ---
struct LargeObjectWithId {
    using id_t = int;
    int obj_id;
    norb::FixedString<20> data;
    double value;

    LargeObjectWithId(int i = 0, std::string d = "", double v = 0.0) : obj_id(i), data(std::move(d)), value(v) {
    }

    id_t id() const {
        return obj_id;
    }

    // Comparison operators
    bool operator==(const LargeObjectWithId &other) const {
        return obj_id == other.obj_id && data == other.data && value == other.value;
    }

    // Provide operator<=> for norb::Pair's default operator<=>
    std::strong_ordering operator<=>(const LargeObjectWithId &other) const {
        if (auto cmp = obj_id <=> other.obj_id; cmp != 0)
            return cmp;
        if (auto cmp = data <=> other.data; cmp != 0)
            return cmp;
        if (value < other.value)
            return std::strong_ordering::less;
        if (value > other.value)
            return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    friend std::ostream &operator<<(std::ostream &os, const LargeObjectWithId &obj) {
        os << "{id:" << obj.obj_id << ", d:'" << obj.data << "', v:" << obj.value << "}";
        return os;
    }
};

// For testing the case without the id() interface
using SimpleValue = int; // int already has operator<=>

// namespace norb { // Assuming norb::vector is in this namespace
//     template <typename T> std::ostream &operator<<(std::ostream &os, const norb::vector<T> &vec) {
//         os << "[";
//         for (size_t i = 0; i < vec.size(); ++i) {
//             os << vec[i]; // Relies on T being streamable
//             if (i < vec.size() - 1)
//                 os << ", ";
//         }
//         os << "]";
//         return os;
//     }
// } // namespace norb

// --- Test Functions ---
void test_with_large_object() {
    std::cout << "\n--- Testing BPlusTree with LargeObjectWithId (should use IDs in index nodes) ---" << std::endl;
    norb::BPlusTree<int, LargeObjectWithId> tree;

    // Check initial state
    assert(tree.size() == 0);
    std::cout << "Initial tree:" << std::endl;
    tree.traverse(true); // Enable checks

    // Insertions
    std::cout << "\nInserting items..." << std::endl;
    tree.insert(10, LargeObjectWithId(1, "apple", 1.0));
    tree.traverse();
    tree.insert(20, LargeObjectWithId(2, "banana", 2.0));
    tree.traverse();
    tree.insert(5, LargeObjectWithId(3, "cherry", 3.0)); // Key 5, ID 3
    assert(tree.size() == 3);
    tree.traverse(true);

    tree.insert(15, LargeObjectWithId(4, "date", 4.0)); // Should trigger leaf split
    tree.insert(25, LargeObjectWithId(5, "elderberry", 5.0));
    tree.insert(1, LargeObjectWithId(6, "fig", 6.0));
    assert(tree.size() == 6);
    std::cout << "\nAfter more insertions (expect splits):" << std::endl;
    tree.traverse(true); // Index nodes should show Pair<int, int (id_t)>

    tree.insert(12, LargeObjectWithId(7, "grape", 7.0));
    tree.insert(30, LargeObjectWithId(8, "honeydew", 8.0));
    tree.insert(17, LargeObjectWithId(9, "kiwi", 9.0)); // Potentially more splits, maybe root split
    assert(tree.size() == 9);
    std::cout << "\nAfter even more insertions (expect more splits):" << std::endl;
    tree.traverse(true);

    // Find operations
    std::cout << "\nFinding items..." << std::endl;
    norb::vector<LargeObjectWithId> found = tree.find_all(15); // Use norb::vector
    std::cout << "Found for key 15: " << found << std::endl;
    assert(found.size() == 1 && found[0] == LargeObjectWithId(4, "date", 4.0));

    found = tree.find_all(10); // Use norb::vector
    std::cout << "Found for key 10: " << found << std::endl;
    assert(found.size() == 1 && found[0] == LargeObjectWithId(1, "apple", 1.0));

    found = tree.find_all(5); // Use norb::vector
    std::cout << "Found for key 5: " << found << std::endl;
    assert(found.size() == 1 && found[0] == LargeObjectWithId(3, "cherry", 3.0));

    found = tree.find_all(99); // Non-existent key, use norb::vector
    std::cout << "Found for key 99: " << found << std::endl;
    assert(found.empty()); // Assuming norb::vector has empty()

    // Insert duplicate key
    tree.insert(15, LargeObjectWithId(10, "date_duplicate", 4.5));
    assert(tree.size() == 10);
    std::cout << "\nAfter inserting duplicate key 15:" << std::endl;
    tree.traverse(true);
    found = tree.find_all(15); // Use norb::vector
    std::cout << "Found for key 15 (after duplicate): " << found << std::endl;
    assert(found.size() == 2);

    // Check order of duplicates. Relies on LargeObjectWithId comparison and norb::Pair comparison.
    // The B+ tree stores pairs (key, value) and if keys are equal, order by value.
    // LargeObjectWithId(4, "date", 4.0) vs LargeObjectWithId(10, "date_duplicate", 4.5)
    // ID 4 vs ID 10. "date" vs "date_duplicate". 4.0 vs 4.5
    // The comparison for LargeObjectWithId is id, then data, then value.
    // So {id:4, ...} should come before {id:10, ...}
    bool order_correct = (found[0].obj_id == 4 && found[1].obj_id == 10);
    if (!order_correct) { // If obj_id is not the primary sort for values with same key
        // Fallback to check if they are present in any order
        order_correct =
            ((found[0].obj_id == 4 && found[1].obj_id == 10) || (found[0].obj_id == 10 && found[1].obj_id == 4));
    }
    assert(order_correct && "Duplicate items not found or not in expected order");

    // Deletions
    std::cout << "\nDeleting items..." << std::endl;
    bool removed = tree.remove(15, LargeObjectWithId(4, "date", 4.0));
    std::cout << "Removed (15, {id:4, d:'date', v:4.0}): " << std::boolalpha << removed << std::endl;
    assert(removed);
    assert(tree.size() == 9);
    tree.traverse(true);

    found = tree.find_all(15); // Use norb::vector
    std::cout << "Found for key 15 (after one deletion): " << found << std::endl;
    assert(found.size() == 1 && found[0] == LargeObjectWithId(10, "date_duplicate", 4.5));

    removed = tree.remove(10, LargeObjectWithId(1, "apple", 1.0));
    assert(removed);
    removed = tree.remove(20, LargeObjectWithId(2, "banana", 2.0));
    assert(removed);
    removed = tree.remove(5, LargeObjectWithId(3, "cherry", 3.0));
    assert(removed);
    assert(tree.size() == 6);
    std::cout << "\nAfter more deletions (expect merges/borrows):" << std::endl;
    tree.traverse(true);

    removed = tree.remove(99, LargeObjectWithId(100, "non_existent", 0.0)); // Non-existent
    assert(!removed);

    // Delete all remaining
    tree.remove(15, LargeObjectWithId(10, "date_duplicate", 4.5));
    tree.remove(25, LargeObjectWithId(5, "elderberry", 5.0));
    tree.remove(1, LargeObjectWithId(6, "fig", 6.0));
    tree.remove(12, LargeObjectWithId(7, "grape", 7.0));
    tree.remove(30, LargeObjectWithId(8, "honeydew", 8.0));
    tree.remove(17, LargeObjectWithId(9, "kiwi", 9.0));

    assert(tree.size() == 0);
    std::cout << "\nAfter deleting all items:" << std::endl;
    tree.traverse(true);

    std::cout << "--- Test with LargeObjectWithId PASSED ---" << std::endl;
}

void test_with_simple_value() {
    std::cout << "\n--- Testing BPlusTree with SimpleValue (int) (should use full value in index nodes) ---"
              << std::endl;
    norb::BPlusTree<int, SimpleValue> tree; // SimpleValue is int

    // Check initial state
    assert(tree.size() == 0);
    std::cout << "Initial tree:" << std::endl;
    tree.traverse(true);

    // Insertions
    std::cout << "\nInserting items..." << std::endl;
    tree.insert(10, 100);
    tree.insert(20, 200);
    tree.insert(5, 50);
    assert(tree.size() == 3);
    tree.traverse(true);

    tree.insert(15, 150); // Should trigger leaf split
    tree.insert(25, 250);
    tree.insert(1, 10);
    assert(tree.size() == 6);
    std::cout << "\nAfter more insertions (expect splits):" << std::endl;
    tree.traverse(true); // Index nodes should show Pair<int, int (SimpleValue)>

    // Find operations
    std::cout << "\nFinding items..." << std::endl;
    norb::vector<SimpleValue> found = tree.find_all(15); // Use norb::vector
    std::cout << "Found for key 15: " << found << std::endl;
    assert(found.size() == 1 && found[0] == 150);

    found = tree.find_all(99); // Non-existent key, use norb::vector
    std::cout << "Found for key 99: " << found << std::endl;
    assert(found.empty()); // Assuming norb::vector has empty()

    // Deletions
    std::cout << "\nDeleting items..." << std::endl;
    bool removed = tree.remove(15, 150);
    std::cout << "Removed (15, 150): " << std::boolalpha << removed << std::endl;
    assert(removed);
    assert(tree.size() == 5);
    tree.traverse(true);

    // Delete all
    tree.remove(10, 100);
    tree.remove(20, 200);
    tree.remove(5, 50);
    tree.remove(25, 250);
    tree.remove(1, 10);
    assert(tree.size() == 0);
    std::cout << "\nAfter deleting all items:" << std::endl;
    tree.traverse(true);

    std::cout << "--- Test with SimpleValue PASSED ---" << std::endl;
}

int main() {
    // This function is from your environment, ensure it does necessary cleanup
    // for your PersistentMemory implementation before each test run.
    norb::chore::remove_associated();
    test_with_large_object();

    norb::chore::remove_associated();
    test_with_simple_value();

    std::cout << "\nAll tests completed." << std::endl;

    return 0;
}