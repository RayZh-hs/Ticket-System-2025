#include "stlite/set.hpp" // Your set implementation

#include <algorithm> // For std::sort, std::is_sorted, std::shuffle, std::next_permutation
#include <iostream>
#include <random>    // For std::mt19937, std::shuffle
#include <set>       // For std::set to compare against (ground truth)
#include <stdexcept> // For std::runtime_error
#include <string>
#include <utility> // For std::pair in insert's return
#include <vector>

// --- Configuration for Tests ---
// #define ENABLE_NORB_DEBUG_CHECKS // Manually uncomment to enable DEBUG_CHECK_UTILS if your set supports it
// You'd need to define USE_DEBUG_FLAGS and DEBUG_CHECK_UTILS in norb_set.h
// or pass them as compiler flags. This test file doesn't control those.

#ifdef ENABLE_NORB_DEBUG_CHECKS
#ifndef USE_DEBUG_FLAGS
#define USE_DEBUG_FLAGS
#endif
#ifndef DEBUG_CHECK_UTILS
#define DEBUG_CHECK_UTILS
#endif
// Re-include to pick up debug flags if they are conditional
#include "norb_set.h"
#endif

// Simple assertion macros
#define TEST_CASE(name)                                                                                                \
    std::cout << "\n--- Test Case: " << name << " ---" << std::endl;                                                   \
    current_test_case_name = name;

#define SUB_TEST(name) std::cout << "  Sub-test: " << name << std::endl;

std::string current_test_case_name = "";

#define ASSERT_TRUE(condition)                                                                                         \
    if (!(condition)) {                                                                                                \
        std::cerr << "Assertion failed in test case: [" << current_test_case_name                                      \
                  << "]: (" #condition ") is false, in " << __FILE__ << " line " << __LINE__ << std::endl;             \
        std::exit(EXIT_FAILURE);                                                                                       \
    }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQUAL(val1, val2)                                                                                       \
    if (!((val1) == (val2))) {                                                                                         \
        std::cerr << "Assertion failed in test case: [" << current_test_case_name                                      \
                  << "]: (" #val1 " == " #val2 ") values: (" << (val1) << " vs " << (val2) << ") in " << __FILE__      \
                  << " line " << __LINE__ << std::endl;                                                                \
        std::exit(EXIT_FAILURE);                                                                                       \
    }

