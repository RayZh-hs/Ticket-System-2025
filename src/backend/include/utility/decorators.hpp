// This file implements python-style decorators
#pragma once

namespace norb {

    template <typename Callable> class PrintResult {
      private:
        std::decay_t<Callable> m_func;
        std::ostream &m_os;
        std::string ending;

      public:
        // Constructor: takes the callable to wrap and the ostream
        PrintResult(Callable &&func, const std::string &ending, std::ostream &os)
            : m_func(std::forward<Callable>(func)), ending(ending), m_os(os) {
        }

        template <typename... Args> void operator()(Args &&...args) const {
            using OriginalReturnType = decltype(m_func(std::forward<Args>(args)...));

            if constexpr (std::is_void_v<OriginalReturnType>) {
                throw std::logic_error("Decorated function returns void; throwing as per policy.");
            } else {
                m_os << m_func(std::forward<Args>(args)...) << ending;
            }
        }
    };

    // Using $deco as decorator naming convention. Would be nice to use @, but cpp forbids that

    // Factory for (f...): any -> (print f...): void
    template <typename Callable>
    auto $print(Callable &&func, const std::string &ending = "\n", std::ostream &os = std::cout) {
        return PrintResult<Callable>(std::forward<Callable>(func), ending, os);
    }
} // namespace norb