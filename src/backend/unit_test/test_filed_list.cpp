#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cassert>
#include <cstdio>     // For std::remove
#include <algorithm>  // For std::max
#include <stdexcept>  // For std::range_error

// --- Start: norb::filesystem mock ---
namespace norb {
namespace filesystem {

// Ensures file exists, creates if not.
void fassert_mock(const std::string& f_name) {
    std::fstream f;
    // Try to open for read/write first. If it exists, this is fine.
    f.open(f_name, std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        // If it couldn't be opened (e.g., doesn't exist), try to create it.
        f.open(f_name, std::ios::out | std::ios::binary | std::ios::trunc); // Create/truncate
        if (f.is_open()) {
            f.close(); // Successfully created, close it.
        } else {
            // This is a more serious issue if creation fails.
            std::cerr << "FATAL: fassert_mock could not create/ensure file " << f_name << std::endl;
            // In a real test setup, this might be an unrecoverable error for the test.
        }
    } else {
        // File was already openable, just close it.
        f.close();
    }
}

bool is_empty(std::fstream& f_stream) {
    if (!f_stream.is_open()) return true; // If not open, treat as empty or error
    
    auto original_pos_g = f_stream.tellg();
    f_stream.seekg(0, std::ios::end);
    bool empty = (f_stream.tellg() == std::streampos(0));
    f_stream.seekg(original_pos_g); // Restore position
    f_stream.clear(); // Clear EOF/fail flags if any were set by seekg to end
    return empty;
}

template <typename T>
void binary_read(std::fstream& f_stream, T& value) {
    if (!f_stream.is_open()) return; // Or assert/throw
    f_stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    // Caller should check f_stream.good() or f_stream.gcount()
}

template <typename T>
void binary_write(std::fstream& f_stream, const T& value) {
    if (!f_stream.is_open()) return; // Or assert/throw
    f_stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    f_stream.flush(); // Important for tests to ensure data is on disk
    // Caller should check f_stream.good()
}

// Alias to match the original code's usage in the provided classes
void fassert(const std::string& f_name) {
    fassert_mock(f_name);
}

} // namespace filesystem
} // namespace norb
// --- End: norb::filesystem mock ---


