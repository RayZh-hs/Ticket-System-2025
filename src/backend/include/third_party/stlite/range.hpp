#pragma once

#include <type_traits>
#include <limits>

#include "pair.hpp"

namespace norb {

    namespace impl
    {
        template <typename T>
        concept HasMinMax = requires(T a) {
            { a.min() } -> std::convertible_to<T>;
            { a.max() } -> std::convertible_to<T>;
        };
    } // namespace impl
    

    // A simple class denoting a range with configurable inclusiveness.
    // It represents mathematical intervals like [from, to], (from, to), [from, to), or (from, to].
    template <typename T> class Range {
      public:
        enum class Inclusiveness { NONE, LEFT, RIGHT, BOTH };

      private:
        T from_;
        T to_;
        bool left_inclusive_ = true;
        bool right_inclusive_ = true;

      public:
        Range() requires std::is_default_constructible_v<T> : from_(), to_() {
            // Default constructor initializes an empty range.
        }
        // Defaults to [L, R]
        Range(const T &from_val, const T &to_val) : from_(from_val), to_(to_val) {
        }

        // Constructor with specified inclusiveness type.
        Range(const T &from_val, const T &to_val, Inclusiveness type) : from_(from_val), to_(to_val) {
            switch (type) {
            case Inclusiveness::NONE:
                left_inclusive_ = false;
                right_inclusive_ = false;
                break;
            case Inclusiveness::LEFT:
                left_inclusive_ = true;
                right_inclusive_ = false;
                break;
            case Inclusiveness::RIGHT:
                left_inclusive_ = false;
                right_inclusive_ = true;
                break;
            case Inclusiveness::BOTH:
                left_inclusive_ = true;
                right_inclusive_ = true;
                break;
            }
        }

        // Checks if the given query value is contained within the range.
        bool contains(const T &query) const {
            bool check_left = left_inclusive_ ? (from_ <= query) : (from_ < query);
            if (!check_left) {
                return false;
            }
            bool check_right = right_inclusive_ ? (query <= to_) : (query < to_);
            return check_right;
        }

        bool contains_from_right(const T &query) const {
            return right_inclusive_ ? (query <= to_) : (query < to_);
        }

        bool contains_from_left(const T &query) const {
            return left_inclusive_ ? (from_ <= query) : (from_ < query);
        }

        // Returns the type of inclusiveness of the range.
        Inclusiveness get_inclusiveness_type() const {
            if (left_inclusive_) {
                return right_inclusive_ ? Inclusiveness::BOTH : Inclusiveness::LEFT;
            } else {
                return right_inclusive_ ? Inclusiveness::RIGHT : Inclusiveness::NONE;
            }
        }

        const T &get_from() const {
            return from_;
        }

        const T &get_to() const {
            return to_;
        }

        bool is_left_inclusive() const {
            return left_inclusive_;
        }

        bool is_right_inclusive() const {
            return right_inclusive_;
        }

        // Checks if the range is considered empty.
        bool is_empty() const {
            if (from_ > to_) {
                return true;
            }
            if (from_ == to_) {
                return !(left_inclusive_ && right_inclusive_);
            }
            return false;
        }

        static Range<T> full_range() {
            if constexpr (impl::HasMinMax<T>) {
                return Range<T>(T::min(), T::max());
            } else if constexpr (std::is_arithmetic_v<T>) {
                return Range<T>(std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max());
            } else {
                static_assert(false, "Type T must have min() and max() methods or be an arithmetic type.");
            }
        }
    };

    // Unpacks (a, range) to Range((a, range.from()), (a, range.to())).
    // a is not a range
    template <typename A, typename T> requires (!std::is_same_v<A, Range<T>>)
    auto unpack_range(const A &a, const Range<T> &range) {
        return Range(norb::make_pair(a, range.get_from()), norb::make_pair(a, range.get_to()));
    }

    // Unpacks (range, b) to Range((range.from(), b), (range.to(), b)).
    // b is not a range
    template <typename T, typename B>
    auto unpack_range(const Range<T> &range, const B &b) {
        return Range(norb::make_pair(range.get_from(), b), norb::make_pair(range.get_to(), b));
    }
} // namespace norb