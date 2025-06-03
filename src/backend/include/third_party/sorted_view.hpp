#pragma once

#include "stlite/vector.hpp"

#include <functional>

namespace norb {
    namespace impl {
        inline void merge_sort(norb::vector<int> &v, int from, int to,
                        const std::function<bool(const int &, const int &)> &cmp) {
            if (from >= to)
                return;

            int mid = from + (to - from) / 2;
            merge_sort(v, from, mid, cmp);
            merge_sort(v, mid + 1, to, cmp);

            // Temporary vector for merging
            norb::vector<int> temp;
            int i = from, j = mid + 1;

            while (i <= mid && j <= to) {
                if (cmp(v[i], v[j])) {
                    temp.push_back(v[i++]);
                } else {
                    temp.push_back(v[j++]);
                }
            }
            while (i <= mid)
                temp.push_back(v[i++]);
            while (j <= to)
                temp.push_back(v[j++]);

            // Copy back to original vector
            for (int k = 0; k < temp.size(); ++k) {
                v[from + k] = temp[k];
            }
        }
    } // namespace impl

    inline auto make_sorted(const int &n, const std::function<bool(const int &, const int &)> &cmp) {
        norb::vector<int> ret;
        for (int i = 0; i < n; ++i)
            ret.push_back(i);
        impl::merge_sort(ret, 0, ret.size() - 1, cmp);
        return ret;
    }
} // namespace norb