#pragma once

#include <iomanip>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace norb {
    // Primary template: attempts to construct T from std::string
    // This will be used if no explicit specialization matches.
    template <typename to_t, typename from_t> to_t semantic_cast(const from_t &s) {
        try {
            return to_t(s);
        } catch (const std::exception &e) {
            throw std::runtime_error("norb::semantic_cast<" + std::string(typeid(to_t).name()) + "> failed for '" + s +
                                     "' using generic T(s) constructor: " + e.what());
        }
    }

    template <typename T>
        requires std::is_integral_v<T>
    inline T semantic_cast(const std::string &s) {
        if (s.empty()) {
            return 0; // Empty string means 0
        }
        try {
            return std::stoi(s);
        } catch (const std::exception &e) {
            throw std::runtime_error("norb::semantic_cast<int> failed for '" + s + "': " + e.what());
        }
    }

    template <typename T>
        requires std::is_integral_v<T>
    inline std::string semantic_cast(const T &s) {
        return std::to_string(s);
    }

    template <> inline bool semantic_cast<bool>(const std::string &s) {
        if (s.empty()) {
            return false; // Empty string means false
        }
        if (s == "true" || s == "True" || s == "TRUE" || s == "1") {
            return true;
        }
        if (s == "false" || s == "False" || s == "FALSE" || s == "0") {
            return false;
        }
        throw std::runtime_error("norb::semantic_cast<bool> failed for '" + s +
                                 "'. Expected true/false/1/0 or empty for false.");
    }

    // Specialization for std::string is crucial because T(const std::string&) for T=std::string is just the copy
    // constructor. The primary template would work, but an explicit one is clearer.
    template <> inline std::string semantic_cast<std::string>(const std::string &s) {
        return s;
    }

    // Add other specializations as needed (e.g., double, long, custom classes not constructible from string)
} // namespace norb
