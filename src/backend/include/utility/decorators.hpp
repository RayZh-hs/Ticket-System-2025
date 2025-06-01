#pragma once

#include "ntraits.hpp"
#include "interface.hpp"

#include <ostream>

namespace norb {
    template <typename T> inline constexpr bool is_variant_v = is_variant<std::decay_t<T>>::value;

    template <typename Callable, typename OstreamLike> class PrintResult {
      private:
        std::decay_t<Callable> m_func;
        OstreamLike &m_os;
        std::string m_ending; // Renamed from 'ending' to avoid conflict with parameter in C++20

      public:
        // Constructor: takes the callable to wrap and the ostream
        PrintResult(Callable &&func, std::string ending_str, OstreamLike &os) // Renamed param
            : m_func(std::forward<Callable>(func)), m_os(os), m_ending(std::move(ending_str)) {
        }

        template <typename... Args> void operator()(Args &&...args) const {
            // Get the result of the function call first
            // Note: If m_func returns void, this line would be problematic.
            // So we handle void separately using decltype on the expression.

            using OriginalReturnType = decltype(m_func(std::forward<Args>(args)...));

            if constexpr (std::is_void_v<OriginalReturnType>) {
                // Original behavior: throws without calling m_func if its return type is void.
                // If you want to call m_func then throw:
                // m_func(std::forward<Args>(args)...);
                throw std::logic_error("Decorated function returns void; throwing as per policy.");
            } else {
                // For non-void types, call the function and store its result.
                auto result = m_func(std::forward<Args>(args)...);

                // Use std::decay_t on the type of 'result' for the is_variant_v check
                // as 'result' might be a reference type if m_func returns a reference.
                if constexpr (is_variant_v<decltype(result)>) {
                    std::visit(
                        [this](const auto &value) {
                            // You could add special handling for std::monostate if needed:
                            // if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::monostate>) {
                            //     m_os.as() << "[monostate]" << m_ending;
                            // } else {
                            m_os.as() << value << m_ending;
                            // }
                        },
                        result);
                } else {
                    // Original behavior for non-variant, non-void types
                    m_os.as() << result << m_ending;
                }
            }
        }
    };

    // Using $deco as decorator naming convention. Would be nice to use @, but cpp forbids that

    // Factory for (f...): any -> (print f...): void
    template <typename Callable, typename OstreamLike = ticket::CoutTimestampWrapper>
    auto $print(Callable &&func, const std::string &ending = "\n", OstreamLike &os = ticket::timestamped_cout) {
        // Pass 'ending' by value to PrintResult constructor to allow move
        return PrintResult<Callable, OstreamLike>(std::forward<Callable>(func), ending, os);
    }
} // namespace norb