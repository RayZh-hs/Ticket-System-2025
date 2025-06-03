#pragma once

#include <compare>
#include <type_traits>
#include <utility>
#include "ntraits.hpp"

namespace norb {
  /**
   * @class Pair
   * @brief A class template similar to std::pair.
   * @tparam first_t_ Type of the first element.
   * @tparam second_t_ Type of the second element.
   */
  template <typename first_t_, typename second_t_> class Pair {
  public:
    first_t_ first;
    second_t_ second;
    Pair() = default;
    Pair(first_t_ first, second_t_ second) : first(first), second(second) {}

    Pair(const Pair &other) = default;
    Pair(Pair &&other) = default;
    Pair &operator=(const Pair &other) = default;

    auto operator<=>(const Pair &other) const requires IsComparable<second_t_> = default;
    bool operator==(const Pair &other) const requires IsComparable<second_t_> = default;

    auto operator<=>(const Pair &other) const requires (not IsComparable<second_t_>) {
      return first <=> other.first;
    }
    bool operator==(const Pair &other) const requires (not IsComparable<second_t_>) {
      return first == other.first;
    }
  };

  // ostream operator for Pair
  template <typename first_t_, typename second_t_>
  inline std::ostream &operator<<(std::ostream &os, const Pair<first_t_, second_t_> &pair) {
    os << '(' << pair.first << ", " << pair.second << ')';
    return os;
  }

  template <typename first_t_, typename second_t_>
  constexpr Pair<std::decay_t<first_t_>, std::decay_t<second_t_>>
  make_pair(first_t_ &&first, second_t_ &&second) {
    return Pair<std::decay_t<first_t_>, std::decay_t<second_t_>>(
        std::forward<first_t_>(first), std::forward<second_t_>(second));
  }
} // namespace norb