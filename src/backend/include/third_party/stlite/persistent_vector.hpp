#pragma once

#include "utils.hpp"
#include "vector.hpp"

#include <fstream>
#include <string>

namespace norb {
    template <typename T> class PersistentVector {
        norb::vector<T> data;
        std::fstream f_stream;
        static constexpr auto file_open_mode = std::ios::binary | std::ios::in | std::ios::out;

      public:
        void flush() {
            f_stream.seekp(0, std::ios::beg);
            int size_ = data.size();
            filesystem::binary_write(f_stream, size_);
            for (const auto &item : data) {
                filesystem::binary_write(f_stream, item);
            }
            f_stream.flush(); // Ensure data is written
        }

        PersistentVector(const std::string &file_name) {
            filesystem::fassert(file_name);
            f_stream.open(file_name, file_open_mode);
            assert(f_stream.good());
            f_stream.seekg(0);
            if (!filesystem::is_empty(f_stream)) {
                // Read the size from the disk.
                int size_;
                filesystem::binary_read(f_stream, size_);
                // Read the data from the disk.
                for (int i = 0; i < size_; ++i) {
                    T item;
                    filesystem::binary_read(f_stream, item);
                    data.push_back(item);
                }
            } else {
                // Write a zero to the disk.
                f_stream.seekp(0);
                int size_ = 0;
                filesystem::binary_write(f_stream, size_);
            }
            assert(f_stream.good());
        }

        ~PersistentVector() {
            flush(); // Ensure data is written before destruction
        }

        // all the following functions will modify the data in the memory
        void push_back(const T &item) {
            data.push_back(item);
        }

        void pop_back() {
            data.pop_back();
        }

        T &operator[](size_t index) {
            return data[index];
        }

        const T &operator[](size_t index) const {
            return data[index];
        }

        size_t size() const {
            return data.size();
        }

        void clear() {
            data.clear();
        }

        // const iterator
        class const_iterator {
            const PersistentVector<T> *vec;
            size_t index;

          public:
            const_iterator(const PersistentVector<T> *v, size_t idx) : vec(v), index(idx) {
            }

            const T &operator*() const {
                return (*vec)[index];
            }

            const_iterator &operator++() {
                ++index;
                return *this;
            }

            bool operator!=(const const_iterator &other) const {
                return index != other.index || vec != other.vec;
            }
        };

        const_iterator begin() const {
            return const_iterator(this, 0);
        }

        const_iterator end() const {
            return const_iterator(this, data.size());
        }
    };
} // namespace norb