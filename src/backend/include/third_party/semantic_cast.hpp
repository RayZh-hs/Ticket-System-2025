#pragma once

#include <string>
#include <stdexcept>
#include <iomanip>
#include <type_traits>

namespace norb
{
    // Primary template: attempts to construct T from std::string
    // This will be used if no explicit specialization matches.
    template <typename T>
    T semantic_cast(const std::string& s) {
        try {
            // Requires T to have a constructor T(const std::string&)
            // or a constructor T(const char*) if s.c_str() is used.
            // Or other suitable conversion.
            // For basic types, this won't work without specializations.
            return T(s);
        } catch (const std::exception& e) {
            throw std::runtime_error("norb::semantic_cast<" + std::string(typeid(T).name()) +
                                     "> failed for '" + s + "' using generic T(s) constructor: " + e.what());
        }
    }

    template <typename T> requires std::is_integral_v<T>
    inline T semantic_cast(const std::string& s) {
        if (s.empty()) {
            return 0; // Empty string means 0
        }
        try {
            return std::stoi(s);
        } catch (const std::exception& e) {
            throw std::runtime_error("norb::semantic_cast<int> failed for '" + s + "': " + e.what());
        }
    }

    template <>
    inline bool semantic_cast<bool>(const std::string& s) {
        if (s.empty()) {
            return false; // Empty string means false
        }
        if (s == "true" || s == "True" || s == "TRUE" || s == "1") {
            return true;
        }
        if (s == "false" || s == "False" || s == "FALSE" || s == "0") {
            return false;
        }
        throw std::runtime_error("norb::semantic_cast<bool> failed for '" + s + "'. Expected true/false/1/0 or empty for false.");
    }

    // Specialization for std::string is crucial because T(const std::string&) for T=std::string is just the copy constructor.
    // The primary template would work, but an explicit one is clearer.
    template <>
    inline std::string semantic_cast<std::string>(const std::string& s) {
        return s;
    }

    // Add other specializations as needed (e.g., double, long, custom classes not constructible from string)

} // namespace norb