// norb_set_pressure_test.cpp

#include "stlite/set.hpp" // Your set implementation

#include <iostream>
#include <vector>
#include <string>
#include <algorithm> // For std::sort, std::shuffle
#include <random>    // For std::mt19937, std::shuffle, std::uniform_int_distribution
#include <set>       // For std::set to compare against (ground truth)
#include <stdexcept> // For std::runtime_error (if used by set or assertions)
#include <chrono>    // For timing
#include <iomanip>   // For std::fixed, std::setprecision

// --- Minimal Stubs for Testing Macros/Helpers ---
// In a real project, these would be in a shared testing utility header.
#ifndef TEST_CASE
std::string current_test_case_name_pt = ""; // Use a different name to avoid clashes if linked with other tests
#define TEST_CASE(name) \
    std::cout << "\n--- Test Case: " << name << " ---" << std::endl; \
    current_test_case_name_pt = name;
#endif

#ifndef SUB_TEST
#define SUB_TEST(name) \
    std::cout << "  Sub-test: " << name << std::endl;
#endif

#ifndef ASSERT_TRUE
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "Assertion failed in pressure test: [" << current_test_case_name_pt << "]: (" #condition ") is false, in " __FILE__ \
                  << " line " << __LINE__ << std::endl; \
        /* std::exit(EXIT_FAILURE); */ /* Avoid exit in library-like test */ \
        throw std::runtime_error("Pressure test assertion failed."); \
    }
#endif

#ifndef ASSERT_FALSE
#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))
#endif

#ifndef ASSERT_EQUAL
#define ASSERT_EQUAL(val1, val2) \
    if (!((val1) == (val2))) { \
        std::cerr << "Assertion failed in pressure test: [" << current_test_case_name_pt << "]: (" #val1 " == " #val2 ") values: (" << (val1) << " vs " << (val2) << ") in " __FILE__ \
                  << " line " << __LINE__ << std::endl; \
        /* std::exit(EXIT_FAILURE); */ \
        throw std::runtime_error("Pressure test assertion failed."); \
    }
#endif

// Minimal stub for verify_set_integrity.
// Replace with your actual verify_set_integrity if it's in a common header.
#ifndef VERIFY_SET_INTEGRITY_DEFINED
#define VERIFY_SET_INTEGRITY_DEFINED
template<typename Key, typename Compare = std::less<Key>>
void verify_set_integrity(const norb::set<Key, Compare>& n_set, const std::set<Key, Compare>& std_set) {
    ASSERT_EQUAL(n_set.size(), std_set.size());
    ASSERT_EQUAL(n_set.empty(), std_set.empty());

    if (n_set.empty()) {
        ASSERT_TRUE(n_set.cbegin() == n_set.cend()); // Use cbegin/cend for const n_set
    } else {
        ASSERT_FALSE(n_set.cbegin() == n_set.cend());
    }

    std::vector<Key> n_elements, std_elements;
    // Use const_iterators for const n_set
    for (typename norb::set<Key, Compare>::const_iterator it = n_set.cbegin(); it != n_set.cend(); ++it) {
        n_elements.push_back(*it);
    }
    for (const auto& k : std_set) {
        std_elements.push_back(k);
    }
    ASSERT_EQUAL(n_elements.size(), std_elements.size());
    for (size_t i = 0; i < n_elements.size(); ++i) {
        ASSERT_EQUAL(n_elements[i], std_elements[i]);
    }

    for(const auto& val : std_set) {
        ASSERT_TRUE(n_set.count(val) == 1);
        ASSERT_FALSE(n_set.find(val) == n_set.cend()); // Use cend for const find
        ASSERT_EQUAL(*(n_set.find(val)), val);
    }
    // Add #ifdef for n_set.check() if it's part of your set and you have debug flags
    #if defined(ENABLE_NORB_DEBUG_CHECKS) || (defined(USE_DEBUG_FLAGS) && defined(DEBUG_CHECK_UTILS))
        // const_cast might be needed if check() is not const
        // const_cast<norb::set<Key, Compare>&>(n_set).check();
        // Or, if check() is const:
        // n_set.check();
    #endif
}
#endif
// --- End Minimal Stubs ---


