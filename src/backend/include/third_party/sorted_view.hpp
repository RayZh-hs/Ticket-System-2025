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

        template <typename T>
        using cmp_func_t = std::function<bool(const T &, const T &)>;
    } // namespace impl

    inline auto make_sorted(const int &n, const impl::cmp_func_t<int> &cmp) {
        norb::vector<int> ret;
        for (int i = 0; i < n; ++i)
            ret.push_back(i);
        impl::merge_sort(ret, 0, ret.size() - 1, cmp);
        return ret;
    }

    inline auto make_supreme(const int &n, const impl::cmp_func_t<int> &cmp) {
        // aim: retrieve the argmax for cmp (equivalent to make_sorted[0])
        if (n == 0) {
            return -1;
        }
        int idx = 0;
        for (int i = 1; i < n; ++i) {
            if (cmp(idx, i)) {
                idx = i;
            }
        }
        return idx;
    }

    template <typename T>
    struct SupremeKeep {
        std::optional<T> val = std::nullopt;
        const impl::cmp_func_t<T> &cmp;

        SupremeKeep(const impl::cmp_func_t<T> &cmp_func) : cmp(cmp_func) {}

        void add(const T &v) {
            if (not val.has_value() or cmp(v, val.value())) {
                val = v;
            }
        }

        auto get() const -> std::optional<T> {
            return val;
        }
    };
} // namespace norb