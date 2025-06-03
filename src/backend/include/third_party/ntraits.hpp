#pragma once

#include <concepts>
#include <iomanip>
#include <optional>
#include <ostream>
#include <type_traits>
#include <variant>

namespace norb {
    template <typename T>
    concept Addable = requires(T a, T b) {
        {
            a + b
        } -> std::convertible_to<T>; // The result of a+b should be convertible
                                     // to T
    };

    template <typename T>
    concept Subtractable = requires(T a, T b) {
        { a - b } -> std::convertible_to<T>;
    };

    template <typename T>
    concept Negatable = requires(T a) {
        { -a } -> std::convertible_to<T>;
    };

    template <typename T>
    concept Ostreamable = requires(T a, std::ostream &os) {
        { os << a } -> std::same_as<std::ostream &>;
    };

    template <typename T>
    concept OstreamableWithPrecision = requires(T a, std::ostream &os, int precision) {
        { os << std::setprecision(precision) << a } -> std::same_as<std::ostream &>;
    };

    template <typename T>
    concept Istreamable = requires(T a, std::istream &is) {
        { is >> a } -> std::same_as<std::istream &>;
    };

    template <typename G>
    concept IsGraph = requires(G g, const G cg, typename G::node_t u_node, typename G::node_t v_node, size_t n_size) {
        typename G::node_t;
        requires std::is_integral_v<typename G::node_t>;

        requires std::is_constructible_v<G, size_t>;

        { cg.get_edge(u_node, v_node) } -> std::semiregular;
        { cg.exists_edge(u_node, v_node) } -> std::same_as<bool>;

        requires requires { typename std::decay_t<decltype(cg.get_edge(u_node, v_node).value())>; } &&
                     requires(const std::decay_t<decltype(cg.get_edge(u_node, v_node).value())> &edge_payload) {
                         g.add_edge(u_node, v_node, edge_payload);
                     };

        { g.remove_edge(u_node, v_node) } -> std::same_as<bool>;

        { cg.size() } -> std::same_as<size_t>;

        { cg.get_edges_from(u_node) } -> std::ranges::range; // Check it returns a range
        requires requires(const decltype(cg.get_edges_from(u_node)) &edges_range) {
            // Check properties of the elements in the range
            // Requires that the range is not empty for this check, or use
            // declval on range_value_t Using std::ranges::range_value_t is
            // safer as it doesn't require iteration.
            typename std::ranges::range_value_t<decltype(edges_range)>;
            requires requires(const std::ranges::range_value_t<decltype(edges_range)> &edge_item) {
                { edge_item.from } -> std::same_as<typename G::node_t>;
                { edge_item.to } -> std::same_as<typename G::node_t>;
                {
                    edge_item.value
                } -> std::convertible_to<
                    std::decay_t<decltype(cg.get_edge(u_node, v_node).value())>>; // Use convertible_to
                                                                                  // for flexibility or
                                                                                  // std::same_as<DeducedEdgeType>
                                                                                  // for strictness
            };
        };

        { cg.get_all_edges() } -> std::ranges::range; // Check it returns a range
        requires requires(const decltype(cg.get_all_edges()) &all_edges_range) {
            typename std::ranges::range_value_t<decltype(all_edges_range)>;
            requires requires(const std::ranges::range_value_t<decltype(all_edges_range)> &edge_item) {
                { edge_item.from } -> std::same_as<typename G::node_t>;
                { edge_item.to } -> std::same_as<typename G::node_t>;
                { edge_item.value } -> std::convertible_to<std::decay_t<decltype(cg.get_edge(u_node, v_node).value())>>;
            };
        };
    };

    template <typename T> struct is_variant : std::false_type {};

    template <typename... Types> struct is_variant<std::variant<Types...>> : std::true_type {};

    template <typename T> struct is_optional : std::false_type {};

    template <typename... Types> struct is_optional<std::optional<Types...>> : std::true_type {};

    template <typename T> constexpr bool is_optional_v = is_optional<std::decay_t<T>>::value;

    template <typename T>
    concept IsComparable = requires(const T &a, const T &b) {
        { a < b } -> std::convertible_to<bool>;
        { a > b } -> std::convertible_to<bool>;
        { a <= b } -> std::convertible_to<bool>;
        { a >= b } -> std::convertible_to<bool>;
        { a == b } -> std::convertible_to<bool>;
        { a != b } -> std::convertible_to<bool>;
    } or std::three_way_comparable_with<T, T>;

    template <typename T>
    concept HasNeq = requires(const T &a, const T &b) {
        { a != b } -> std::convertible_to<bool>;
    };
} // namespace norb
