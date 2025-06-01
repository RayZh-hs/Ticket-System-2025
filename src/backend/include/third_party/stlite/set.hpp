#pragma once

// only for std::less<T>
#include <cassert>
#include <cstddef>
#include <functional>
// #include <utility> // std::pair is still used for insert's return type, but not for value_type

#include <stdexcept> // For std::out_of_range (though at() is removed), std::runtime_error

// Debug Flags

#ifdef USE_DEBUG_FLAGS
#define DEBUG_VERBOSE       // provides verbose information when manually called.
                            // is known to fail compiling for certain classes
#define DEBUG_EXTRA_VERBOSE // information that changes the output, should
                            // be closed in the auto-testing phase and
                            // submission. is known to fail compiling for
                            // certain classes. needs DEBUG_VERBOSE to work
#define DEBUG_VIS_UTILS     // provides auxiliary visualisation aids
#define DEBUG_CHECK_UTILS   // provides safeguard checks for the tree. is
                            // O(n) per call

// #define DEBUG_TRAIT_HELPER      // this should not be let on in the testing
// phase
#endif
#define DEBUG_ASSERTIONS // provides assertions that can be removed to speed up
                         // the structure

#ifndef DEBUG_VERBOSE
#ifdef DEBUG_EXTRA_VERBOSE
// fail compiling here and warn
#error "ERROR: DEBUG_EXTRA_VERBOSE requires DEBUG_VERBOSE to also be defined."
#endif
#endif

#ifdef DEBUG_VIS_UTILS
#include <string> // For std::to_string
#endif
#ifdef DEBUG_VERBOSE
#include <iostream> // For std::cerr
#endif

namespace norb {
#ifdef DEBUG_TRAIT_HELPER
    typedef int Key;
    // typedef int T; // T is removed for set
    typedef std::less<int> Compare;
#else
    template <class Key, class Compare = std::less<Key>> // T is removed
#endif
    class set { // Renamed from map to set
      public:
        /**
         * the internal type of data. For a set, this is just the Key itself.
         * It should have a default constructor, a copy constructor.
         */
        typedef Key value_type; // Changed from std::pair<const Key, T>

      private:
        struct AVLNode {
            AVLNode *parent = nullptr, *left = nullptr, *right = nullptr;
            int height = 0;
            Key key_data; // Changed from value_type key_val_pair; stores Key directly

            // Constructor for set, takes a Key
            explicit AVLNode(const Key &k, AVLNode *parent_ptr = nullptr, AVLNode *left_ptr = nullptr,
                             AVLNode *right_ptr = nullptr, const int h = 1)
                : parent(parent_ptr), left(left_ptr), right(right_ptr), height(h), key_data(k) {
            }

            // Copy constructor for AVLNode (default would also work for Key key_data)
            // Retaining explicit version from original map's second constructor style
            explicit AVLNode(const AVLNode &other) = default; // Or can be explicit:
            // AVLNode(const AVLNode& other) : parent(nullptr), left(nullptr), right(nullptr), height(other.height),
            // key_data(other.key_data) {} Note: deep_copy will handle parent/left/right pointers. This ctor is for `new
            // AVLNode(*this)` in deep_copy.

            ~AVLNode() {
                delete left;
                delete right;
            }

            [[nodiscard]] int left_height() const {
                return left == nullptr ? 0 : left->height;
            }

            [[nodiscard]] int right_height() const {
                return right == nullptr ? 0 : right->height;
            }

            void update_height() {
                height = std::max(left_height(), right_height()) + 1;
            }

            void update_height_chain() {
                const int cur_height = height;
                update_height();
                if (height != cur_height && parent != nullptr) {
                    parent->update_height_chain();
                }
            }

            AVLNode *rightmost() {
                auto ans = this;
                while (ans->right != nullptr)
                    ans = ans->right;
                return ans;
            }

            AVLNode *leftmost() {
                auto *ans = this;
                while (ans->left != nullptr)
                    ans = ans->left;
                return ans;
            }

