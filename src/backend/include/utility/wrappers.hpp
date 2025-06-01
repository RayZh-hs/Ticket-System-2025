#pragma once

#include "semantic_cast.hpp"
#include "stlite/vector.hpp"
#include <string>

namespace ticket {

    using norb::semantic_cast;

    template <typename val_t, const char delimiter = '|'> struct ConcentratedString {
      private:
        norb::vector<val_t> decoded;

      public:
        ConcentratedString(const std::string &s) {
            std::string mem;
            for (int i = 0; i < s.length(); i++) {
                if (s[i] == delimiter) {
                    decoded.push_back(semantic_cast<val_t>(mem));
                    mem.clear();
                } else {
                    mem += s[i];
                }
            }
            decoded.push_back(semantic_cast<val_t>(mem));
        }

        auto as_vector() const {
            return decoded;
        }

        size_t size() const {
            return decoded.size();
        }
    };
} // namespace ticket