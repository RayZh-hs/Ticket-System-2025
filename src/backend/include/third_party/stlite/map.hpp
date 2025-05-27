#pragma once

// only for std::less<T>
#include <cassert>
#include <cstddef>
#include <functional>
#include <utility>

#include <stdexcept>

// Debug Flags

#ifdef USE_DEBUG_FLAGS
#define DEBUG_VERBOSE  // provides verbose information when manually called.
                       // is known to fail compiling for certain classes
#define DEBUG_EXTRA_VERBOSE  // information that changes the output, should
                             // be closed in the auto-testing phase and
                             // submission. is known to fail compiling for
                             // certain classes. needs DEBUG_VERBOSE to work
#define DEBUG_VIS_UTILS      // provides auxiliary visualisation aids
#define DEBUG_CHECK_UTILS    // provides safeguard checks for the tree. is
                             // O(n) per call

// #define DEBUG_TRAIT_HELPER      // this should not be let on in the testing
// phase
#endif
#define DEBUG_ASSERTIONS  // provides assertions that can be removed to speed up
                          // the structure

#ifndef DEBUG_VERBOSE
#ifdef DEBUG_EXTRA_VERBOSE
// fail compiling here and warn
#error "ERROR: DEBUG_EXTRA_VERBOSE requires DEBUG_VERBOSE to also be defined."
#endif
#endif

#ifdef DEBUG_VIS_UTILS
#include <string>
#endif
#ifdef DEBUG_VERBOSE
#include <iostream>
#endif

namespace norb {
#ifdef DEBUG_TRAIT_HELPER
typedef int Key;
typedef int T;
typedef std::less<int> Compare;
#else
template <class Key, class T, class Compare = std::less<Key> >
#endif
class map {
   public:
    /**
     * the internal type of data.
     * it should have a default constructor, a copy constructor.
     * You can use sjtu::map as value_type by typedef.
     */
    typedef std::pair<const Key, T> value_type;

   private:
    struct AVLNode {
        AVLNode *parent = nullptr, *left = nullptr, *right = nullptr;
        int height = 0;
        value_type key_val_pair;

        AVLNode(Key key, T value, AVLNode *parent = nullptr,
                AVLNode *left = nullptr, AVLNode *right = nullptr,
                const int height = 1)
            : parent(parent),
              left(left),
              right(right),
              height(height),
              key_val_pair(key, value) {
        }

        explicit AVLNode(const value_type key_value_pair,
                         AVLNode *parent = nullptr, AVLNode *left = nullptr,
                         AVLNode *right = nullptr, const int height = 1)
            : parent(parent),
              left(left),
              right(right),
              height(height),
              key_val_pair(key_value_pair) {
        }

        ~AVLNode() {
            delete left;  // one can delete a nullptr it turns out
            delete right;
        }

        [[nodiscard]] int left_height() const {
            return left == nullptr ? 0 : left->height;
        }

        [[nodiscard]] int right_height() const {
            return right == nullptr ? 0 : right->height;
        }

        // update the height for this node only (assuming that the two
        // subtrees are correct)
        void update_height() {
            height = std::max(left_height(), right_height()) + 1;
        }

        // update height in a chain, all the way to the top of the tree (also
        // assumes the correctness of subtrees)
        void update_height_chain() {
            const int cur_height = height;
            update_height();
            if (height != cur_height && parent != nullptr) {
                parent->update_height_chain();
            }
        }

        AVLNode *rightmost() {
            auto ans = this;
            while (ans->right != nullptr) ans = ans->right;
            return ans;
        }

        AVLNode *leftmost() {
            auto *ans = this;
            while (ans->left != nullptr) ans = ans->left;
            return ans;
        }

        // duplicate all the nodes under and including self, recursively
        // returns a pointer to the copied self
        AVLNode *deep_copy(AVLNode *parent_ptr = nullptr) {
            auto *self_copy = new AVLNode(*this);
            self_copy->parent = parent_ptr;
            if (this->left != nullptr) {
                self_copy->left = this->left->deep_copy(self_copy);
            }
            if (this->right != nullptr) {
                self_copy->right = this->right->deep_copy(self_copy);
            }
            return self_copy;
        }

