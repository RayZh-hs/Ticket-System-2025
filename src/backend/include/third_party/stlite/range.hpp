#pragma once

namespace norb
{
    // A simple class denoting [from, to]
    template <typename T>
    class Range
    {
        T from, to;

        Range(const T& from, const T& to)
            : from(from), to(to)
        {
        }

        bool contains(const T& query) const
        {
            return from <= query && query <= to;
        }
    };
}
