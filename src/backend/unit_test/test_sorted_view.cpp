#include "sorted_view.hpp"
#include "stlite/vector.hpp"
#include <iostream>

int main() {
    norb::vector<int> v;
    v.push_back(1);
    v.push_back(5);
    v.push_back(3);
    v.push_back(2);
    v.push_back(4);

    for (auto i : norb::make_sorted(v.size(), [&v](const int &i, const int &j) { return v[i] < v[j]; })) {
        std::cout << v[i] << std::endl;
    }
}