// --- Start: Pasted and slightly modified class definitions ---
// (Original code from prompt, with filesystem calls using mocks,
//  one compile fix in FiledSegmentList::allocate, and improved getPos return type)
namespace norb {
    template <typename T_> class FiledNaiveList {
      public:
        explicit FiledNaiveList(const std::string &f_name_) {
            f_name = f_name_;
            filesystem::fassert(f_name); // Uses our mock
            f_stream.open(f_name, file_open_mode);
            assert(f_stream.good() && "FiledNaiveList: f_stream not good after open in constructor");
            f_stream.seekg(0);
            if (!filesystem::is_empty(f_stream)) {
                filesystem::binary_read(f_stream, size_);
                // Check stream state after read
                assert((f_stream.good() || f_stream.eof()) && "FiledNaiveList: f_stream bad after reading size_");
                if (!f_stream.good() && f_stream.eof() && filesystem::is_empty(f_stream)) { // e.g. file exists but is 0 bytes
                    size_ = 0; // Correct size_ if EOF on empty file
                    f_stream.clear(); // Clear EOF state
                }
            } else {
                f_stream.clear(); // Clear flags if is_empty set eof
                f_stream.seekp(0);
                filesystem::binary_write(f_stream, size_); // size_ is 0 initially
                assert(f_stream.good() && "FiledNaiveList: f_stream bad after writing initial size_");
            }
        }
        explicit FiledNaiveList(const char *f_name_) : FiledNaiveList(std::string(f_name_)) {
        }

        ~FiledNaiveList() {
            if (f_stream.is_open()) f_stream.close();
        }

        T_ get(const int index) const {
            assert(f_stream.good() && "FiledNaiveList: f_stream not good at start of get");
            if (index >= size_ || index < 0) {
                throw std::range_error("norb::FiledNaiveList: INDEX OUT OF RANGE!");
            }
            f_stream.clear(); // Clear any previous error flags
            f_stream.seekg(getPos(index), std::ios::beg);
            assert(f_stream.good() && "FiledNaiveList: f_stream not good after seekg in get");
            T_ ret{}; // Default initialize
            filesystem::binary_read(f_stream, ret);
            assert((f_stream.good() || f_stream.eof()) && "FiledNaiveList: f_stream not good after binary_read in get");
            return ret;
        }

        T_ set(const int index, T_ to) { // Pass T_ by value as in original
            assert(f_stream.good() && "FiledNaiveList: f_stream not good at start of set");
            if (index < 0) {
                throw std::range_error("norb::FiledNaiveList: INDEX OUT OF RANGE! (negative)");
            }
            
            bool extended = false;
            if (index >= size_) { 
                size_ = index + 1;
                extended = true;
            }
            
            f_stream.clear(); // Clear any previous error flags
            if(extended) {
                std::streampos current_p_before_size_write = f_stream.tellp();
                f_stream.seekp(0);
                filesystem::binary_write(f_stream, size_);
                assert(f_stream.good() && "FiledNaiveList: f_stream not good after writing size_ in set");
                if(current_p_before_size_write != std::streampos(-1)) { // try to restore if it was valid
                     f_stream.seekp(current_p_before_size_write); // May not be useful if next op is seekp(getPos)
                }
            }

            f_stream.seekp(getPos(index), std::ios::beg);
            assert(f_stream.good() && "FiledNaiveList: f_stream not good after seekp in set");
            filesystem::binary_write(f_stream, to);
            assert(f_stream.good() && "FiledNaiveList: f_stream not good after binary_write in set");
            return to;
        }

        int size() const {
            return size_;
        }

        T_ push_back(T_ to) { // Pass T_ by value
            assert(f_stream.good() && "FiledNaiveList: f_stream not good at start of push_back");
            const auto ret = set(size_, to); 
            assert(f_stream.good() && "FiledNaiveList: f_stream not good after set in push_back");
            return ret;
        }

      private:
        static constexpr auto file_open_mode = std::ios::binary | std::ios::in | std::ios::out;
        // static constexpr int sizeof_t = sizeof(T_); // Original, can use sizeof(T_) directly

        int size_ = 0;
        std::string f_name;
        mutable std::fstream f_stream;

        std::streamoff getPos(const int index) const { 
            return static_cast<std::streamoff>(sizeof(int)) + static_cast<std::streamoff>(sizeof(T_)) * index;
        }
    };