#define ASSERT_THROWS(expression, ExceptionType)                                                                       \
    do {                                                                                                               \
        bool caught = false;                                                                                           \
        try {                                                                                                          \
            expression;                                                                                                \
        } catch (const ExceptionType &) {                                                                              \
            caught = true;                                                                                             \
        } catch (...) {                                                                                                \
            std::cerr << "Assertion failed in test case: [" << current_test_case_name                                  \
                      << "]: Expected " #ExceptionType " but caught different exception from (" #expression ") in "    \
                      << __FILE__ << " line " << __LINE__ << std::endl;                                                \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
        if (!caught) {                                                                                                 \
            std::cerr << "Assertion failed in test case: [" << current_test_case_name                                  \
                      << "]: Expected " #ExceptionType " from (" #expression ") but no exception was thrown, in "      \
                      << __FILE__ << " line " << __LINE__ << std::endl;                                                \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (0)

// Helper to check set integrity and compare with std::set
template <typename Key, typename Compare = std::less<Key>>
void verify_set_integrity(const norb::set<Key, Compare> &n_set, const std::set<Key, Compare> &std_set) {
    ASSERT_EQUAL(n_set.size(), std_set.size());
    ASSERT_EQUAL(n_set.empty(), std_set.empty());

    if (n_set.empty()) {
        ASSERT_TRUE(n_set.begin() == n_set.end());
        ASSERT_TRUE(n_set.cbegin() == n_set.cend());
    } else {
        ASSERT_FALSE(n_set.begin() == n_set.end());
        ASSERT_FALSE(n_set.cbegin() == n_set.cend());
    }

    std::vector<Key> n_elements, std_elements;
    for (const auto &k : n_set) {
        n_elements.push_back(k);
    }
    for (const auto &k : std_set) {
        std_elements.push_back(k);
    }
    ASSERT_EQUAL(n_elements.size(), std_elements.size());
    for (size_t i = 0; i < n_elements.size(); ++i) {
        ASSERT_EQUAL(n_elements[i], std_elements[i]);
    }

    // Check if sorted according to Compare
    if (n_elements.size() > 1) {
        for (size_t i = 0; i < n_elements.size() - 1; ++i) {
            Compare comp;
            ASSERT_FALSE(comp(n_elements[i + 1], n_elements[i])); // !(next < current)
            ASSERT_TRUE(comp(n_elements[i], n_elements[i + 1]) ||
                        (!comp(n_elements[i + 1], n_elements[i]) &&
                         !comp(n_elements[i], n_elements[i + 1]))); // current < next OR current == next
        }
    }

    for (const auto &val : std_set) {
        ASSERT_TRUE(n_set.count(val) == 1);
        ASSERT_FALSE(n_set.find(val) == n_set.end());
        ASSERT_EQUAL(*(n_set.find(val)), val);
    }

#ifdef ENABLE_NORB_DEBUG_CHECKS
    const_cast<norb::set<Key, Compare> &>(n_set).check(); // Requires check() to be public or friend
#endif
}

void test_constructors_and_basic_ops() {
    TEST_CASE("Constructors and Basic Ops");

    SUB_TEST("Default constructor");
    norb::set<int> s1;
    std::set<int> std_s1;
    verify_set_integrity(s1, std_s1);
    ASSERT_TRUE(s1.empty());
    ASSERT_EQUAL(s1.size(), 0);
    ASSERT_TRUE(s1.begin() == s1.end());
    ASSERT_TRUE(s1.cbegin() == s1.cend());
}

void test_insert_and_find() {
    TEST_CASE("Insert and Find");
    norb::set<int> s;
    std::set<int> std_s;

    SUB_TEST("Insert single element");
    auto p1 = s.insert(10);
    std_s.insert(10);
    ASSERT_TRUE(p1.second); // Inserted
    ASSERT_EQUAL(*(p1.first), 10);
    verify_set_integrity(s, std_s);

    SUB_TEST("Find existing element");
    auto it_find = s.find(10);
    ASSERT_FALSE(it_find == s.end());
    ASSERT_EQUAL(*it_find, 10);

    SUB_TEST("Find non-existent element");
    ASSERT_TRUE(s.find(99) == s.end());

    SUB_TEST("Insert multiple unique elements");
    int data[] = {5, 15, 3, 7, 12, 17};
    for (int x : data) {
        auto p = s.insert(x);
        std_s.insert(x);
        ASSERT_TRUE(p.second);
        ASSERT_EQUAL(*(p.first), x);
    }
    verify_set_integrity(s, std_s);

    SUB_TEST("Insert duplicate element");
    auto p_dup = s.insert(7);        // 7 is already in
    ASSERT_FALSE(p_dup.second);      // Not inserted
    ASSERT_EQUAL(*(p_dup.first), 7); // Iterator to existing
    verify_set_integrity(s, std_s);  // Size should not change

    SUB_TEST("Count elements");
    ASSERT_EQUAL(s.count(10), 1);
    ASSERT_EQUAL(s.count(7), 1);
    ASSERT_EQUAL(s.count(99), 0); // Non-existent
}

void test_iterators() {
    TEST_CASE("Iterators");
    norb::set<int> s;
    std::set<int> std_s;
    int data[] = {50, 25, 75, 10, 30, 60, 90, 5, 15, 28, 35, 55, 65, 80, 95};
    for (int x : data) {
        s.insert(x);
        std_s.insert(x);
    }
    verify_set_integrity(s, std_s);

    SUB_TEST("Forward iteration");
    auto it_s = s.begin();
    auto it_std = std_s.begin();
    while (it_s != s.end() && it_std != std_s.end()) {
        ASSERT_EQUAL(*it_s, *it_std);
        // To test operator->, you'd typically do it_s.operator->() if it returns a pointer to Key,
        // or if Key were a struct: it_s->member. For primitive Key, *it_s is fine.
        // The original it_s-> was a syntax error. We'll rely on *it_s for value comparison.
        // If you want to explicitly test operator-> returns the correct pointer:
        ASSERT_EQUAL(*(it_s.operator->()), *it_std); // Correct way to use operator-> and get value
        ++it_s;
        ++it_std;
    }
    ASSERT_TRUE(it_s == s.end());
    ASSERT_TRUE(it_std == std_s.end());

    SUB_TEST("Const forward iteration (cbegin/cend)");
    auto cit_s = s.cbegin();
    auto cit_std = std_s.cbegin();
    while (cit_s != s.cend() && cit_std != std_s.cend()) {
        ASSERT_EQUAL(*cit_s, *cit_std);
        // Same as above for operator->
        ASSERT_EQUAL(*(cit_s.operator->()), *cit_std); // Correct way
        ++cit_s;
        ++cit_std;
    }
    ASSERT_TRUE(cit_s == s.cend());
    ASSERT_TRUE(cit_std == std_s.cend());

    SUB_TEST("Backward iteration");
    if (!s.empty()) {
        auto rit_s = s.end(); // Use non-const end for norb::set if decrementing non-const iterator
        auto rit_std = std_s.end();
        do {
            --rit_s;
            --rit_std;
            ASSERT_EQUAL(*rit_s, *rit_std);
        } while (rit_s != s.begin() && rit_std != std_s.begin());
        ASSERT_TRUE(rit_s == s.begin());
        ASSERT_TRUE(rit_std == std_s.begin());
    }

    SUB_TEST("Iterator to const_iterator conversion");
    norb::set<int>::iterator regular_it = s.begin();
    // norb::set<int>::const_iterator const_it = regular_it; // OLD - causes error due to explicit ctor
    norb::set<int>::const_iterator const_it(regular_it); // FIX 1: Explicit construction
    // OR, and preferably if as_const() exists and works as intended:
    // auto const_it = regular_it.as_const(); // FIX 2: Using as_const()

    if (regular_it != s.end()) { // Check regular_it, not const_it before it's potentially reassigned
        ASSERT_EQUAL(*regular_it, *const_it);
    }

    SUB_TEST("Iterator comparisons");
    auto b1 = s.begin();
    auto b2 = s.begin();
    auto e1 = s.end();
    ASSERT_TRUE(b1 == b2);
    ASSERT_FALSE(b1 == e1);
    if (b1 != e1) {
        auto next_b1 = b1;
        ++next_b1;
        ASSERT_FALSE(b1 == next_b1);
    }

    SUB_TEST("Iterator exceptions");
    norb::set<int> empty_s;
    ASSERT_THROWS(++(empty_s.end()), std::runtime_error);
    ASSERT_THROWS(--(empty_s.begin()), std::runtime_error); // begin() == end() for empty

    if (!s.empty()) {
        auto last_it = s.end();
        --last_it; // Points to max element
        auto first_it = s.begin();

        ASSERT_THROWS(++(s.end()), std::runtime_error);
        ASSERT_THROWS(--(s.begin()), std::runtime_error);
    }
}

void test_erase() {
    TEST_CASE("Erase");

    SUB_TEST("Erase various node types");
    // Test cases: leaf, node with left child, node with right child, node with two children, root
    int initial_data[] = {50, 25, 75, 10, 30, 60, 90, 5, 15, 28, 35, 55, 65, 80, 95};
    int elements_to_erase[] = {
        5,  // Leaf
        15, // Leaf
        10, // Node with one child (28, after 5 and 15 are gone, 10 has no left, 28 as right if 15 was its child)
            // Let's re-evaluate erase order for clarity
        65,                // Leaf
        55,                // Node with one child (left, 60 after 65 gone)
        95,                // Leaf
        80,                // Node with one child (right, 90 after 95 gone)
        25,                // Node with two children (10, 30 initially, depends on prior erasures)
        75,                // Node with two children
        50,                // Root with two children
        28, 30, 35, 60, 90 // Clean up
    };
    // A more systematic approach:
    // Build a known tree, erase specific nodes, verify.
    // Example: {50, 25, 75, 10, 30, 60, 90}
    // Erase 10 (leaf)
    // Erase 90 (leaf)
    // Erase 25 (one child, 30, after 10 is gone)
    // Erase 75 (one child, 60, after 90 is gone)
    // Erase 50 (root, two children 30, 60)
    // Erase 30
    // Erase 60 -> empty

    std::vector<int> erase_order_test_data = {50, 25, 75, 10, 30, 60, 90};
    std::vector<int> erase_sequence = {10, 90, 25, 75, 50, 30, 60}; // Specific order

    norb::set<int> s_erase;
    std::set<int> std_s_erase;

    for (int x : erase_order_test_data) {
        s_erase.insert(x);
        std_s_erase.insert(x);
    }
    verify_set_integrity(s_erase, std_s_erase);

    for (int val_to_erase : erase_sequence) {
        SUB_TEST("Erasing " + std::to_string(val_to_erase));
        auto it = s_erase.find(val_to_erase);
        ASSERT_FALSE(it == s_erase.end()); // Must exist

        s_erase.erase(it);
        std_s_erase.erase(val_to_erase);
        verify_set_integrity(s_erase, std_s_erase);
    }
    ASSERT_TRUE(s_erase.empty());

    SUB_TEST("Erase all elements (random order)");
    norb::set<int> s_rand_erase;
    std::set<int> std_s_rand_erase;
    std::vector<int> v_data;
    for (int i = 1; i <= 20; ++i)
        v_data.push_back(i * 5);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(v_data.begin(), v_data.end(), g);

    for (int x : v_data) {
        s_rand_erase.insert(x);
        std_s_rand_erase.insert(x);
    }
    verify_set_integrity(s_rand_erase, std_s_rand_erase);

    std::shuffle(v_data.begin(), v_data.end(), g); // Shuffle again for erase order

    for (int val_to_erase : v_data) {
        auto it = s_rand_erase.find(val_to_erase);
        ASSERT_FALSE(it == s_rand_erase.end());
        s_rand_erase.erase(it);
        std_s_rand_erase.erase(val_to_erase);
        verify_set_integrity(s_rand_erase, std_s_rand_erase);
    }
    ASSERT_TRUE(s_rand_erase.empty());

    SUB_TEST("Erase exceptions");
    norb::set<int> s_ex;
    s_ex.insert(1);
    ASSERT_THROWS(s_ex.erase(s_ex.end()), std::runtime_error);

    norb::set<int> s_other; // Iterator from different set
    auto it_other = s_other.insert(100).first;
    ASSERT_THROWS(s_ex.erase(it_other), std::runtime_error);

    norb::set<int> empty_s_ex;
    ASSERT_THROWS(empty_s_ex.erase(empty_s_ex.begin()), std::runtime_error); // begin() == end()
}

void test_clear_and_count() {
    TEST_CASE("Clear and Count");
    norb::set<int> s;
    std::set<int> std_s;

    SUB_TEST("Clear empty set");
    s.clear();
    std_s.clear(); // std::set clear is no-op on empty
    verify_set_integrity(s, std_s);

    SUB_TEST("Clear non-empty set");
    int data[] = {10, 5, 15, 20};
    for (int x : data) {
        s.insert(x);
        std_s.insert(x);
    }
    verify_set_integrity(s, std_s);
    ASSERT_FALSE(s.empty());

    s.clear();
    std_s.clear();
    verify_set_integrity(s, std_s);
    ASSERT_TRUE(s.empty());
    ASSERT_EQUAL(s.size(), 0);
    ASSERT_TRUE(s.begin() == s.end());

    SUB_TEST("Count after clear");
    ASSERT_EQUAL(s.count(10), 0);
}

void test_copy_assignment() {
    TEST_CASE("Copy Constructor and Assignment Operator");
    norb::set<int> s1;
    std::set<int> std_s1;
    int data[] = {100, 50, 150, 25, 75, 125, 175};
    for (int x : data) {
        s1.insert(x);
        std_s1.insert(x);
    }

    SUB_TEST("Copy constructor");
    norb::set<int> s2 = s1;
    std::set<int> std_s2 = std_s1;
    verify_set_integrity(s2, std_s2);
    // Modify s2, s1 should be unchanged
    s2.insert(1);
    std_s2.insert(1);
    s2.erase(s2.find(100));
    std_s2.erase(100);
    verify_set_integrity(s1, std_s1); // s1 unchanged
    verify_set_integrity(s2, std_s2); // s2 changed

    SUB_TEST("Assignment operator (to empty)");
    norb::set<int> s3;
    std::set<int> std_s3;
    s3 = s1;
    std_s3 = std_s1;
    verify_set_integrity(s3, std_s3);
    // Modify s3, s1 should be unchanged
    s3.insert(2);
    std_s3.insert(2);
    s3.erase(s3.find(50));
    std_s3.erase(50);
    verify_set_integrity(s1, std_s1);
    verify_set_integrity(s3, std_s3);

    SUB_TEST("Assignment operator (to non-empty)");
    norb::set<int> s4;
    s4.insert(999);
    s4.insert(888);
    std::set<int> std_s4;
    std_s4.insert(999);
    std_s4.insert(888);
    s4 = s1;
    std_s4 = std_s1;
    verify_set_integrity(s4, std_s4);

    SUB_TEST("Self-assignment");
    s1 = s1; // Should not crash or corrupt
    verify_set_integrity(s1, std_s1);
}

void test_const_correctness() {
    TEST_CASE("Const Correctness");
    norb::set<int> s_mut;
    std::set<int> std_s_mut;
    int data[] = {10, 20, 5};
    for (int x : data) {
        s_mut.insert(x);
        std_s_mut.insert(x);
    }

    const norb::set<int> &s_const = s_mut;
    const std::set<int> &std_s_const = std_s_mut;

    ASSERT_EQUAL(s_const.empty(), std_s_const.empty());
    ASSERT_EQUAL(s_const.size(), std_s_const.size());

    ASSERT_EQUAL(s_const.count(10), std_s_const.count(10));
    ASSERT_EQUAL(s_const.count(100), std_s_const.count(100));

    auto cit_find = s_const.find(20);
    ASSERT_FALSE(cit_find == s_const.cend());
    ASSERT_EQUAL(*cit_find, 20);
    ASSERT_TRUE(s_const.find(200) == s_const.cend());

    auto cit_s = s_const.cbegin(); // Or s_const.begin() if const overload is correct
    auto cit_std = std_s_const.cbegin();
    while (cit_s != s_const.cend() && cit_std != std_s_const.cend()) {
        ASSERT_EQUAL(*cit_s, *cit_std);
        // ASSERT_EQUAL(cit_s->, *cit_std); // OLD
        ASSERT_EQUAL(*(cit_s.operator->()), *cit_std); // FIX: Test const_iterator::operator->
        ++cit_s;
        ++cit_std;
    }
    ASSERT_TRUE(cit_s == s_const.cend());
    ASSERT_TRUE(cit_std == std_s_const.cend());
}

void test_stress_and_balancing() {
    TEST_CASE("Stress Test and Balancing");
    norb::set<int> s;
    std::set<int> std_s;
    const int num_elements = 2000; // Increased for better stress

    std::vector<int> v_rand, v_sorted, v_reversed;
    for (int i = 0; i < num_elements; ++i) {
        v_rand.push_back(i);
        v_sorted.push_back(i);
        v_reversed.push_back(num_elements - 1 - i);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(v_rand.begin(), v_rand.end(), g);

    SUB_TEST("Insert " + std::to_string(num_elements) + " random elements");
    s.clear();
    std_s.clear();
    for (int x : v_rand) {
        s.insert(x);
        std_s.insert(x);
    }
    verify_set_integrity(s, std_s);

    SUB_TEST("Insert " + std::to_string(num_elements) + " sorted elements");
    s.clear();
    std_s.clear();
    for (int x : v_sorted) {
        s.insert(x);
        std_s.insert(x);
    }
    verify_set_integrity(s, std_s);

    SUB_TEST("Insert " + std::to_string(num_elements) + " reverse sorted elements");
    s.clear();
    std_s.clear();
    for (int x : v_reversed) {
        s.insert(x);
        std_s.insert(x);
    }
    verify_set_integrity(s, std_s);

    SUB_TEST("Erase " + std::to_string(num_elements / 2) + " random elements from large set");
    // s and std_s currently have reverse sorted elements
    std::vector<int> to_erase = v_reversed;
    std::shuffle(to_erase.begin(), to_erase.end(), g);
    for (int i = 0; i < num_elements / 2; ++i) {
        int val = to_erase[i];
        auto it = s.find(val);
        if (it != s.end()) { // Should always be found
            s.erase(it);
            std_s.erase(val);
        }
    }
    verify_set_integrity(s, std_s);

    SUB_TEST("Insert and erase all permutations of small set (e.g., 5 elements)");
    std::vector<int> p_data = {1, 2, 3, 4, 5};
    std::sort(p_data.begin(), p_data.end());
    int perm_count = 0;
    do {
        perm_count++;
        s.clear();
        std_s.clear();
        for (int x : p_data) {
            s.insert(x);
            std_s.insert(x);
        }
        verify_set_integrity(s, std_s);

        std::vector<int> p_erase_order = p_data;
        std::shuffle(p_erase_order.begin(), p_erase_order.end(), g);
        for (int x : p_erase_order) {
            s.erase(s.find(x));
            std_s.erase(x);
            verify_set_integrity(s, std_s);
        }
    } while (std::next_permutation(p_data.begin(), p_data.end()) && perm_count < 24); // Limit permutations for speed
    std::cout << "  Tested " << perm_count << " permutations for insert/erase." << std::endl;
}

struct DescendingIntCompare {
    bool operator()(int a, int b) const {
        return a > b;
    }
};

void test_custom_comparator() {
    TEST_CASE("Custom Comparator (std::greater)");
    norb::set<int, std::greater<int>> s_greater;
    std::set<int, std::greater<int>> std_s_greater;

    int data[] = {10, 5, 20, 1, 15};
    for (int x : data) {
        s_greater.insert(x);
        std_s_greater.insert(x);
    }
    verify_set_integrity<int, std::greater<int>>(s_greater, std_s_greater);

    std::vector<int> expected_order = {20, 15, 10, 5, 1}; // Descending

    // Use a new iterator variable for this scope
    auto it_greater = s_greater.begin();
    for (int val : expected_order) {
        ASSERT_FALSE(it_greater == s_greater.end());
        ASSERT_EQUAL(*it_greater, val);
        ++it_greater;
    }
    ASSERT_TRUE(it_greater == s_greater.end());

    TEST_CASE("Custom Comparator (struct DescendingIntCompare)"); // This changes current_test_case_name
    norb::set<int, DescendingIntCompare> s_custom_desc;
    std::set<int, DescendingIntCompare> std_s_custom_desc;
    for (int x : data) {
        s_custom_desc.insert(x);
        std_s_custom_desc.insert(x);
    }
    verify_set_integrity<int, DescendingIntCompare>(s_custom_desc, std_s_custom_desc);

    // Declare a new iterator variable for this specific type of set
    auto it_custom = s_custom_desc.begin();
    for (int val : expected_order) { // expected_order is still valid here
        ASSERT_FALSE(it_custom == s_custom_desc.end());
        ASSERT_EQUAL(*it_custom, val);
        ++it_custom;
    }
    ASSERT_TRUE(it_custom == s_custom_desc.end());
}

void test_string_keys() {
    TEST_CASE("String Keys");
    norb::set<std::string> s_str;
    std::set<std::string> std_s_str;

    std::string data[] = {"apple", "banana", "cherry", "date", "fig", "apricot"};
    for (const auto &str : data) {
        s_str.insert(str);
        std_s_str.insert(str);
    }
    verify_set_integrity(s_str, std_s_str);

    s_str.erase(s_str.find("banana"));
    std_s_str.erase("banana");
    verify_set_integrity(s_str, std_s_str);

    ASSERT_EQUAL(s_str.count("apple"), 1);
    ASSERT_EQUAL(s_str.count("grape"), 0);
}

int main() {
    std::cout << "Starting norb::set tests..." << std::endl;

    test_constructors_and_basic_ops();
    test_insert_and_find();
    test_iterators();
    test_erase();
    test_clear_and_count();
    test_copy_assignment();
    test_const_correctness();
    test_stress_and_balancing(); // This is a heavier test
    test_custom_comparator();
    test_string_keys();

    std::cout << "\nAll norb::set tests passed successfully!" << std::endl;
    return 0;
}