        // delete all the nodes under self, recursively
        // will keep self
        void deep_delete() {
            if (this->left != nullptr) {
                this->left->deep_delete();
                delete this->left;
                this->left = nullptr;
            }
            if (this->right != nullptr) {
                this->right->deep_delete();
                delete this->right;
                this->right = nullptr;
            }
        }

#ifdef DEBUG_VIS_UTILS
        [[nodiscard]] __attribute__((noinline)) std::string repr() const {
            std::string ans = std::to_string(key_val_pair.first);
            ans += "(";
            ans += (left == nullptr ? "" : left->repr());
            ans += ", ";
            ans += (right == nullptr ? "" : right->repr());
            ans += ")";
            return ans;
        }
#endif

#ifdef DEBUG_CHECK_UTILS
        void check_parent() {
#ifdef DEBUG_VERBOSE
            std::cerr << this->key_val_pair.first << ' ';
            // std::cerr.flush();
#endif
            if (left != nullptr) assert(this->left->parent == this);
            if (right != nullptr) assert(this->right->parent == this);
            if (left != nullptr) left->check_parent();
            if (right != nullptr) right->check_parent();
        }

        void check_height() {
#ifdef DEBUG_VERBOSE
            std::cerr << this->key_val_pair.first << ' ';
            // std::cerr.flush();
#endif
            assert(this->height ==
                   1 + std::max(this->left_height(), this->right_height()));
            if (left != nullptr) this->left->check_height();
            if (right != nullptr) this->right->check_height();
        }

        void check_balance() {
#ifdef DEBUG_VERBOSE
            std::cerr << this->key_val_pair.first << ' ';
            // std::cerr.flush();
#endif
            assert(std::abs(this->left_height() - this->right_height()) <= 1);
            if (left != nullptr) this->left->check_balance();
            if (right != nullptr) this->right->check_balance();
        }
#endif
    };

    AVLNode *root = nullptr;
    size_t size_ = 0;

    AVLNode *&parent_pointer_to(AVLNode *node) {
        if (node->parent == nullptr) {
            return root;
        }
        if (node->parent->left == node) {
            return node->parent->left;
        }
#ifdef DEBUG_ASSERTIONS
        assert(node->parent->right == node);
#endif
        return node->parent->right;
    }

    AVLNode *rotate_left(AVLNode *node) {
        AVLNode *node_r = node->right;
        // shift responsibility and remap hierarchy
        node->right = node_r->left;
        node_r->left = node;
        // change parent node
        if (node->right != nullptr) {
            node->right->parent = node;
        }
        auto &node_p = parent_pointer_to(node);
        node_r->parent = node->parent;
        node->parent = node_r;
        node_p = node_r;
        // update heights
        node->update_height();
        node_r->update_height_chain();
        return node_r;
    }

    AVLNode *rotate_right(AVLNode *node) {
        AVLNode *node_l = node->left;
        // shift responsibility and remap hierarchy
        node->left = node_l->right;
        node_l->right = node;
        // change parent node
        if (node->left != nullptr) {
            node->left->parent = node;
        }
        auto &node_p = parent_pointer_to(node);
        node_l->parent = node->parent;
        node->parent = node_l;
        node_p = node_l;
        // update heights
        node->update_height();
        node_l->update_height_chain();
        return node_l;
    }

    void swap_node_with_empty(AVLNode *node, AVLNode *empty_node) {
        // set everything around node
        parent_pointer_to(node) = empty_node;
        if (node->left != nullptr) node->left->parent = empty_node;
        if (node->right != nullptr) node->right->parent = empty_node;
        // copy node's info into empty_node
        empty_node->parent = node->parent;
        empty_node->left = node->left;
        empty_node->right = node->right;
        // clear node
        node->parent = node->left = node->right = nullptr;
        empty_node->height = node->height;
    }

