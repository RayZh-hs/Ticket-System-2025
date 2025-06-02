#pragma once

#include "naive_persistent_memory.hpp"
#include "persistent_memory.hpp"
#include "stlite/pair.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <queue> // only used in debug
#include <stlite/range.hpp>
#include <string>
#include <type_traits>

namespace norb {
    namespace impl {

        // Trait to check for val_t::id_t and val_t::id() -> val_t::id_t
        template <typename T, typename = void> struct has_id_interface : std::false_type {};

        template <typename T>
        struct has_id_interface<T,
                                std::void_t<typename T::id_t,                        // Check for nested type id_t
                                            decltype(std::declval<const T &>().id()) // Check for id() method
                                            >>
            : std::integral_constant<bool, std::is_same_v<decltype(std::declval<const T &>().id()),
                                                          typename T::id_t> // Check return type of id()
                                     > {};

        template <typename T> inline constexpr bool has_id_interface_v = has_id_interface<T>::value;

        template <typename V, typename Enable = void> struct index_value_type_helper {
            using type = V; // Default to V itself
        };

        template <typename V> struct index_value_type_helper<V, std::enable_if_t<has_id_interface_v<V>>> {
            using type = typename V::id_t;
        };

        // Helper function to extract the value for index storage
        template <typename V> auto get_hashed_value(const V &value) {
            if constexpr (has_id_interface_v<V>) {
                return value.id();
            } else {
                return value;
            }
        }

        template <typename T, typename V> auto get_hashed_pair(const Pair<T, V> &key_value) {
            if constexpr (has_id_interface_v<V>) {
                return Pair(key_value.first, key_value.second.id());
            } else {
                return key_value;
            }
        }
    } // namespace impl

    enum idx_type {
        AUTOMATIC = 0,
        MANUAL = 1,
    };