    template <typename T_> class FiledSegmentList {
      public:
        explicit FiledSegmentList(const std::string &f_name_) {
            f_name = f_name_;
            filesystem::fassert(f_name); // Uses our mock
            f_stream.open(f_name, file_open_mode);
            assert(f_stream.good() && "FiledSegmentList: f_stream not good after open in constructor");
            f_stream.seekg(0);
            if (!filesystem::is_empty(f_stream)) {
                filesystem::binary_read(f_stream, size_);
                assert((f_stream.good() || f_stream.eof()) && "FiledSegmentList: f_stream bad after reading size_");
                 if (!f_stream.good() && f_stream.eof() && filesystem::is_empty(f_stream)) {
                    size_ = 0; 
                    f_stream.clear(); 
                }
            } else {
                f_stream.clear();
                f_stream.seekp(0);
                filesystem::binary_write(f_stream, size_); // size_ is 0 initially
                assert(f_stream.good() && "FiledSegmentList: f_stream bad after writing initial size_");
            }
        }
        explicit FiledSegmentList(const char *f_name_) : FiledSegmentList(std::string(f_name_)) {
        }

        ~FiledSegmentList() {
            if (f_stream.is_open()) f_stream.close();
        }

        struct Segment {
            int cur;  // Start index of this segment in the global list
            int size; // Number of elements in this segment
        };

        T_ get(const Segment &seg, const int index) const {
            assert(f_stream.good() && "FiledSegmentList: f_stream not good at start of get");
            if (index < 0 || index >= seg.size) {
                throw std::range_error("norb::FiledSegmentList: INDEX OUT OF RANGE!");
            }
            f_stream.clear();
            f_stream.seekg(getPos(seg.cur + index));
            assert(f_stream.good() && "FiledSegmentList: f_stream not good after seekg in get");
            T_ ret{}; // Default initialize
            filesystem::binary_read(f_stream, ret);
            assert((f_stream.good() || f_stream.eof()) && "FiledSegmentList: f_stream not good after binary_read in get");
            return ret;
        }

        T_ set(const Segment &seg, const int index, const T_ &to) const { // Pass T_ by const ref
            assert(f_stream.good() && "FiledSegmentList: f_stream not good at start of set");
            if (index < 0 || index >= seg.size) {
                throw std::range_error("norb::FiledSegmentList: INDEX OUT OF RANGE!");
            }
            f_stream.clear();
            f_stream.seekp(getPos(seg.cur + index)); // Use seekp for writing
            assert(f_stream.good() && "FiledSegmentList: f_stream not good after seekp in set");
            filesystem::binary_write(f_stream, to);
            assert(f_stream.good() && "FiledSegmentList: f_stream not good after binary_write in set");
            return to;
        }

        Segment allocate(const int segment_len) { 
            assert(f_stream.good() && "FiledSegmentList: f_stream not good at start of allocate");
            
            const Segment seg = {size_, segment_len}; // Corrected: size_ is current total, segment_len is for this segment
            
            int old_total_size = size_;
            size_ += segment_len;

            f_stream.clear();
            // Problematic original logic for "ensuring allocation":
            if (segment_len > 0) { 
                // This seeks to one element *past* the new total size and writes one byte.
                // This DOES NOT initialize the segment's data area.
                f_stream.seekp(getPos(old_total_size + segment_len)); 
                assert(f_stream.good() && "FiledSegmentList: seekp past end failed in allocate");
                f_stream.put('\0'); 
                assert(f_stream.good() && "FiledSegmentList: put past end failed in allocate");
            }
            
            std::streampos current_p_before_size_write = f_stream.tellp();
            f_stream.seekp(0);
            filesystem::binary_write(f_stream, size_);
            assert(f_stream.good() && "FiledSegmentList: f_stream not good after writing new total size_ in allocate");
            if(current_p_before_size_write != std::streampos(-1)) {
                 f_stream.seekp(current_p_before_size_write);
            }
            return seg;
        }

      private:
        static constexpr auto file_open_mode = std::ios::binary | std::ios::in | std::ios::out;
        // static constexpr int sizeof_t = sizeof(T_); // Original

        int size_ = 0; // Total number of T_ elements in the file
        std::string f_name;
        mutable std::fstream f_stream;

        std::streamoff getPos(const int index) const { 
            return static_cast<std::streamoff>(sizeof(int)) + static_cast<std::streamoff>(sizeof(T_)) * index;
        }
    };
} // namespace norb
// --- End: Pasted and slightly modified class definitions ---


// --- Test Utilities ---
const std::string TEST_NAIVE_LIST_FILE = "test_naive_list.dat";
const std::string TEST_SEGMENT_LIST_FILE = "test_segment_list.dat";
int tests_passed = 0;
int tests_failed = 0;

void cleanup_file(const std::string& filename) {
    std::remove(filename.c_str());
}

// Helper to run a test function
void run_test(void (*test_func)(), const std::string& test_name) {
    std::cout << "Running test: " << test_name << " ... ";
    // Always clean up before test to ensure fresh state
    if (test_name.find("NaiveList") != std::string::npos) {
        cleanup_file(TEST_NAIVE_LIST_FILE);
    } else if (test_name.find("SegmentList") != std::string::npos) {
        cleanup_file(TEST_SEGMENT_LIST_FILE);
    }

    try {
        test_func();
        std::cout << "PASSED" << std::endl;
        tests_passed++;
    } catch (const std::range_error& e) {
        std::cout << "FAILED (std::range_error: " << e.what() << ")" << std::endl;
        tests_failed++;
    } catch (const std::exception& e) {
        std::cout << "FAILED (std::exception: " << e.what() << ")" << std::endl;
        tests_failed++;
    } catch (...) {
        std::cout << "FAILED (unknown exception)" << std::endl;
        tests_failed++;
    }
}