            AVLNode *deep_copy(AVLNode *parent_ptr = nullptr) {
                auto *self_copy = new AVLNode(*this); // Uses AVLNode's copy constructor for key_data
                self_copy->parent = parent_ptr;
                if (this->left != nullptr) {
                    self_copy->left = this->left->deep_copy(self_copy);
                }
                if (this->right != nullptr) {
                    self_copy->right = this->right->deep_copy(self_copy);
                }
                return self_copy;
            }

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
                // Assuming Key is convertible via std::to_string or has a similar mechanism
                // This might need adjustment if Key is a complex type.
                std::string ans = std::to_string(key_data); // Changed from key_val_pair.first
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
                std::cerr << this->key_data << ' '; // Changed from key_val_pair.first
#endif
                if (left != nullptr)
                    assert(this->left->parent == this);
                if (right != nullptr)
                    assert(this->right->parent == this);
                if (left != nullptr)
                    left->check_parent();
                if (right != nullptr)
                    right->check_parent();
            }

            void check_height() {
#ifdef DEBUG_VERBOSE
                std::cerr << this->key_data << ' '; // Changed from key_val_pair.first
#endif
                assert(this->height == 1 + std::max(this->left_height(), this->right_height()));
                if (left != nullptr)
                    this->left->check_height();
                if (right != nullptr)
                    this->right->check_height();
            }

            void check_balance() {
#ifdef DEBUG_VERBOSE
                std::cerr << this->key_data << ' '; // Changed from key_val_pair.first
#endif
                assert(std::abs(this->left_height() - this->right_height()) <= 1);
                if (left != nullptr)
                    this->left->check_balance();
                if (right != nullptr)
                    this->right->check_balance();
            }
#endif
        };