// Function to generate various patterns of data
std::vector<int> generate_data_pt(int size, const std::string& pattern_type) { // Renamed to avoid clashes
    std::vector<int> data(size);
    if (pattern_type == "sorted") {
        for (int i = 0; i < size; ++i) data[i] = i;
    } else if (pattern_type == "reverse_sorted") {
        for (int i = 0; i < size; ++i) data[i] = size - 1 - i;
    } else if (pattern_type == "random") {
        std::random_device rd;
        std::mt19937 g(rd());
        for (int i = 0; i < size; ++i) data[i] = i;
        std::shuffle(data.begin(), data.end(), g);
    } else if (pattern_type == "sawtooth") {
        for (int i = 0; i < size; ++i) {
            if (i % 2 == 0) data[i] = i / 2;
            else data[i] = size - 1 - (i / 2);
        }
    }
    return data;
}


void run_pressure_tests() { // Renamed main test function
    TEST_CASE("Pressure Test");

    const int NUM_OPERATIONS_LARGE = 10000000;
    const int NUM_OPERATIONS_MIXED = 50000;

    std::vector<std::string> patterns = {"sorted", "reverse_sorted", "random", "sawtooth"};

    for (const auto& pattern : patterns) {
        SUB_TEST("Large Inserts (" + pattern + ", " + std::to_string(NUM_OPERATIONS_LARGE) + " elements)");
        norb::set<int> n_set;
        std::set<int> std_set;
        std::vector<int> data = generate_data_pt(NUM_OPERATIONS_LARGE, pattern);

        auto start_time = std::chrono::high_resolution_clock::now();
        for (int val : data) {
            n_set.insert(val);
            std_set.insert(val);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration_ms = end_time - start_time;
        std::cout << "    norb::set insert time: " << std::fixed << std::setprecision(2) << duration_ms.count() << " ms" << std::endl;

        verify_set_integrity(n_set, std_set);

        SUB_TEST("Large Finds after " + pattern + " inserts");
        std::vector<int> find_data = data;
        std::random_device rd_find; // Use separate rd for different shuffles
        std::mt19937 g_find(rd_find());
        std::shuffle(find_data.begin(), find_data.end(), g_find);

        start_time = std::chrono::high_resolution_clock::now();
        for (int val : find_data) {
            ASSERT_FALSE(n_set.find(val) == n_set.end());
        }
        for (int i = 0; i < NUM_OPERATIONS_LARGE / 10; ++i) {
            ASSERT_TRUE(n_set.find(NUM_OPERATIONS_LARGE + i + 100) == n_set.end());
        }
        end_time = std::chrono::high_resolution_clock::now();
        duration_ms = end_time - start_time;
        std::cout << "    norb::set find time: " << std::fixed << std::setprecision(2) << duration_ms.count() << " ms" << std::endl;

        SUB_TEST("Large Erases after " + pattern + " inserts");
        std::vector<int> erase_data = data;
        std::random_device rd_erase; // Separate rd
        std::mt19937 g_erase(rd_erase());
        std::shuffle(erase_data.begin(), erase_data.end(), g_erase);

        start_time = std::chrono::high_resolution_clock::now();
        for (int val : erase_data) {
            auto it = n_set.find(val);
            // Only erase if found, as original data might have duplicates,
            // and std_set would only store unique ones.
            if (std_set.count(val)) { // Check if it should exist based on std_set
                 ASSERT_FALSE(it == n_set.end()); // If std_set has it, norb::set must too
                 n_set.erase(it);
                 std_set.erase(val);
            } else {
                 // If std_set doesn't have it (e.g. duplicate in input `data`),
                 // norb::set shouldn't have it either after initial sync.
                 ASSERT_TRUE(it == n_set.end() || n_set.find(val) == n_set.end());
            }
        }
        end_time = std::chrono::high_resolution_clock::now();
        duration_ms = end_time - start_time;
        std::cout << "    norb::set erase time: " << std::fixed << std::setprecision(2) << duration_ms.count() << " ms" << std::endl;

        verify_set_integrity(n_set, std_set);
        ASSERT_TRUE(n_set.empty());
        ASSERT_TRUE(std_set.empty());
    }

    SUB_TEST("Mixed Insert and Erase (" + std::to_string(NUM_OPERATIONS_MIXED * 2) + " ops)");
    norb::set<int> n_set_mixed;
    std::set<int> std_set_mixed;
    std::random_device rd_mixed;
    std::mt19937 g_mixed(rd_mixed());
    std::uniform_int_distribution<> val_dist(0, NUM_OPERATIONS_MIXED * 2);
    std::uniform_int_distribution<> op_dist(0, 2);

    std::vector<int> current_elements_tracker; // Track elements confirmed in std_set_mixed

    auto start_time_mixed = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPERATIONS_MIXED * 2; ++i) {
        int operation = op_dist(g_mixed);
        int value = val_dist(g_mixed);

        if (i > 0 && i % (NUM_OPERATIONS_MIXED / 5) == 0) {
             verify_set_integrity(n_set_mixed, std_set_mixed);
        }

        if (operation == 0) { // Insert
            auto n_pair = n_set_mixed.insert(value);
            auto std_pair = std_set_mixed.insert(value);
            ASSERT_EQUAL(n_pair.second, std_pair.second); // Check insertion success matches
            if (std_pair.second) {
                current_elements_tracker.push_back(value);
            }
        } else if (operation == 1 && !std_set_mixed.empty()) { // Erase
            int val_to_erase;
            bool erase_known = !current_elements_tracker.empty() && (g_mixed() % 3 != 0); // Bias to erase known elements

            if (erase_known) {
                std::uniform_int_distribution<> erase_idx_dist(0, current_elements_tracker.size() - 1);
                int idx_to_erase = erase_idx_dist(g_mixed);
                val_to_erase = current_elements_tracker[idx_to_erase];

                // Remove from tracker by swapping with last and popping
                std::swap(current_elements_tracker[idx_to_erase], current_elements_tracker.back());
                current_elements_tracker.pop_back();
            } else {
                val_to_erase = value; // Try to erase a potentially non-existent value
            }

            auto it_n = n_set_mixed.find(val_to_erase);
            bool n_found = (it_n != n_set_mixed.end());
            bool std_found = (std_set_mixed.count(val_to_erase) > 0);
            ASSERT_EQUAL(n_found, std_found);

            if (n_found) {
                n_set_mixed.erase(it_n);
            }
            std_set_mixed.erase(val_to_erase); // std::set::erase(key) returns count

            if (erase_known && !std_found) {
                // This case implies an issue with current_elements_tracker logic or a bug
                // std::cerr << "Warning: Tried to erase known element " << val_to_erase << " but not in std_set." << std::endl;
            }
            if (!erase_known && std_found) { // If we erased a random value that happened to be in tracker
                 auto it_tracker = std::find(current_elements_tracker.begin(), current_elements_tracker.end(), val_to_erase);
                 if (it_tracker != current_elements_tracker.end()) {
                     current_elements_tracker.erase(it_tracker);
                 }
            }

        } else { // Find
            ASSERT_EQUAL(n_set_mixed.count(value), std_set_mixed.count(value));
        }
    }
    auto end_time_mixed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_ms_mixed = end_time_mixed - start_time_mixed;
    std::cout << "    norb::set mixed ops time: " << std::fixed << std::setprecision(2) << duration_ms_mixed.count() << " ms" << std::endl;

    verify_set_integrity(n_set_mixed, std_set_mixed);
    std::cout << "    Final size after mixed ops: " << n_set_mixed.size() << std::endl;


    SUB_TEST("Repeated insert/erase of same small set of values");
    const int small_set_size = 100;
    const int repeat_ops = 20000; // Increased ops
    std::vector<int> small_data = generate_data_pt(small_set_size, "random");

    norb::set<int> n_set_repeat;
    std::set<int> std_set_repeat;

    start_time_mixed = std::chrono::high_resolution_clock::now();
    for(int i=0; i < repeat_ops; ++i) {
        int val_idx = g_mixed() % small_set_size; // Random index from small_data
        int val = small_data[val_idx];

        // Alternate between inserting and erasing the chosen value
        if (n_set_repeat.count(val)) { // If it's there, erase it
            n_set_repeat.erase(n_set_repeat.find(val));
            std_set_repeat.erase(val);
        } else { // If it's not there, insert it
            n_set_repeat.insert(val);
            std_set_repeat.insert(val);
        }

        if (i > 0 && i % (repeat_ops / 20) == 0) { // Periodic check
            verify_set_integrity(n_set_repeat, std_set_repeat);
        }
    }
    end_time_mixed = std::chrono::high_resolution_clock::now();
    duration_ms_mixed = end_time_mixed - start_time_mixed;
    std::cout << "    norb::set repeated small ops time: " << std::fixed << std::setprecision(2) << duration_ms_mixed.count() << " ms" << std::endl;
    verify_set_integrity(n_set_repeat, std_set_repeat);
}

// Example main to run this specific test file
// You would typically integrate run_pressure_tests() into a larger test suite runner.
int main() {
    std::cout << "Starting norb::set PRESSURE tests..." << std::endl;
    try {
        run_pressure_tests();
        std::cout << "\nAll norb::set PRESSURE tests passed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "A pressure test assertion failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown exception occurred during pressure tests." << std::endl;
        return 1;
    }
    return 0;
}