// Helper to read the total size directly from file header for verification
int read_total_size_header(const std::string& filename) {
    std::fstream f(filename, std::ios::binary | std::ios::in);
    if (!f.is_open()) return -1; 
    int total_size = 0;
    f.read(reinterpret_cast<char*>(&total_size), sizeof(int));
    if (!f.good() && f.gcount() != sizeof(int)) return -2; // Read error or incomplete read
    return total_size;
}

template<typename T>
T read_value_from_file(const std::string& filename, std::streamoff pos, bool& read_success) {
    std::fstream f(filename, std::ios::binary | std::ios::in);
    T val{}; 
    read_success = false;
    if(!f.is_open()) {
        std::cerr << "read_value_from_file: could not open file " << filename << std::endl;
        return val;
    }
    f.seekg(pos);
    if(!f.good()) {
        std::cerr << "read_value_from_file: seekg to " << pos << " failed on " << filename << std::endl;
        return val;
    }
    f.read(reinterpret_cast<char*>(&val), sizeof(T));
    if(f.gcount() == sizeof(T)) {
        read_success = true;
    } else {
         std::cerr << "read_value_from_file: read " << f.gcount() << " bytes, expected " << sizeof(T) << " from " << filename << " at " << pos << std::endl;
    }
    return val;
}

// --- FiledNaiveList Tests ---
void test_naive_list_creation_empty_file() {
    norb::FiledNaiveList<int> list(TEST_NAIVE_LIST_FILE);
    assert(list.size() == 0 && "Initial size not 0 for new file");
    assert(read_total_size_header(TEST_NAIVE_LIST_FILE) == 0 && "File header size not 0 for new file");
}

void test_naive_list_creation_existing_file() {
    // Prepare an existing file
    {
        std::fstream f(TEST_NAIVE_LIST_FILE, std::ios::binary | std::ios::out | std::ios::trunc);
        assert(f.is_open());
        int initial_total_size = 3;
        f.write(reinterpret_cast<const char*>(&initial_total_size), sizeof(int));
        for (int i = 0; i < initial_total_size; ++i) {
            int val = i * 10;
            f.write(reinterpret_cast<const char*>(&val), sizeof(int));
        }
        f.close();
    }
    norb::FiledNaiveList<int> list(TEST_NAIVE_LIST_FILE);
    assert(list.size() == 3 && "Size not read correctly from existing file");
    assert(list.get(0) == 0 && "Existing data [0] not read correctly");
    assert(list.get(1) == 10 && "Existing data [1] not read correctly");
    assert(list.get(2) == 20 && "Existing data [2] not read correctly");
}

void test_naive_list_set_get() {
    norb::FiledNaiveList<int> list(TEST_NAIVE_LIST_FILE);
    list.set(0, 100);
    list.set(1, 200);
    assert(list.size() == 2 && "Size not updated correctly after set");
    assert(list.get(0) == 100 && "get(0) failed");
    assert(list.get(1) == 200 && "get(1) failed");
    assert(read_total_size_header(TEST_NAIVE_LIST_FILE) == 2 && "File header size incorrect after set");

    list.set(0, 101); // Overwrite
    assert(list.size() == 2 && "Size changed on overwrite");
    assert(list.get(0) == 101 && "Overwrite failed");
}

void test_naive_list_push_back() {
    norb::FiledNaiveList<double> list(TEST_NAIVE_LIST_FILE);
    list.push_back(1.1);
    assert(list.size() == 1 && "Size not 1 after first push_back");
    assert(list.get(0) == 1.1 && "First push_back value incorrect");

    list.push_back(2.2);
    assert(list.size() == 2 && "Size not 2 after second push_back");
    assert(list.get(0) == 1.1 && "Original value changed after second push_back");
    assert(list.get(1) == 2.2 && "Second push_back value incorrect");
    assert(read_total_size_header(TEST_NAIVE_LIST_FILE) == 2 && "File header size incorrect after push_back");
}

void test_naive_list_out_of_bounds() {
    norb::FiledNaiveList<int> list(TEST_NAIVE_LIST_FILE);
    list.push_back(10); // size is 1, valid index 0

    bool caught = false;
    try { list.get(1); } catch (const std::range_error&) { caught = true; }
    assert(caught && "get(1) OOB not caught");

    caught = false;
    try { list.get(-1); } catch (const std::range_error&) { caught = true; }
    assert(caught && "get(-1) OOB not caught");
    
    caught = false;
    try { list.set(-1, 0); } catch (const std::range_error&) { caught = true; }
    assert(caught && "set(-1, val) OOB not caught");
}