        AVLNode *root = nullptr;
        size_t size_ = 0;
        // Compare compare_func; // If Compare needs to store state. std::less is stateless.

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
            node->right = node_r->left;
            node_r->left = node;
            if (node->right != nullptr) {
                node->right->parent = node;
            }
            auto &node_p = parent_pointer_to(node);
            node_r->parent = node->parent;
            node->parent = node_r;
            node_p = node_r;
            node->update_height();
            node_r->update_height_chain();
            return node_r;
        }

        AVLNode *rotate_right(AVLNode *node) {
            AVLNode *node_l = node->left;
            node->left = node_l->right;
            node_l->right = node;
            if (node->left != nullptr) {
                node->left->parent = node;
            }
            auto &node_p = parent_pointer_to(node);
            node_l->parent = node->parent;
            node->parent = node_l;
            node_p = node_l;
            node->update_height();
            node_l->update_height_chain();
            return node_l;
        }

        void swap_node_with_empty(AVLNode *node, AVLNode *empty_node) {
            parent_pointer_to(node) = empty_node;
            if (node->left != nullptr)
                node->left->parent = empty_node;
            if (node->right != nullptr)
                node->right->parent = empty_node;
            empty_node->parent = node->parent;
            empty_node->left = node->left;
            empty_node->right = node->right;
            node->parent = node->left = node->right = nullptr;
            empty_node->height = node->height;
        }

        void swap_nodes(AVLNode *node_a, AVLNode *node_b) {
            // Creates a temporary AVLNode C with node_a's data.
            // AVLNode constructor now takes Key directly.
            auto *node_c = new AVLNode(node_a->key_data);
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
        class const_iterator;

        class SharedIteratorMixin {
            friend class set; // Changed from friend class map;

          protected:
            set *origin = nullptr; // Changed from map*
            AVLNode *node = nullptr;

            SharedIteratorMixin() = default;

            SharedIteratorMixin(set *origin_set, AVLNode *current_node) // Changed param name
                : origin(origin_set), node(current_node) {
            }

            [[nodiscard]] bool at_end() const {
                return node == nullptr;
            }

            void increment_self() {
                if (origin == nullptr || origin->root == nullptr || at_end()) {
                    throw std::runtime_error("Invalid Iterator: Cannot increment end or invalid iterator");
                }
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
                    node = nullptr; // Reached end
                }
            }

            void decrement_self() {
                if (origin == nullptr || origin->root == nullptr) {
                    throw std::runtime_error("Invalid Iterator: Cannot decrement on empty or invalid set");
                }
                if (at_end()) {
                    node = origin->root->rightmost();
                    if (node == nullptr) { // Should not happen if origin->root was not nullptr
                        throw std::runtime_error("Invalid Iterator: Empty set but root was not null");
                    }
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
                        // Reached past the beginning
                        throw std::runtime_error("Invalid Iterator: Cannot decrement begin iterator");
                    }
                }
            }
        };

        class iterator : public SharedIteratorMixin {
            friend class const_iterator;
            friend class set; // Allow set to access constructor

          public:
            iterator() = default;

            iterator(set *origin_set, AVLNode *current_node) // Changed param type and name
                : SharedIteratorMixin(origin_set, current_node) {
            }

            iterator(const iterator &other) = default;

            iterator operator++(int) {
                iterator temp = *this;
                this->increment_self();
                return temp;
            }

            iterator &operator++() {
                this->increment_self();
                return *this;
            }

            iterator operator--(int) {
                iterator temp = *this;
                this->decrement_self();
                return temp;
            }

            iterator &operator--() {
                this->decrement_self();
                return *this;
            }

            // For set, operator* returns a const reference to the key
            const Key &operator*() const {
                if (this->node == nullptr)
                    throw std::runtime_error("Dereferencing end iterator or null iterator");
                return this->node->key_data; // Changed from key_val_pair
            }

            const Key *operator->() const noexcept {
                // C++ standard allows -> on end iterators to be UB, but often caught by implementations
                // Here, returning &(this->node->key_data) would crash if node is nullptr.
                // Consider returning a pointer to a temporary if node is null, or assert/throw.
                // For simplicity and matching typical behavior (crash on end iterator dereference):
                // assert(this->node != nullptr); // Or rely on user not to dereference end()
                return &(this->node->key_data); // Changed from key_val_pair
            }

            bool operator==(const iterator &rhs) const {
                return this->node == rhs.node && this->origin == rhs.origin;
            }

            bool operator==(const const_iterator &rhs) const; // Declaration, def after const_iterator

            bool operator!=(const iterator &rhs) const {
                return !(*this == rhs);
            }

            bool operator!=(const const_iterator &rhs) const; // Declaration

            [[nodiscard]] const_iterator as_const() const; // Declaration
        };

        class const_iterator : public SharedIteratorMixin {
            friend class iterator;
            friend class set; // Allow set to access constructor

          public:
            const_iterator() = default;

            const_iterator(const set *origin_set, AVLNode *current_node) // Changed param type
                : SharedIteratorMixin(const_cast<set *>(origin_set), current_node) {
            }

            const_iterator(const const_iterator &other) = default;

            explicit const_iterator(const iterator &other) : SharedIteratorMixin(other.origin, other.node) {
            }

            const_iterator &operator=(const iterator &other) {
                this->origin = other.origin;
                this->node = other.node;
                return *this;
            }

            const_iterator operator++(int) {
                const_iterator temp = *this;
                this->increment_self();
                return temp;
            }

            const_iterator &operator++() {
                this->increment_self();
                return *this;
            }

            const_iterator operator--(int) {
                const_iterator temp = *this;
                this->decrement_self();
                return temp;
            }

            const_iterator &operator--() {
                this->decrement_self();
                return *this;
            }

            const Key &operator*() const {
                if (this->node == nullptr)
                    throw std::runtime_error("Dereferencing end iterator or null iterator");
                return this->node->key_data; // Changed from key_val_pair
            }

            const Key *operator->() const noexcept {
                return &(this->node->key_data); // Changed from key_val_pair
            }

            bool operator==(const iterator &rhs) const {
                return this->node == rhs.node && this->origin == rhs.origin;
            }

            bool operator==(const const_iterator &rhs) const {
                return this->node == rhs.node && this->origin == rhs.origin;
            }

            bool operator!=(const iterator &rhs) const {
                return !(*this == rhs);
            }

            bool operator!=(const const_iterator &rhs) const {
                return !(*this == rhs);
            }
        };

        // Definitions for methods declared in iterator
        // inline bool iterator::operator==(const const_iterator &rhs) const {
        //     return this->node == rhs.node && this->origin == rhs.origin;
        // }
        // inline bool iterator::operator!=(const const_iterator &rhs) const {
        //     return !(*this == rhs);
        // }
        // inline const_iterator iterator::as_const() const {
        //     return {this->origin, this->node};
        // }

        set() = default;

        set(const set &other) { // Changed from map
            clear();
            if (other.root != nullptr)
                root = other.root->deep_copy();
            size_ = other.size();
        }

        set &operator=(const set &other) { // Changed from map
            if (this != &other) {
                clear();
                if (other.root != nullptr)
                    root = other.root->deep_copy();
                size_ = other.size();
            }
            return *this;
        }

        ~set() {
            clear();
        }

        // at() and operator[] are removed for set, as they are map-specific.
        // T &at(const Key &key) -> REMOVED
        // const T &at(const Key &key) const -> REMOVED
        // T &operator[](const Key &key) -> REMOVED
        // const T &operator[](const Key &key) const -> REMOVED

        iterator begin() {
            if (empty())
                return {this, nullptr};
            AVLNode *leftmost_node = root->leftmost();
            return {this, leftmost_node};
        }

        // Add this const overload
        const_iterator begin() const {
            if (empty())
                return {this, nullptr};
            AVLNode *leftmost_node = root->leftmost();
            return {this, leftmost_node};
        }

        const_iterator cbegin() const { // Should already be correct
            if (empty())
                return {this, nullptr};
            AVLNode *leftmost_node = root->leftmost();
            return {this, leftmost_node};
        }

        iterator end() { // Non-const version
            return {this, nullptr};
        }

        // Add this const overload
        const_iterator end() const {
            return {this, nullptr};
        }

        const_iterator cend() const { // Should already be correct
            return {this, nullptr};
        }

        bool empty() const {
            return size_ == 0;
        }

        size_t size() const {
            return size_;
        }

        void clear() {
            if (root != nullptr) {
                root->deep_delete();
                delete root;
                root = nullptr;
                size_ = 0;
            }
        }

      private:
        AVLNode *balance_node(AVLNode *node) {
#ifdef DEBUG_VERBOSE
            std::cerr << "(BN) Balancing Node: " << node->key_data << '\n'; // Changed
#endif
            const int l_height = node->left_height();
            const int r_height = node->right_height();
            if (l_height >= r_height + 2) {
                const int node_ll_height = node->left->left_height();
                const int node_lr_height = node->left->right_height();
                if (node_ll_height >= node_lr_height) {
                    rotate_right(node);
                } else {
                    rotate_left(node->left);
                    rotate_right(node);
                }
            } else if (r_height >= l_height + 2) {
                const int node_rl_height = node->right->left_height();
                const int node_rr_height = node->right->right_height();
                if (node_rr_height >= node_rl_height) {
                    rotate_left(node);
                } else {
                    rotate_right(node->right);
                    rotate_left(node);
                }
            } else {
                // This case should ideally not be reached if called appropriately
                // For example, only call if std::abs(l_height - r_height) >= 2
                // The original code implies it's called when imbalance is detected.
                // If it's called on a balanced node, this error is misleading.
                // However, keeping original logic:
                throw std::runtime_error("Corrupted tree structure or balance_node called on balanced node");
            }
            return node->parent; // node is now a child of the new root of this subtree
        }

        void balance_upstream(AVLNode *node) {
            while (node != nullptr) {
                // const int ori_height = node->height; // Not strictly needed for logic here
                if (std::abs(node->left_height() - node->right_height()) >= 2) {
                    node = balance_node(
                        node); // balance_node returns the new parent of original node (or new root of subtree)
                               // The original node might have moved down.
                               // We need to continue balancing from the parent of the subtree's new root.
                               // balance_node returns node->parent, which is correct.
                }
                node->update_height(); // Update height of current node
                // if (ori_height == node->height && std::abs(node->left_height() - node->right_height()) < 2) break; //
                // Optimization: if height didn't change and node is balanced, ancestors might not need update. AVL
                // guarantees for insertion/deletion often mean only O(1) rotations, but height propagation can go to
                // root.
                node = node->parent;
            }
        }

      public:
        /**
         * insert an element (a Key for set).
         * return a pair, the first of the pair is
         *   the iterator to the new element (or the element that prevented the
         * insertion), the second one is true if insert successfully, or false.
         */
        std::pair<iterator, bool> insert(const Key &k) { // Changed param from value_type to const Key& k
            if (root == nullptr) {
                root = new AVLNode(k); // Pass k directly
                ++size_;
                return {iterator(this, root), true};
            }

            AVLNode *curr = root;
            AVLNode *parent_of_curr = nullptr; // Keep track of parent for insertion

            while (curr != nullptr) {
                parent_of_curr = curr;
                if (Compare()(k, curr->key_data)) { // Compare k with curr->key_data
                    curr = curr->left;
                } else if (Compare()(curr->key_data, k)) { // Compare curr->key_data with k
                    curr = curr->right;
                } else {
                    // Key already exists
                    return {iterator(this, curr), false};
                }
            }

            // Insert new node
            AVLNode *new_node;
            if (Compare()(k, parent_of_curr->key_data)) {
                new_node = parent_of_curr->left = new AVLNode(k, parent_of_curr);
            } else {
                new_node = parent_of_curr->right = new AVLNode(k, parent_of_curr);
            }
            ++size_;

            balance_upstream(parent_of_curr); // Balance from the parent of the new node

            return {iterator(this, new_node), true};
        }

        void erase(iterator pos) {
            if (pos == end() || pos.origin != this || pos.node == nullptr) { // Added pos.node == nullptr check
                throw std::runtime_error("Invalid Iterator for erase");
            }

            AVLNode *node_to_delete = pos.node;
            AVLNode *balance_start_node;

            // If node_to_delete has two children, swap it with its inorder predecessor (or successor)
            // Original code swaps with inorder predecessor (left->rightmost)
            if (node_to_delete->left != nullptr && node_to_delete->right != nullptr) {
                AVLNode *predecessor = node_to_delete->left->rightmost();
                swap_nodes(node_to_delete, predecessor);
                // After swap_nodes, node_to_delete still points to the original memory block of pos.node,
                // but this block is now in the predecessor's original structural position.
                // The actual content to be "kept" (from predecessor) is now at original pos.node's structural spot.
                // The node we want to physically remove is the one `node_to_delete` points to,
                // which is now guaranteed to have at most one child (it was a predecessor).
            }

            // At this point, node_to_delete has at most one child.
#ifdef DEBUG_ASSERTIONS
            // After potential swap, node_to_delete (which is the one to be unlinked)
            // must have at most one child.
            // If it was swapped, it was a predecessor, so its right child was null.
            // If not swapped, it originally had at most one child.
            assert(node_to_delete->left == nullptr || node_to_delete->right == nullptr);
#endif

            balance_start_node = node_to_delete->parent;
            AVLNode *child = (node_to_delete->left != nullptr) ? node_to_delete->left : node_to_delete->right;

            if (child != nullptr) {
                child->parent = balance_start_node;
            }

            if (balance_start_node == nullptr) { // node_to_delete was root
                root = child;
            } else if (balance_start_node->left == node_to_delete) {
                balance_start_node->left = child;
            } else {
                balance_start_node->right = child;
            }

            // Isolate node_to_delete before deleting to prevent its destructor from deleting children it no longer
            // owns.
            node_to_delete->left = nullptr;
            node_to_delete->right = nullptr;
            delete node_to_delete;
            --size_;

            if (balance_start_node != nullptr || root != nullptr) { // only balance if tree not empty
                balance_upstream(
                    balance_start_node); // balance_start_node could be null if root was deleted and tree is now empty
            }
        }

        [[nodiscard]] size_t count(const Key &k) const { // Changed param name
            return (find(k) == cend()) ? 0 : 1;
        }

        [[nodiscard]] iterator find(const Key &k) { // Changed param name
            AVLNode *curr = root;
            while (curr != nullptr) {
                if (Compare()(k, curr->key_data)) { // Compare k with curr->key_data
                    curr = curr->left;
                } else if (Compare()(curr->key_data, k)) { // Compare curr->key_data with k
                    curr = curr->right;
                } else {
                    return iterator(this, curr); // Found
                }
            }
            return end(); // Not found
        }

        [[nodiscard]] const_iterator find(const Key &k) const { // Changed param name
            AVLNode *curr = root;
            while (curr != nullptr) {
                if (Compare()(k, curr->key_data)) { // Compare k with curr->key_data
                    curr = curr->left;
                } else if (Compare()(curr->key_data, k)) { // Compare curr->key_data with k
                    curr = curr->right;
                } else {
                    return const_iterator(this, curr); // Found
                }
            }
            return cend(); // Not found
        }

#ifdef DEBUG_VIS_UTILS
        std::string repr() const {
            if (root == nullptr) {
                return "set()"; // Changed from AVL()
            }
            return "set(" + (root->repr()) + ")"; // Changed from AVL()
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

} // namespace norb