    void swap_nodes(AVLNode *node_a, AVLNode *node_b) {
        auto *node_c = new AVLNode(node_a->key_val_pair);
#ifdef DEBUG_ASSERTIONS
        const int node_a_height = node_a->height;
        const int node_b_height = node_b->height;
#endif
        swap_node_with_empty(node_a, node_c);
        swap_node_with_empty(node_b, node_a);
        swap_node_with_empty(node_c, node_b);
#ifdef DEBUG_ASSERTIONS
        assert(node_a_height == node_b->height);
        assert(node_b_height == node_a->height);
#endif
        delete node_c;
    }

   public:
    /**
     * see BidirectionalIterator at CppReference for help.
     *
     * if there is anything wrong throw invalid_iterator.
     *     like it = map.begin(); --it;
     *       or it = map.end(); ++end();
     */
    class const_iterator;

    class SharedIteratorMixin {
        friend class map;

       protected:
        map *origin = nullptr;
        AVLNode *node = nullptr;

        SharedIteratorMixin() = default;

        SharedIteratorMixin(map *origin, AVLNode *node)
            : origin(origin), node(node) {
        }

        [[nodiscard]] bool at_end() const {
            return node == nullptr;
        }

        void increment_self() {
            if (origin == nullptr) throw std::runtime_error("Invalid Iterator");
            if (origin->root == nullptr) throw std::runtime_error("Invalid Iterator");
            if (at_end()) throw std::runtime_error("Invalid Iterator");
            if (node->right != nullptr) {
                node = node->right->leftmost();
            } else {
                while (node->parent != nullptr) {
                    if (node->parent->left == node) {
                        node = node->parent;
                        return;
                    }
                    node = node->parent;
                }
                // have reached the top, go on to end()
                node = nullptr;
            }
        }

        void decrement_self() {
            if (origin == nullptr) throw std::runtime_error("Invalid Iterator");
            if (origin->root == nullptr) throw std::runtime_error("Invalid Iterator");
            if (at_end()) {
                // Get the rightmost child of root.
                node = origin->root->rightmost();
            } else {
                if (node->left != nullptr) {
                    node = node->left->rightmost();
                } else {
                    while (node->parent != nullptr) {
                        if (node->parent->right == node) {
                            node = node->parent;
                            return;
                        }
                        node = node->parent;
                    }
                    // have reached the top
                    // this means that the given iterator points to the
                    // beginning of the map
                    throw std::runtime_error("Invalid Iterator");
                }
            }
        }
    };

    class iterator : public SharedIteratorMixin {
        friend class const_iterator;

       public:
        iterator() = default;

        iterator(map *origin, AVLNode *node)
            : SharedIteratorMixin(origin, node) {
        }

        iterator(const iterator &other) = default;

        // iter++
        iterator operator++(int) {
            const auto this_copy = iterator(*this);
            this->increment_self();
            return this_copy;
        }

        // ++iter
        iterator &operator++() {
            this->increment_self();
            return *this;
        }

        // iter--
        iterator operator--(int) {
            const auto this_copy = iterator(*this);
            this->decrement_self();
            return this_copy;
        }

        // --iter
        iterator &operator--() {
            this->decrement_self();
            return *this;
        }

        value_type &operator*() const {
            return this->node->key_val_pair;
        }

        /**
         * an operator to check whether two iterators are same (pointing to the
         * same memory).
         */
        bool operator==(const iterator &rhs) const {
            return this->node == rhs.node && this->origin == rhs.origin;
        }

        bool operator==(const const_iterator &rhs) const {
            return this->node == rhs.node && this->origin == rhs.origin;
        }

        /**
         * some other operator for iterator.
         */
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }

        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }

        /**
         * for the support of it->first.
         * See
         * <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/>
         * for help.
         */
        value_type *operator->() const noexcept {
            return &(this->node->key_val_pair);
        }