    template <typename idx_t, typename val_t, const idx_type index_node_type = AUTOMATIC> class BPlusTree {
      private:
        // automatic storage types
        using index_node_val_type_ = typename impl::index_value_type_helper<val_t>::type;
        using automatic_index_storage_t = Pair<idx_t, index_node_val_type_>;
        // automatic vs. manual
        using index_storage_t = std::conditional_t<index_node_type == MANUAL, idx_t, automatic_index_storage_t>;
        using leaf_storage_t = Pair<idx_t, val_t>;
        using MutableHandle = PersistentMemory::MutableHandle;
        template <typename val_t_> using TrackedConfig = NaivePersistentMemory::tracker_t_<val_t_>;
        using stack_frame_t_ = std::pair<MutableHandle, size_t>;
        enum node_type { index, leaf };

      public:
        struct IndexNode;
        struct LeafNode;

        TrackedConfig<size_t> tree_height = NaivePersistentMemory::track<size_t>(0);
        TrackedConfig<size_t> tree_size = NaivePersistentMemory::track<size_t>(0);
        TrackedConfig<MutableHandle> root_handle = NaivePersistentMemory::track<MutableHandle>();

        struct IndexNode {
            static constexpr size_t aux_var_size = sizeof(size_t) * 2; // layer, size
#ifndef USE_SMALL_BATCH
            static constexpr size_t node_capacity =
                (PAGE_SIZE - aux_var_size) / (sizeof(index_storage_t) + sizeof(MutableHandle)) - 1;
#else
            static constexpr size_t node_capacity = 8;
#endif
            static constexpr size_t merge_threshold = node_capacity * .25f;
            static constexpr size_t split_threshold = node_capacity * .75f;
            static_assert(PAGE_SIZE - aux_var_size > (sizeof(index_storage_t) + sizeof(MutableHandle)));
            static_assert(node_capacity >= 4);

            size_t layer = 0;
            size_t size = 0;

            /**
             * Now data refers to the smallest element that has been in the child
             */
            index_storage_t data[node_capacity];
            MutableHandle children[node_capacity];
            // MutableHandle parent;
        };

        struct LeafNode {
            static constexpr size_t aux_var_size = sizeof(size_t) + sizeof(MutableHandle); // size, sibling
#ifndef USE_SMALL_BATCH
            static constexpr size_t node_capacity = (PAGE_SIZE - aux_var_size) / sizeof(leaf_storage_t);
#else
            static constexpr size_t node_capacity = 8;
#endif
            static constexpr size_t merge_threshold = node_capacity * .25f;
            static constexpr size_t split_threshold = node_capacity * .75f;
            static_assert(PAGE_SIZE - aux_var_size > sizeof(leaf_storage_t));
            static_assert(node_capacity >= 4);

            size_t size = 0;

            leaf_storage_t data[node_capacity];
            MutableHandle sibling;
            // MutableHandle parent;
        };

        static size_t lower_bound(const IndexNode &node, const idx_t &key) {
            size_t left = 0, right = node.size;
            while (left < right) {
                const size_t mid = (left + right) / 2;
                if constexpr (index_node_type == MANUAL) {
                    if (node.data[mid] >= key) // node.data[mid] is idx_t
                        right = mid;
                    else
                        left = mid + 1;
                } else {                             // AUTOMATIC
                    if (node.data[mid].first >= key) // node.data[mid] is index_storage_t_
                        right = mid;
                    else
                        left = mid + 1;
                }
            }
            return (left > 1) ? (left - 1) : 0;
        }

        static size_t lower_bound(const IndexNode &node, const automatic_index_storage_t &index) {
            if (node.size == 0)
                return 0; // Safety for empty node
            size_t l = 0, r = node.size - 1;

            while (l < r) {
                const size_t mid = (l + r + 1) / 2;
                if constexpr (index_node_type == MANUAL) {
                    if (node.data[mid] > index.first) // node.data[mid] is idx_t, compare with index.first
                        r = mid - 1;
                    else
                        l = mid;
                } else {                        // AUTOMATIC
                    if (node.data[mid] > index) // node.data[mid] is index_storage_t_
                        r = mid - 1;
                    else
                        l = mid;
                }
            }
            return l;
        }

        static size_t lower_bound(const LeafNode &node, const idx_t &key) {
            size_t left = 0, right = node.size;
            while (left < right) {
                const size_t mid = (left + right) / 2;
                if (node.data[mid].first >= key)
                    right = mid;
                else
                    left = mid + 1;
            }
            return left;
        }

        static size_t lower_bound(const LeafNode &node, const leaf_storage_t &target) {
            size_t left = 0, right = node.size;
            while (left < right) {
                const size_t mid = (left + right) / 2;
                if (node.data[mid] >= target)
                    right = mid;
                else
                    left = mid + 1;
            }
            return left;
        }

        std::pair<MutableHandle, vector<stack_frame_t_>> stack_descend_to_leaf(const automatic_index_storage_t &index) {
            MutableHandle handle = root_handle.val;
            vector<stack_frame_t_> history;
            for (int i = 0; i < tree_height.val - 1; i++) {
                const auto &index_node_ref = *handle.const_ref<IndexNode>();
                const auto next_node_idx = lower_bound(index_node_ref, index);
                history.push_back({handle, next_node_idx});
                handle = index_node_ref.children[next_node_idx];
                assert(!handle.is_nullptr());
            }
            return std::make_pair(handle, history);
        }

        std::pair<MutableHandle, size_t> get_insertion_pos(const MutableHandle &starting_block,
                                                           const leaf_storage_t &target) const {
            const MutableHandle leaf = starting_block;
            auto leaf_ptr = leaf.const_ref<LeafNode>().as_raw_ptr();
            const auto insertion_pos = lower_bound(*leaf_ptr, target);
            return std::make_pair(leaf, insertion_pos);
        }

        bool handle_leaf_overflow(const stack_frame_t_ &frame) {
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t insert_at_pos = frame.second; // This is the index of the child that overflowed
            auto old_node_href = parent_node_href->children[insert_at_pos].template ref<LeafNode>();
            const MutableHandle new_node_handle = PersistentMemory::create_mutable_and_init<LeafNode>();
            auto new_node_href = new_node_handle.ref<LeafNode>();

            const auto new_leaf_size = (new_node_href->size = old_node_href->size / 2);
            const auto remaining_size = (old_node_href->size -= new_leaf_size);
            array::migrate(new_node_href->data, old_node_href->data + remaining_size, new_leaf_size);

            new_node_href->sibling = old_node_href->sibling;
            old_node_href->sibling = new_node_handle;

            index_storage_t key_for_parent_data;
            if constexpr (index_node_type == MANUAL) {
                key_for_parent_data = new_node_href->data[0].first;
            } else { // AUTOMATIC
                key_for_parent_data = impl::get_hashed_pair(new_node_href->data[0]);
            }
            // New key/child are inserted at insert_at_pos + 1
            array::insert_at(parent_node_href->data, parent_node_href->size, insert_at_pos + 1, key_for_parent_data);
            array::insert_at(parent_node_href->children, parent_node_href->size, insert_at_pos + 1, new_node_handle);
            ++parent_node_href->size;
            return parent_node_href->size >= IndexNode::split_threshold;
        }

        bool handle_index_overflow(const stack_frame_t_ &frame) {
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t insert_at_pos = frame.second;
            auto old_node_href = parent_node_href->children[insert_at_pos].template ref<IndexNode>();
            const MutableHandle new_node_handle = PersistentMemory::create_mutable_and_init<IndexNode>();
            auto new_node_href = new_node_handle.ref<IndexNode>();

            new_node_href->layer = old_node_href->layer;
            const size_t new_node_size = new_node_href->size = old_node_href->size / 2;
            const size_t old_node_size = old_node_href->size -=
                new_node_size; // old_node_href->size is now remaining size

            array::migrate(new_node_href->data, old_node_href->data + old_node_size, new_node_size);
            array::migrate(new_node_href->children, old_node_href->children + old_node_size, new_node_size);

            // new_node_href->data[0] is already actual_index_node_data_storage_t_
            array::insert_at(parent_node_href->data, parent_node_href->size, insert_at_pos + 1, new_node_href->data[0]);
            array::insert_at(parent_node_href->children, parent_node_href->size, insert_at_pos + 1, new_node_handle);
            ++parent_node_href->size;
            return parent_node_href->size >= IndexNode::split_threshold;
        }

        void handle_root_overflow(const node_type &root_node_is) {
            const auto new_root_handle = PersistentMemory::create_mutable_and_init<IndexNode>();
            auto new_root_href = new_root_handle.template ref<IndexNode>();
            new_root_href->layer = tree_height.val; // Current height, will be incremented effectively by new root
            tree_height.val++;                      // Increment tree height

            // New root initially points to the old root.
            // The old root (child 0 of new root) will then be split.
            new_root_href->children[0] = root_handle.val;

            if (root_node_is == node_type::leaf) {
                const auto &leaf_data_zero = root_handle.val.const_ref<LeafNode>()->data[0];
                if constexpr (index_node_type == MANUAL) {
                    new_root_href->data[0] = leaf_data_zero.first;
                } else { // AUTOMATIC
                    new_root_href->data[0] = impl::get_hashed_pair(leaf_data_zero);
                }
            } else { // root_node_is == node_type::index
                new_root_href->data[0] = root_handle.val.const_ref<IndexNode>()->data[0];
            }
            new_root_href->size = 1; // New root has one key and one child initially

            root_handle.val = new_root_handle; // Update tree's root handle

            // Now, split the child of the new root (which is the old root)
            if (root_node_is == node_type::index)
                handle_index_overflow({new_root_handle, 0}); // This will make new_root_href->size = 2
            else                                             // root_node_is == node_type::leaf
                handle_leaf_overflow({new_root_handle, 0});  // This will make new_root_href->size = 2
        }

        void merge_leaf_with_right(const MutableHandle &parent_handle, const size_t &node_id) {
            auto parent_node_href = parent_handle.ref<IndexNode>();
            assert(node_id < parent_node_href->size - 1 && "Node must have a right sibling to merge with");
            auto old_node_href = parent_node_href->children[node_id].template ref<LeafNode>();
            auto right_node_handle = parent_node_href->children[node_id + 1];
            auto right_node_href = right_node_handle.template ref<LeafNode>();

            array::migrate(old_node_href->data + old_node_href->size, right_node_href->data, right_node_href->size);
            old_node_href->size += right_node_href->size;
            old_node_href->sibling = right_node_href->sibling;

            // Remove the key and child pointer for the merged (right) node from parent
            array::remove_at(parent_node_href->data, parent_node_href->size, node_id + 1);
            array::remove_at(parent_node_href->children, parent_node_href->size, node_id + 1);
            --parent_node_href->size;
            PersistentMemory::remove<LeafNode>(right_node_handle);
        }

        void merge_index_with_right(const MutableHandle &parent_handle, const size_t &node_id) {
            auto parent_node_href = parent_handle.ref<IndexNode>();
            assert(node_id < parent_node_href->size - 1 && "Node must have a right sibling to merge with");
            auto old_node_href = parent_node_href->children[node_id].template ref<IndexNode>();
            auto right_node_handle = parent_node_href->children[node_id + 1];
            auto right_node_href = right_node_handle.template ref<IndexNode>();

            array::migrate(old_node_href->data + old_node_href->size, right_node_href->data, right_node_href->size);
            array::migrate(old_node_href->children + old_node_href->size, right_node_href->children,
                           right_node_href->size);
            old_node_href->size += right_node_href->size;

            array::remove_at(parent_node_href->data, parent_node_href->size, node_id + 1);
            array::remove_at(parent_node_href->children, parent_node_href->size, node_id + 1);
            --parent_node_href->size;
            PersistentMemory::remove<IndexNode>(right_node_handle); // Corrected from LeafNode to IndexNode
        }

        bool handle_leaf_underflow(const stack_frame_t_ &frame) {
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t old_child_at_pos = frame.second;
            auto old_child_href = parent_node_href->children[old_child_at_pos].template ref<LeafNode>();

            // A1. Borrow from left
            if (old_child_at_pos != 0 &&
                parent_node_href->children[old_child_at_pos - 1].template const_ref<LeafNode>()->size >
                    LeafNode::merge_threshold + 1) { // Note: B+ tree usually checks > merge_threshold
                auto left_child_href = parent_node_href->children[old_child_at_pos - 1].template ref<LeafNode>();
                const auto to_insert = left_child_href->data[--left_child_href->size];        // Take last from left
                array::insert_at(old_child_href->data, old_child_href->size++, 0, to_insert); // Insert first in current

                if constexpr (index_node_type == MANUAL) {
                    parent_node_href->data[old_child_at_pos] = to_insert.first;
                } else { // AUTOMATIC
                    parent_node_href->data[old_child_at_pos] = impl::get_hashed_pair(to_insert);
                }
                return false; // No further action needed up the tree
            }
            // A2. Borrow from right
            if (old_child_at_pos < parent_node_href->size - 1 &&
                parent_node_href->children[old_child_at_pos + 1].template const_ref<LeafNode>()->size >
                    LeafNode::merge_threshold + 1) {
                auto right_child_href = parent_node_href->children[old_child_at_pos + 1].template ref<LeafNode>();
                const auto to_insert = right_child_href->data[0];                    // Take first from right
                array::remove_at(right_child_href->data, right_child_href->size, 0); // remove_at handles dtor
                --(right_child_href->size);
                old_child_href->data[old_child_href->size++] = to_insert; // Insert last in current

                if constexpr (index_node_type == MANUAL) {
                    parent_node_href->data[old_child_at_pos + 1] = right_child_href->data[0].first;
                } else { // AUTOMATIC
                    parent_node_href->data[old_child_at_pos + 1] = impl::get_hashed_pair(right_child_href->data[0]);
                }
                return false; // No further action needed
            }

            // B. Merge with sibling
            if (old_child_at_pos < parent_node_href->size - 1) // Merge with right sibling
                merge_leaf_with_right(frame.first, old_child_at_pos);
            else // old_child_at_pos is the rightmost child, merge it with its left sibling
                merge_leaf_with_right(frame.first, old_child_at_pos - 1);

            return (parent_node_href->template size) < IndexNode::merge_threshold;
        }

        bool handle_index_underflow(const stack_frame_t_ &frame) {
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t old_child_at_pos = frame.second;
            auto old_child_href = parent_node_href->children[old_child_at_pos].template ref<IndexNode>();

            // A1. Borrow from left
            if (old_child_at_pos != 0 &&
                parent_node_href->children[old_child_at_pos - 1].template const_ref<IndexNode>()->size >
                    IndexNode::merge_threshold + 1) {
                auto left_child_href = parent_node_href->children[old_child_at_pos - 1].template ref<IndexNode>();
                const auto left_child_current_size = left_child_href->size; // Size before taking element

                auto data_to_insert_in_child =
                    left_child_href->data[left_child_current_size - 1]; // Key from left sibling
                auto child_to_insert_in_child =
                    left_child_href->children[left_child_current_size - 1]; // Child from left sibling

                array::insert_at(old_child_href->data, old_child_href->size, 0, data_to_insert_in_child);
                array::insert_at(old_child_href->children, old_child_href->size, 0, child_to_insert_in_child);
                ++old_child_href->size;

                // not used Assuming direct manipulation of size and elements here, so manual dtor call. If
                // array::remove_at were used on left_child_href, it would handle this.
                if constexpr (!std::is_trivially_destructible_v<index_storage_t>) {
                    left_child_href->data[left_child_current_size - 1].~actual_index_node_data_storage_t_();
                }
                // Assuming MutableHandle might need explicit destruction if not trivial
                if constexpr (!std::is_trivially_destructible_v<MutableHandle>) {
                    left_child_href->children[left_child_current_size - 1].~MutableHandle();
                }
                --left_child_href->size;

                parent_node_href->data[old_child_at_pos] = data_to_insert_in_child; // Update parent key
                return false;
            }
            // A2. Borrow from right
            if (old_child_at_pos < parent_node_href->size - 1 &&
                parent_node_href->children[old_child_at_pos + 1].template const_ref<IndexNode>()->size >
                    IndexNode::merge_threshold + 1) {
                auto right_child_href = parent_node_href->children[old_child_at_pos + 1].template ref<IndexNode>();

                auto data_to_insert_in_child = right_child_href->data[0];      // Key from right sibling
                auto child_to_insert_in_child = right_child_href->children[0]; // Child from right sibling

                const auto current_old_child_size = old_child_href->size;
                old_child_href->data[current_old_child_size] = data_to_insert_in_child;
                old_child_href->children[current_old_child_size] = child_to_insert_in_child;
                ++old_child_href->size;

                array::remove_at(right_child_href->data, right_child_href->size, 0); // remove_at handles dtor
                array::remove_at(right_child_href->children, right_child_href->size, 0);
                --right_child_href->size;

                parent_node_href->data[old_child_at_pos + 1] = right_child_href->data[0]; // Update parent key
                return false;
            }

            // B. Merge
            if (old_child_at_pos < parent_node_href->size - 1)
                merge_index_with_right(frame.first, old_child_at_pos);
            else
                merge_index_with_right(frame.first, old_child_at_pos - 1);

            return (parent_node_href->size) < IndexNode::merge_threshold;
        }

        void handle_root_underflow(const node_type &root_node_is) {
            const auto old_root_handle = root_handle.val;
            if (root_node_is == node_type::index) {
                // An index root underflows if it has 0 keys (implying 1 child)
                // This single child becomes the new root.
                assert(old_root_handle.const_ref<IndexNode>()->size == 0 &&
                       "Index root underflow implies 0 keys, 1 child");
                root_handle.val = old_root_handle.const_ref<IndexNode>()->children[0];
                PersistentMemory::remove<IndexNode>(old_root_handle);
            } else { // Leaf root
                // A leaf root underflows if it becomes empty.
                assert(old_root_handle.const_ref<LeafNode>()->size == 0 && "Leaf root underflow implies 0 elements");
                root_handle.val.set_nullptr(); // Tree is now empty
                PersistentMemory::remove<LeafNode>(old_root_handle);
            }
            tree_height.val -= 1; // Height decreases in both cases
            if (tree_height.val == 0) {
                assert(root_handle.val.is_nullptr() && tree_size.val == 0);
            }
        }

      public:
        BPlusTree() = default;
        ~BPlusTree() = default;

        [[nodiscard]] size_t size() const {
            return tree_size.val;
        }

        void find_all_do(const idx_t &key, const std::function<void(const val_t &)> &function) const {
            if (tree_height.val == 0)
                return;
            MutableHandle handle = root_handle.val;
            for (int i = 0; i < tree_height.val - 1; i++) {
                const auto &index_node_ref = *handle.const_ref<IndexNode>();
                const auto next_node_idx = lower_bound(index_node_ref, key);
                handle = index_node_ref.children[next_node_idx];
                assert(!handle.is_nullptr());
            }
            const LeafNode *leaf_node_ref = handle.const_ref<LeafNode>().as_raw_ptr();
            size_t cur = lower_bound(*leaf_node_ref, key);
            while (true) {
                for (; cur < leaf_node_ref->size; ++cur) {
                    if (leaf_node_ref->data[cur].first != key)
                        return;
                    function(leaf_node_ref->data[cur].second);
                }
                cur = 0;
                handle = leaf_node_ref->sibling;
                if (handle.is_nullptr())
                    return;
                leaf_node_ref = handle.const_ref<LeafNode>().as_raw_ptr();
            }
        }

        [[nodiscard]] vector<val_t> find_all(const idx_t &key) const {
            vector<val_t> ret;
            const auto lambda = [&ret](const val_t &val) { ret.push_back(val); };
            find_all_do(key, lambda);
            return ret;
        }

        void find_all_in_range_do(Range<idx_t> range, const std::function<void(const val_t &)> &function) const {
            if (tree_height.val == 0 || range.is_empty())
                return;
            MutableHandle handle = root_handle.val;
            for (int i = 0; i < tree_height.val - 1; i++) {
                const auto &index_node_ref = *handle.const_ref<IndexNode>();
                const auto next_node_idx = lower_bound(index_node_ref, range.get_from());
                handle = index_node_ref.children[next_node_idx];
                assert(!handle.is_nullptr());
            }
            const LeafNode *leaf_node_ref = handle.const_ref<LeafNode>().as_raw_ptr();
            size_t cur = lower_bound(*leaf_node_ref, range.get_from());
            while (true) {
                for (; cur < leaf_node_ref->size; ++cur) {
                    const auto key_data = leaf_node_ref->data[cur].first;
                    if (not range.contains_from_right(key_data))
                        return;
                    if (range.contains_from_left(key_data))
                        function(leaf_node_ref->data[cur].second);
                }
                cur = 0;
                handle = leaf_node_ref->sibling;
                if (handle.is_nullptr())
                    return;
                leaf_node_ref = handle.const_ref<LeafNode>().as_raw_ptr();
            }
        }

        [[nodiscard]] vector<val_t> find_all_in_range(const Range<idx_t> &key) const {
            vector<val_t> ret;
            const auto lambda = [&ret](const val_t &val) { ret.push_back(val); };
            find_all_in_range_do(key, lambda);
            return ret;
        }

        void insert(const idx_t &key, const val_t &val) {
            if (tree_height.val == 0) { // Empty tree
                root_handle.val = PersistentMemory::create_mutable_and_init<LeafNode>();
                LeafNode &node = *root_handle.val.ref<LeafNode>();
                node.data[node.size++] = norb::make_pair(key, val);
                tree_height.val = 1;
                tree_size.val = 1;
                return;
            }

            tree_size.val++;
            auto index_for_descent = norb::make_pair(key, impl::get_hashed_value(val));
            auto [handle, history] = stack_descend_to_leaf(index_for_descent);
            auto [leaf_node_handle, within_leaf_node_pos] = get_insertion_pos(handle, norb::make_pair(key, val));
            auto leaf_node_href = leaf_node_handle.template ref<LeafNode>();

            array::insert_at(leaf_node_href->data, leaf_node_href->size, within_leaf_node_pos,
                             norb::make_pair(key, val));
            ++leaf_node_href->size;

            bool needs_parent_split = false;
            if (leaf_node_href->size >= LeafNode::split_threshold) {
                if (tree_height.val == 1) { // Root is the leaf that overflowed
                    handle_root_overflow(node_type::leaf);
                } else { // Leaf is not root, propagate overflow upwards if needed
                    assert(!history.empty());
                    needs_parent_split = handle_leaf_overflow(history.back());
                    history.pop_back();
                }
            }

            while (needs_parent_split && !history.empty()) {
                needs_parent_split = handle_index_overflow(history.back());
                history.pop_back();
            }

            if (needs_parent_split) { // Root (an index node) itself needs to split
                assert(history.empty() && tree_height.val > 1);
                handle_root_overflow(node_type::index);
            }
        }

        size_t count(const idx_t &key) const {
            size_t counter = 0;
            find_all_do(key, [&counter](const val_t &) { counter++; });
            return counter;
        }

        size_t count_in_range(const Range<idx_t> &range) const {
            size_t counter = 0;
            find_all_in_range_do(range, [&counter](const val_t &) { counter++; });
            return counter;
        }

        bool remove(const idx_t &key, const val_t &val) {
            if (tree_height.val == 0)
                return false;

            auto index_for_descent = norb::make_pair(key, impl::get_hashed_value(val));
            auto [handle, history] = stack_descend_to_leaf(index_for_descent);
            auto [leaf_node_handle, within_leaf_node_pos] = get_insertion_pos(handle, norb::make_pair(key, val));

            const auto leaf_node_const_href = leaf_node_handle.template const_ref<LeafNode>();
            if (within_leaf_node_pos >= leaf_node_const_href->size ||
                leaf_node_const_href->data[within_leaf_node_pos].first != key ||
                leaf_node_const_href->data[within_leaf_node_pos].second != val)
                return false; // Element not found

            tree_size.val--;
            auto leaf_node_href = leaf_node_handle.template ref<LeafNode>();
            array::remove_at(leaf_node_href->data, leaf_node_href->size, within_leaf_node_pos);
            --leaf_node_href->size;

            bool needs_parent_merge = false;
            if (tree_height.val == 1) {                     // Root is a leaf
                if (leaf_node_href->size == 0) {            // Root leaf became empty
                    handle_root_underflow(node_type::leaf); // Tree becomes empty
                }
            } else { // Tree height > 1, leaf is not root
                if ((leaf_node_href->size) < LeafNode::merge_threshold) {
                    assert(!history.empty());
                    needs_parent_merge = handle_leaf_underflow(history.back());
                    history.pop_back();
                }
            }

            while (needs_parent_merge && !history.empty()) {
                needs_parent_merge = handle_index_underflow(history.back());
                history.pop_back();
            }

            if (needs_parent_merge) { // Root (an index node) itself underflowed
                assert(history.empty() && tree_height.val > 1);
                // If index root has 0 keys (size refers to keys), it means it has 1 child.
                // This child becomes the new root.
                if (root_handle.val.const_ref<IndexNode>()->size == 0) {
                    handle_root_underflow(node_type::index);
                }
            }
            return true;
        }

        int remove_all(const idx_t &key) {
            const auto vals = find_all(key);
            for (const auto &val : vals) {
                remove(key, val);
            }
            return vals.size();
        }

        void traverse(const bool &do_check = false) const
            requires(Ostreamable<idx_t> && Ostreamable<val_t> &&
                     (index_node_type == MANUAL || Ostreamable<index_node_val_type_>))
        {
            std::cout << "--- Traversing B+ Tree (" << this << ") ---" << std::endl;
            std::cout << "[Info] Size: " << tree_size.val << ", Height: " << tree_height.val << std::endl;

            if (root_handle.val.is_nullptr()) {
                std::cout << "[Tree] Empty" << std::endl;
                std::cout << "--- End Traversal ---" << std::endl << std::endl;
                return;
            }

            std::cout << "[Info] Root Page ID: " << root_handle.val.page_id << std::endl;

            std::queue<std::pair<MutableHandle, size_t>> q;
            q.emplace(root_handle.val, 0);

            size_t current_level = 0;
            std::cout << "Level " << current_level << ":" << std::endl;

            while (!q.empty()) {
                auto [current_handle, node_level] = q.front();
                q.pop();

                if (node_level > current_level) {
                    current_level = node_level;
                    std::cout << "\nLevel " << current_level << ":" << std::endl; // Added newline for better formatting
                }

                bool is_leaf = (node_level == tree_height.val - 1);
                std::cout << "  Node (Page ID: " << current_handle.page_id << ") ";

                if (is_leaf) {
                    assert(!current_handle.is_nullptr() && "Corrupted handle");
                    auto node_ref = current_handle.const_ref<LeafNode>();
                    std::cout << "[Leaf] Size: " << node_ref->size << " | Sibling: "
                              << (node_ref->sibling.is_nullptr() ? "NULL" : std::to_string(node_ref->sibling.page_id))
                              << " | Data: [";
                    for (size_t i = 0; i < node_ref->size; ++i) {
                        std::cout << "(" << node_ref->data[i].first << "," << node_ref->data[i].second << ")"
                                  << (i == node_ref->size - 1 ? "" : ", ");
                    }
                    std::cout << "]" << std::endl;
                    if (do_check) {
                        for (size_t i = 0; i + 1 < node_ref->size; ++i) {
                            assert(node_ref->data[i] <= node_ref->data[i + 1] && "Leaf key order violation");
                        }
                        if (tree_height.val > 1 && node_level < tree_height.val - 1) { // Not root or not the only node
                            assert(node_ref->size >= LeafNode::merge_threshold && "Leaf underflow violation");
                        } else if (tree_height.val == 1 && tree_size.val > 0 && node_ref->size == 0) {
                            // This case can happen if the last element is removed from a root leaf.
                            // It should be handled by handle_root_underflow making the tree empty.
                        } else if (tree_height.val > 1 && (node_ref->size) < LeafNode::merge_threshold &&
                                   (node_ref->size) > 0) {
                            // This is an underflow if not root. Root leaf can have < merge_threshold elements.
                            // The original check was `if (tree_height.val > 1)`
                            assert(node_ref->size >= LeafNode::merge_threshold &&
                                   "Leaf underflow violation (non-root)");
                        }
                        assert(node_ref->size <= LeafNode::node_capacity && "Leaf overflow violation");
                    }

                } else { // Index Node
                    assert(!current_handle.is_nullptr() && "Corrupted handle");
                    auto node_ref = current_handle.const_ref<IndexNode>();
                    std::cout << "[Index] Size: " << node_ref->size << " (Layer: " << node_ref->layer
                              << ") | Children/Keys: \n"; // Added newline

                    for (size_t i = 0; i < node_ref->size; ++i) {
                        std::cout << "    C" << i << ":"; // Indent for readability
                        if (node_ref->children[i].is_nullptr()) {
                            std::cout << "NULL";
                        } else {
                            std::cout << node_ref->children[i].page_id;
                            q.push({node_ref->children[i], node_level + 1});
                        }
                        std::cout << " Range=[";
                        if constexpr (index_node_type == MANUAL) {
                            std::cout << node_ref->data[i];
                        } else { // AUTOMATIC
                            std::cout << "(" << node_ref->data[i].first << "," << node_ref->data[i].second << ")";
                        }
                        std::cout << ", ";
                        if (i + 1 < node_ref->size) {
                            if constexpr (index_node_type == MANUAL) {
                                std::cout << node_ref->data[i + 1];
                            } else { // AUTOMATIC
                                std::cout << "(" << node_ref->data[i + 1].first << "," << node_ref->data[i + 1].second
                                          << ")";
                            }
                        } else {
                            std::cout << "inf";
                        }
                        std::cout << ") \n"; // Added newline
                    }
                    // std::cout << std::endl; // Removed as \n is now in the loop
                    if (do_check) {
                        for (size_t i = 0; i + 1 < node_ref->size; ++i) {
                            if constexpr (index_node_type == MANUAL) {
                                assert(node_ref->data[i] <= node_ref->data[i + 1] && "Index key order violation");
                            } else {
                                assert(node_ref->data[i].first <= node_ref->data[i + 1].first &&
                                       "Index key order violation");
                                // Could also check second part of pair if firsts are equal
                                if (node_ref->data[i].first == node_ref->data[i + 1].first) {
                                    assert(node_ref->data[i].second <= node_ref->data[i + 1].second &&
                                           "Index key.second order violation");
                                }
                            }
                        }
                        if (node_level > 0) { // Not root
                            assert(node_ref->size >= IndexNode::merge_threshold && "Index underflow violation");
                        } else { // Is root index node
                            assert(node_ref->size >= 1 && "Root index node must have at least 1 key (unless it's about "
                                                          "to be demoted with 0 keys/1 child)");
                        }
                        assert(node_ref->size <= IndexNode::node_capacity && "Index overflow violation");
                        for (size_t i = 0; i < node_ref->size; ++i) {
                            assert(!node_ref->children[i].is_nullptr() && "Index node has null child pointer");
                        }
                    }
                }
            }
            std::cout << "--- End Traversal ---" << std::endl << std::endl;
        }
    };
} // namespace norb