#pragma once

#include "semantic_cast.hpp"
#include "stlite/vector.hpp"
#include <string>

namespace ticket {

    using norb::semantic_cast;

    inline constexpr const char default_placeholder[] = "_";
    template <typename val_t, const char *placeholder = default_placeholder, const char delimiter = '|'>
    struct ConcentratedString {
      private:
        norb::vector<val_t> decoded;

      public:
        ConcentratedString(const std::string &s) {
            if (s == placeholder)
                return;
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

    // a half-ordered tuple where only the first element is ordered
    template <typename val_t, typename... Args> struct TrailingTuple {
      private:
        val_t primary;
        std::tuple<Args...> trailing;

      public:
        // Default constructor (only enabled if all types are default-constructible)
        TrailingTuple()
            requires std::is_default_constructible_v<val_t> && (std::is_default_constructible_v<Args> && ...)
            : primary(), trailing() {
        }

        TrailingTuple(const val_t &primary, const Args &...args) : primary(primary), trailing(args...) {
        }

        template <size_t I> auto get() const {
            if constexpr (I == 0) {
                return primary;
            } else {
                return std::get<I - 1>(trailing);
            }
        }

        auto operator<=>(const TrailingTuple &other) const {
            return primary <=> other.primary;
        }

        bool operator==(const TrailingTuple &other) const {
            return primary == other.primary;
        }

        bool operator!=(const TrailingTuple &other) const {
            return !(*this == other);
        }

        using id_t = val_t;
        id_t id() const {
            return primary;
        }
    };
} // namespace ticket