        [[nodiscard]] const_iterator as_const() const {
            return {this->origin, this->node};
        }
    };

    class const_iterator : public SharedIteratorMixin {
        // it should have similar member method as iterator.
        //  and it should be able to construct from an iterator.
       private:
        // data members.

        friend class iterator;

       public:
        const_iterator() = default;

        const_iterator(const map *origin, AVLNode *node)
            : SharedIteratorMixin(const_cast<map *>(origin), node) {
        }

        const_iterator(const const_iterator &other) = default;

        explicit const_iterator(const iterator &other)
            : SharedIteratorMixin(other.origin, other.node) {
        }

        const_iterator &operator=(const iterator &other) {
            this->origin = other.origin;
            this->node = other.node;
            return *this;
        }

        // And other methods in iterator.

        // iter++
        const_iterator operator++(int) {
            const auto this_copy = const_iterator(*this);
            this->increment_self();
            return this_copy;
        }

        // ++iter
        const_iterator &operator++() {
            this->increment_self();
            return *this;
        }

        // iter--
        const_iterator operator--(int) {
            const auto this_copy = const_iterator(*this);
            this->decrement_self();
            return this_copy;
        }

        // --iter
        const_iterator &operator--() {
            this->decrement_self();
            return *this;
        }

        const value_type &operator*() const {
            return this->node->key_val_pair;
        }

        /**
         * an operator to check whether two iterators are same (pointing to the
         * same memory).
         */
        bool operator==(const iterator &rhs) const {
            return this->node == rhs.node && this->origin == rhs.origin;
        }

        bool operator==(const const_iterator &rhs) const {
            return this->node == rhs.node && this->origin == rhs.origin;
        }

        /**
         * some other operator for iterator.
         */
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }

        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }

        /**
         * for the support of it->first.
         * See
         * <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/>
         * for help.
         */
        const value_type *operator->() const noexcept {
            return &(this->node->key_val_pair);
        }
    };

    map() = default;

    map(const map &other) {
        clear();
        if (other.root != nullptr) root = other.root->deep_copy();
        size_ = other.size();
    }

    map &operator=(const map &other) {
        if (this != &other) {
            clear();
            if (other.root != nullptr) root = other.root->deep_copy();
            size_ = other.size();
        }
        return *this;
    }

    ~map() {
        clear();
    }

    /**
     * access specified element with bounds checking
     * Returns a reference to the mapped value of the element with key
     * equivalent to key. If no such element exists, an exception of type
     * `index_out_of_bound`
     */
    T &at(const Key &key) {
        const iterator iter = find(key);
        if (iter == end()) {
            throw std::out_of_range("Index out of bound");
        }
        return iter->second;
    }

    const T &at(const Key &key) const {
        const const_iterator iter = find(key);
        if (iter == cend()) {
            throw std::out_of_range("Index out of bound");
        }
        return iter->second;
    }

    /**
     * access specified element
     * Returns a reference to the value that is mapped to a key equivalent to
     * key, performing an insertion if such key does not already exist.
     */
    T &operator[](const Key &key)
        requires std::is_default_constructible_v<T>
    {
        const auto ans = insert({key, T()});
        const auto node = ans.first.node;
        return node->key_val_pair.second;
    }

    /**
     * behave like at() throw index_out_of_bound if such key does not exist.
     */
    const T &operator[](const Key &key) const {
        return at(key);
    }

    /**
     * return an iterator to the beginning
     */
    iterator begin() {
        if (empty()) {
            // quote https://en.cppreference.com/w/cpp/container/map/begin:
            // If the map is empty, the returned iterator will be equal to
            // end().
            return {this, nullptr};
        }
        AVLNode *leftmost_node = root->leftmost();
        return {this, leftmost_node};
    }

    const_iterator cbegin() const {
        if (empty()) {
            // quote https://en.cppreference.com/w/cpp/container/map/begin:
            // If the map is empty, the returned iterator will be equal to
            // end().
            return {this, nullptr};
        }
        AVLNode *leftmost_node = root->leftmost();
        return {this, leftmost_node};
    }

    /**
     * return an iterator to the end
     * in fact, it returns past-the-end.
     */
    iterator end() const {
        return {this, nullptr};
    }

    const_iterator cend() const {
        return {this, nullptr};
    }

    /**
     * checks whether the container is empty
     * return true if empty, otherwise false.
     */
    bool empty() const {
        return size_ == 0;
    }

    /**
     * returns the number of elements.
     */
    size_t size() const {
        return size_;
    }

    /**
     * clears the contents
     */
    void clear() {
        if (root != nullptr) {
            root->deep_delete();
            delete root;
            root = nullptr;
            size_ = 0;
        }
    }

   private:
    // returns the pointer to the node that takes the place of the inputted node
    AVLNode *balance_node(AVLNode *node) {
#ifdef DEBUG_VERBOSE
        std::cerr << "(BN) Balancing Node: " << node->key_val_pair.first
                  << '\n';
        // std::cerr.flush();
#endif
        const int l_height = node->left_height();
        const int r_height = node->right_height();
        if (l_height >= r_height + 2) {
            const int node_ll_height = node->left->left_height();
            const int node_lr_height = node->left->right_height();
            if (node_ll_height >= node_lr_height) {
#ifdef DEBUG_VERBOSE
                std::cerr << "(BN) Mode = R\n";
                // std::cerr.flush();
#endif
                rotate_right(node);
            } else {
#ifdef DEBUG_VERBOSE
                std::cerr << "(BN) Mode = LR\n";
                // std::cerr.flush();
#endif
                rotate_left(node->left);
                rotate_right(node);
            }
        } else if (r_height >= l_height + 2) {
            const int node_rl_height = node->right->left_height();
            const int node_rr_height = node->right->right_height();
            if (node_rr_height >= node_rl_height) {
#ifdef DEBUG_VERBOSE
                std::cerr << "(BN) Mode = L\n";
                // std::cerr.flush();
#endif
                rotate_left(node);
            } else {
#ifdef DEBUG_VERBOSE
                std::cerr << "(BN) Mode = RL\n";
                // std::cerr.flush();
#endif
                rotate_right(node->right);
                rotate_left(node);
            }
        } else {
            throw std::runtime_error("Corrupted tree structure");
        }
        return node->parent;
    }

    // start from the node and traverse upwards, updating the height of each
    // node if a node's height is not changed, it exits if a node is found to be
    // unbalanced, it is corrected and the function also exits AVL asserts that
    // the change will restore the changed tree to its original height
    void balance_upstream(AVLNode *node) {
        while (node != nullptr) {
            const int ori_height = node->height;
            if (std::abs(node->left_height() - node->right_height()) >= 2) {
                balance_node(node);
                // assert(replacement_node->height == ori_height); // this is
                // guaranteed by the rotation rule, but only in insert mode
                // break;
            }
            node->update_height();
            // if (ori_height == node->height) break;
            node = node->parent;
        }
    }

   public:
    /**
     * insert an element.
     * return a pair, the first of the pair is
     *   the iterator to the new element (or the element that prevented the
     * insertion), the second one is true if insert successfully, or false.
     */
    std::pair<iterator, bool> insert(const value_type &value) {  // insert a new one
        if (root == nullptr) {
            root = new AVLNode(value);
            ++size_;
            return {iterator(this, root), true};
        }
        // go on and locate where the new node should be inserted
        AVLNode *node = root;
        const auto &key = value.first;
        while (true) {
            const auto &node_key = node->key_val_pair.first;
            if (Compare()(key, node_key)) {
                // insert into the left side
                if (node->left == nullptr) {
                    auto new_node = node->left = new AVLNode(value, node);
                    ++size_;
                    balance_upstream(node);
                    // node->left does not necessarily point to the original node anymore
                    // so we stored it beforehand and use it now
                    return {iterator(this, new_node), true};
                } else {
                    node = node->left;
                    continue;
                }
            }
            if (Compare()(node_key, key)) {
                // insert into the right side
                if (node->right == nullptr) {
                    auto new_node = node->right = new AVLNode(value, node);
                    ++size_;
                    balance_upstream(node);
                    return {iterator(this, new_node), true};
                } else {
                    node = node->right;
                    continue;
                }
            }
            // this means that a conflicting entry has been found
            return {iterator(this, node), false};
        }
    }

    /**
     * erase the element at pos.
     *
     * throw if pos pointed to a bad element (pos == this->end() || pos points
     * an element out of this)
     */
    void erase(iterator pos) {
        if (pos == end() || pos.origin != this) {
            throw std::runtime_error("Invalid Iterator");
        }
        if (pos.node->left != nullptr) {
            swap_nodes(pos.node, pos.node->left->rightmost());
        }

#ifdef DEBUG_ASSERTIONS
        assert(pos.node->left == nullptr || pos.node->right == nullptr);
#endif

        // first change all around the node, bypassing it
        // there are three cases in total
        AVLNode *pos_parent_node = pos.node->parent;
        if (pos.node->left != nullptr) {  // case 1
            pos.node->left->parent = pos_parent_node;
            parent_pointer_to(pos.node) = pos.node->left;
        }
        if (pos.node->right != nullptr) {  // case 2
            pos.node->right->parent = pos_parent_node;
            parent_pointer_to(pos.node) = pos.node->right;
        }
        if (pos.node->left == nullptr &&
            pos.node->right == nullptr) {  // case 3
            parent_pointer_to(pos.node) = nullptr;
        }
        // finally erase the node
        pos.node->right = nullptr;
        pos.node->left = nullptr;
        delete pos.node;
        --size_;
        // and balance the tree
        balance_upstream(pos_parent_node);
    }

    /**
     * Returns the number of elements with key
     *   that compares equivalent to the specified argument,
     *   which is either 1 or 0
     *     since this container does not allow duplicates.
     * The default method of check the equivalence is !(a < b || b > a)
     */
    [[nodiscard]] size_t count(const Key &key) const {
        if (find(key) == cend()) {
            return 0;
        }
        return 1;
    }
    /**
     * Finds an element with key equivalent to key.
     * key value of the element to search for.
     * Iterator to an element with key equivalent to key.
     *   If no such element is found, past-the-end (see end()) iterator is
     * returned.
     */
    [[nodiscard]] iterator find(const Key &key) {
        AVLNode *node = root;
        while (node != nullptr) {
            const auto &node_key = node->key_val_pair.first;
            if (Compare()(key, node_key)) {
                node = node->left;
                continue;
            }
            if (Compare()(node_key, key)) {
                node = node->right;
                continue;
            }
            return {this, node};
        }
        return end();
    }

    [[nodiscard]] const_iterator find(const Key &key) const {
        AVLNode *node = root;
        while (node != nullptr) {
            const auto &node_key = node->key_val_pair.first;
            if (Compare()(key, node_key)) {
                node = node->left;
                continue;
            }
            if (Compare()(node_key, key)) {
                node = node->right;
                continue;
            }
            return {this, node};
        }
        return cend();
    }

#ifdef DEBUG_VIS_UTILS
    std::string repr() const {
        if (root == nullptr) {
            return "AVL()";
        }
        return "AVL(" + (root->repr()) + ")";
    }
#endif

#ifdef DEBUG_CHECK_UTILS
    void check() {
#ifdef DEBUG_VERBOSE
        std::cerr << "(CK) Performing check\n";
#endif
        if (root != nullptr) {
#ifdef DEBUG_VERBOSE
            std::cerr << "(CKPar) ";
#endif
            root->check_parent();
#ifdef DEBUG_VERBOSE
            std::cerr << '\n';
            std::cerr << "(CKHei) ";
#endif
            root->check_height();
#ifdef DEBUG_VERBOSE
            std::cerr << '\n';
            std::cerr << "(CKBal) ";
#endif
            root->check_balance();
#ifdef DEBUG_VERBOSE
            std::cerr << '\n';
#endif
        }
    }
#endif
};

}