void test_naive_list_set_extends_and_reads_potential_garbage_in_hole() {
    norb::FiledNaiveList<int> list(TEST_NAIVE_LIST_FILE); // File is new, likely zero-filled by OS on extension
    list.set(0, 10); // size becomes 1
    list.set(2, 30); // size becomes 3. Element at index 1 is a "hole"
    
    assert(list.size() == 3 && "Size not 3 after extending set");
    assert(list.get(0) == 10 && "Value at [0] incorrect");
    assert(list.get(2) == 30 && "Value at [2] incorrect");

    std::cout << "[INFO] NaiveList: Reading from 'hole' at index 1: ";
    int val_in_hole = list.get(1); // Reads uninitialized part of file
    std::cout << val_in_hole << ". (This value is uninitialized, often 0 on new files)." << std::endl;
    // For a new file, this often reads as 0 if the OS zero-fills pages.
    // We don't assert val_in_hole == 0 because it's not guaranteed by the class.
    // The main point is that get() doesn't crash and the surrounding elements are fine.
}


// --- FiledSegmentList Tests ---
void test_segment_list_creation_empty_file() {
    norb::FiledSegmentList<int> list(TEST_SEGMENT_LIST_FILE);
    assert(read_total_size_header(TEST_SEGMENT_LIST_FILE) == 0 && "File header size not 0 for new segment list file");
}

void test_segment_list_creation_existing_file() {
    {
        std::fstream f(TEST_SEGMENT_LIST_FILE, std::ios::binary | std::ios::out | std::ios::trunc);
        assert(f.is_open());
        int initial_total_size = 2;
        f.write(reinterpret_cast<const char*>(&initial_total_size), sizeof(int));
        int val1 = 10, val2 = 20;
        f.write(reinterpret_cast<const char*>(&val1), sizeof(int));
        f.write(reinterpret_cast<const char*>(&val2), sizeof(int));
        f.close();
    }
    norb::FiledSegmentList<int> list(TEST_SEGMENT_LIST_FILE);
    auto seg = list.allocate(1); // Should append after existing 2 elements
    assert(seg.cur == 2 && "New segment 'cur' not after existing elements");
    assert(seg.size == 1 && "New segment 'size' incorrect");
    assert(read_total_size_header(TEST_SEGMENT_LIST_FILE) == 3 && "Total size in file header incorrect");
}

void test_segment_list_allocate_and_check_uninitialized_read() {
    norb::FiledSegmentList<int> list(TEST_SEGMENT_LIST_FILE);
    auto seg = list.allocate(2); 

    assert(seg.cur == 0 && "Segment cur incorrect");
    assert(seg.size == 2 && "Segment size incorrect");
    assert(read_total_size_header(TEST_SEGMENT_LIST_FILE) == 2 && "Total size in header incorrect");

    std::cout << "[INFO] SegmentList: Reading from uninitialized segment: " << std::endl;
    // The values read here are from an uninitialized part of the file due to allocate's behavior.
    int val0 = list.get(seg, 0);
    int val1 = list.get(seg, 1);
    std::cout << "  val[0]=" << val0 << ", val[1]=" << val1 << ". (These are uninitialized)." << std::endl;
    // We don't assert specific values, just that get() doesn't crash.
}

void test_segment_list_set_get() {
    norb::FiledSegmentList<char> list(TEST_SEGMENT_LIST_FILE);
    auto seg = list.allocate(3);

    list.set(seg, 0, 'a');
    list.set(seg, 1, 'b');
    list.set(seg, 2, 'c');

    assert(list.get(seg, 0) == 'a' && "get/set char [0] failed");
    assert(list.get(seg, 1) == 'b' && "get/set char [1] failed");
    assert(list.get(seg, 2) == 'c' && "get/set char [2] failed");

    bool read_success;
    std::streamoff base_pos = sizeof(int);
    assert(read_value_from_file<char>(TEST_SEGMENT_LIST_FILE, base_pos + 0 * sizeof(char), read_success) == 'a' && read_success);
    assert(read_value_from_file<char>(TEST_SEGMENT_LIST_FILE, base_pos + 1 * sizeof(char), read_success) == 'b' && read_success);
    assert(read_value_from_file<char>(TEST_SEGMENT_LIST_FILE, base_pos + 2 * sizeof(char), read_success) == 'c' && read_success);
}

