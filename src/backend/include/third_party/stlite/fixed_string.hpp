#pragma once

#include <cstring>
#include <iostream>

namespace norb
{
    template <size_t max_len>
    class FixedString
    {
    private:
        char str[max_len + 1]{};

    public:
        FixedString() = default;
        FixedString(const FixedString&) = default;

        FixedString(const std::string& std_string)
        {
            strcpy(str, std_string.c_str());
            str[max_len] = '\0';
        }

        explicit FixedString(const char* std_string)
        {
            strcpy(str, std_string);
            str[max_len] = '\0';
        }

        explicit operator std::string() const { return std::string(str); }

        bool operator!=(const FixedString& other) const
        {
            return strcmp(str, other.str) != 0;
        }

        bool operator == (const FixedString& other) const {
            return strcmp(str, other.str) == 0;
        }

        auto operator<=>(const FixedString& other) const
        {
            return strcmp(str, other.str) <=> 0;
        }
    };

    template <size_t max_len>
    std::istream& operator >>(std::istream& is, FixedString<max_len>& fixed_string)
    {
        std::string s;
        is >> s;
        if (is)
            fixed_string = s;
        return is;
    }

    template <size_t max_len>
    std::ostream& operator <<(std::ostream& os, const FixedString<max_len>& fixed_string)
    {
        const auto s = std::string(fixed_string);
        os << s;
        return os;
    }

    // The utf-8 utility is implemented via std::string, so this is only a alias
    template <size_t max_len>
    using FixedUTF8String = FixedString<max_len>;
} // namespace norb
