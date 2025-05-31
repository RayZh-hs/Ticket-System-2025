#pragma once

#include "stlite/fixed_string.hpp"
#include "stlite/vector.hpp"
#include "settings.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

namespace norb
{
    namespace filesystem
    {
        // Check whether a fstream is empty.
        inline bool is_empty(std::fstream& f)
        {
            const std::streampos pos = f.tellg();
            // Change Note: using seek(0) on an empty file will disrupt the fstream f.
            f.seekg(0, std::ios::beg);
            const auto begin = f.tellg();
            f.seekg(0, std::ios::end);
            const auto end = f.tellg();
            f.seekg(pos);
            assert(f.good());
            return begin == end;
        }

        // Create file when it doesn't exist.
        // ! Can only be called when no other fstreams are pointing to that file !
        inline void fassert(const std::string& path)
        {
            std::fstream f;
            f.open(path, std::ios::app);
            f.close();
        }

        // Get the size of a file in bytes
        inline unsigned long get_size(const std::string& path)
        {
            std::fstream f(path, std::ios::in | std::ios::ate);
            return f.tellg();
        }

        // Binary read from a file.
        template <typename T_>
        void binary_read(std::fstream& f, T_& item)
        {
            f.read(reinterpret_cast<char*>(&item), sizeof(item));
        }

        // Binary write into a file.
        template <typename T_>
        void binary_write(std::fstream& f, const T_& item)
        {
            f.write(reinterpret_cast<const char*>(&item), sizeof(item));
        }

        // Binary write into a file.
        template <typename T_>
        void binary_write(std::fstream& f, T_& item, const int size_)
        {
            f.write(reinterpret_cast<char*>(&item), size_);
        }

        // Clear all texts within a stream and reopen it.
        inline void trunc(std::fstream& f, const std::string& file_name,
                          const std::_Ios_Openmode mode)
        {
            f.close();
            remove(file_name.c_str());
            fassert(file_name);
            f.open(file_name, mode);
        }
    } // namespace filesystem

    namespace hash
    {
        // a collection of commonly used hashing algorithms
        template <typename T>
        concept HashMethod = requires(T t, const std::string& s)
        {
            typename T::hash_t;
            { T::hash(s) } -> std::same_as<typename T::hash_t>;
        };

        template <size_t length>
        struct NoHash
        {
            using hash_t = norb::FixedString<length>;
            static hash_t hash(const std::string& s) { return hash_t(s); }
        };

        template <typename integer_t = uint64_t, integer_t multiplier = 131,
                  integer_t salt = 227, integer_t initial_value = 0>
        struct NaiveHash
        {
            using hash_t = integer_t;

            static hash_t hash(const std::string& str)
            {
                hash_t hash = initial_value;
                for (const unsigned char i : str)
                {
                    hash = hash * multiplier + static_cast<hash_t>(i) + salt;
                }
                return hash;
            }
        };

        struct Fnv1aHash
        {
            using hash_t = uint32_t;

            static hash_t hash(const std::string& str)
            {
                // FNV constants for 32-bit hash
                constexpr hash_t fnv_prime = 16777619U;
                constexpr hash_t fnv_offset_basis = 2166136261U;

                hash_t hash = fnv_offset_basis;

                // Iterate over bytes as unsigned char
                for (const unsigned char c : str)
                {
                    hash ^= static_cast<hash_t>(c); // XOR with byte
                    hash *= fnv_prime; // Multiply by prime
                }
                return hash;
            }
        };

        struct Fnv1a64Hash
        {
            using hash_t = uint64_t;

            static hash_t hash(const std::string& str)
            {
                uint64_t hash_val = 0xcbf29ce484222325ULL;
                for (unsigned char c : str)
                {
                    hash_val ^= static_cast<uint64_t>(c);
                    hash_val *= 0x100000001b3ULL;
                }
                return hash_val;
            }
        };

        struct Djb2Hash
        {
            using hash_t = uint32_t;

            static hash_t hash(const std::string& str)
            {
                hash_t hash = 5381; // Initial magic constant
                // Iterate over bytes as unsigned char
                for (unsigned char c : str)
                {
                    // hash = hash * 33 + c
                    hash = ((hash << 5) + hash) + static_cast<hash_t>(c);
                }
                return hash;
            }
        };
    } // namespace hash

    // simple array-related utils
    namespace array
    {
        template <typename T_>
        void insert_at(T_* array, const size_t& array_size, const size_t& pos,
                       const T_& new_val)
        {
            memmove(array + pos + 1, array + pos, sizeof(T_) * (array_size - pos));
            array[pos] = new_val;
        }

        template <typename T_>
        void remove_at(T_* array, const size_t& array_size, const size_t& pos)
        {
            array[pos].~T_();
            memmove(array + pos, array + pos + 1,
                    sizeof(T_) * (array_size - pos - 1));
            if constexpr (!std::is_trivially_destructible_v<T_>)
                memset(array + (array_size - 1), 0, sizeof(T_));
        }

        template <typename T_>
        void migrate(T_* dest, T_* src, const size_t& migrate_count)
        {
            memcpy(dest, src, sizeof(T_) * migrate_count);
            if constexpr (!std::is_trivially_destructible_v<T_>)
                memset(src, 0, sizeof(T_) * migrate_count);
        }

        template <typename T_>
        bool equals(const norb::vector<T_>& a, const norb::vector<T_>& b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (a[i] != b[i])
                    return false;
            }
            return true;
        }
    } // namespace array

    namespace string
    {
        inline std::string trim(std::string s)
        {
            size_t start = 0;
            while (start < s.length() && std::isspace(static_cast<unsigned char>(s[start]))) {
                start++;
            }
            // Erase leading whitespace
            if (start > 0) {
                s.erase(0, start);
            }

            // Trim trailing whitespace ---
            if (!s.empty()) {
                size_t end = s.length() - 1;
                while (end > 0 && std::isspace(static_cast<unsigned char>(s[end]))) {
                    end--;
                }

                if (std::isspace(static_cast<unsigned char>(s[end]))) { // If the character at 'end' is still a space (e.g. string was all spaces)
                    if (end == 0) {
                        s.clear();
                    }
                } else {
                    s.erase(end + 1);
                }
            }
            return s;
        }
    }

    // unstructured stuff
    namespace chore
    {
        // Returns whether the three given variables are in ascending order,
        // non-strictly.
        template <typename T_>
        bool ascend(T_ begin, T_ val, T_ end)
        {
            return begin <= val && val <= end;
        }

        inline bool ascend(const char* begin, const char* val, const char* end)
        {
            return strcmp(begin, val) <= 0 && strcmp(val, end) <= 0;
        }

        inline void remove_associated()
        {
            std::remove(settings::PMEM_FILE_NAME.c_str());
            std::remove(settings::NPMEM_FILE_NAME.c_str());
            std::remove("persistent.config");
        }
    }; // namespace chore
} // namespace norb