void test_segment_list_multiple_segments() {
    norb::FiledSegmentList<int> list(TEST_SEGMENT_LIST_FILE);

    auto seg1 = list.allocate(2); 
    list.set(seg1, 0, 10);
    list.set(seg1, 1, 20);
    assert(read_total_size_header(TEST_SEGMENT_LIST_FILE) == 2);

    auto seg2 = list.allocate(3); 
    assert(seg2.cur == 2 && "seg2.cur incorrect");
    assert(seg2.size == 3 && "seg2.size incorrect");
    list.set(seg2, 0, 30);
    list.set(seg2, 1, 40);
    list.set(seg2, 2, 50);
    assert(read_total_size_header(TEST_SEGMENT_LIST_FILE) == 5);

    assert(list.get(seg1, 0) == 10 && "seg1[0] val error");
    assert(list.get(seg1, 1) == 20 && "seg1[1] val error");
    assert(list.get(seg2, 0) == 30 && "seg2[0] val error");
    assert(list.get(seg2, 1) == 40 && "seg2[1] val error");
    assert(list.get(seg2, 2) == 50 && "seg2[2] val error");
}

void test_segment_list_out_of_bounds() {
    norb::FiledSegmentList<int> list(TEST_SEGMENT_LIST_FILE);
    auto seg = list.allocate(1); // cur=0, size=1. Valid index for this segment: 0

    bool caught = false;
    try { list.get(seg, 1); } catch (const std::range_error&) { caught = true; }
    assert(caught && "SegmentList get(seg, 1) OOB not caught");

    caught = false;
    try { list.get(seg, -1); } catch (const std::range_error&) { caught = true; }
    assert(caught && "SegmentList get(seg, -1) OOB not caught");

    caught = false;
    try { list.set(seg, 1, 100); } catch (const std::range_error&) { caught = true; }
    assert(caught && "SegmentList set(seg, 1, val) OOB not caught");
    
    caught = false;
    try { list.set(seg, -1, 100); } catch (const std::range_error&) { caught = true; }
    assert(caught && "SegmentList set(seg, -1, val) OOB not caught");
}


int main() {
    std::cout << "Starting Norb List Tests..." << std::endl;

    run_test(test_naive_list_creation_empty_file, "NaiveList: Creation on Empty File");
    run_test(test_naive_list_creation_existing_file, "NaiveList: Creation on Existing File");
    run_test(test_naive_list_set_get, "NaiveList: Set and Get");
    run_test(test_naive_list_push_back, "NaiveList: Push Back");
    run_test(test_naive_list_out_of_bounds, "NaiveList: Out of Bounds Access");
    run_test(test_naive_list_set_extends_and_reads_potential_garbage_in_hole, "NaiveList: Set Extends (Hole Behavior)");

    run_test(test_segment_list_creation_empty_file, "SegmentList: Creation on Empty File");
    run_test(test_segment_list_creation_existing_file, "SegmentList: Creation on Existing File");
    run_test(test_segment_list_allocate_and_check_uninitialized_read, "SegmentList: Allocate (Uninitialized Read Behavior)");
    run_test(test_segment_list_set_get, "SegmentList: Set and Get in Segment");
    run_test(test_segment_list_multiple_segments, "SegmentList: Multiple Segments");
    run_test(test_segment_list_out_of_bounds, "SegmentList: Out of Bounds Access in Segment");

    std::cout << "\n--- Test Summary ---" << std::endl;
    std::cout << "Tests Passed: " << tests_passed << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;

    // Final cleanup (optional, as individual tests clean up)
    cleanup_file(TEST_NAIVE_LIST_FILE);
    cleanup_file(TEST_SEGMENT_LIST_FILE);

    return (tests_failed == 0) ? 0 : 1;
}