#pragma once

#include "naive_persistent_memory.hpp"
#include "persistent_memory.hpp"
#include "stlite/pair.hpp"

#include <functional>
#include <type_traits>
// This stl is only used in the traverse() function, which is for debugging
// only. traverse() is not used in the main function (src.cpp)
#include <queue>

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

    template <typename idx_t, typename val_t> class BPlusTree {
      private:
        using index_node_val_type_ = typename impl::index_value_type_helper<val_t>::type;
        using index_storage_t_ = Pair<idx_t, index_node_val_type_>;
        using leaf_storage_t_ = Pair<idx_t, val_t>;
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
            static constexpr size_t aux_var_size = sizeof(size_t) * 2;
#ifndef USE_SMALL_BATCH
            static constexpr size_t node_capacity =
                (PAGE_SIZE - aux_var_size) / (sizeof(index_storage_t_) + sizeof(MutableHandle)) - 1;
#else
            static constexpr size_t node_capacity = 8;
#endif
            static constexpr size_t merge_threshold = node_capacity * .25f;
            static constexpr size_t split_threshold = node_capacity * .75f;
            static_assert(PAGE_SIZE - aux_var_size > (sizeof(idx_t) + sizeof(MutableHandle)));
            static_assert(node_capacity >= 4);

            size_t layer = 0;
            size_t size = 0;

            /**
             * Now data refers to the smallest element that has been in the child
             */
            index_storage_t_ data[node_capacity];
            MutableHandle children[node_capacity];
            // MutableHandle parent;
        };

        struct LeafNode {
            static constexpr size_t aux_var_size = sizeof(size_t) + sizeof(MutableHandle);
#ifndef USE_SMALL_BATCH
            static constexpr size_t node_capacity = (PAGE_SIZE - aux_var_size) / sizeof(leaf_storage_t_);
#else
            static constexpr size_t node_capacity = 8;
#endif
            static constexpr size_t merge_threshold = node_capacity * .25f;
            static constexpr size_t split_threshold = node_capacity * .75f;
            static_assert(PAGE_SIZE - aux_var_size > sizeof(leaf_storage_t_));
            static_assert(node_capacity >= 4);

            size_t size = 0;

            leaf_storage_t_ data[node_capacity];
            MutableHandle sibling;
            // MutableHandle parent;
        };

        /**
         * @brief Calculates the node that contains the beginning of key.
         * @param node Const reference to the index node to check.
         * @param key The key to search for.
         * @return The index to the next node to search in.
         */
        static size_t lower_bound(const IndexNode &node, const idx_t &key) {
            size_t left = 0, right = node.size;
            while (left < right) {
                const size_t mid = (left + right) / 2;
                if (node.data[mid].first >= key)
                    right = mid;
                else
                    left = mid + 1;
            }
            return (left > 1) ? (left - 1) : 0;
        }

        /**
         * @brief Calculates the node that contains the beginning of key.
         * @param node Const reference to the index node to check.
         * @param index The key-val pair to search for.
         * @return The index to the next node to search in.
         */
        static size_t lower_bound(const IndexNode &node, const index_storage_t_ &index) {
            size_t left = 0, right = node.size - 1;
            while (left < right) {
                const size_t mid = (left + right + 1) / 2;
                if (node.data[mid] > index)
                    right = mid - 1;
                else
                    left = mid;
            }
            return left;
        }

        /**
         * @brief Calculates the first key-val pair starting with no less than the
         * given key.
         * @param node Const reference to the leaf node to check.
         * @param key The key to search for.
         * @return The position of the appropriate pair, [important] maximum being
         * node.size.
         */
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

        /**
         * @brief Calculates the first key-val pair no less than the given key-val
         * pair.
         * @param node Const reference to the leaf node to check.
         * @param target The key-val pair to search for.
         * @return The position of the appropriate pair, [important] maximum being
         * node.size.
         */
        static size_t lower_bound(const LeafNode &node, const leaf_storage_t_ &target) {
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

        std::pair<MutableHandle, vector<stack_frame_t_>> stack_descend_to_leaf(const index_storage_t_ &index) {
            MutableHandle handle = root_handle.val;
            vector<stack_frame_t_> history;
            for (int i = 0; i < tree_height.val - 1; i++) {
                const auto &index_node_ref = *handle.const_ref<IndexNode>();
                const auto next_node_idx = lower_bound(index_node_ref, index);
                history.push_back({handle, next_node_idx});
                handle = index_node_ref.children[next_node_idx];
                assert(!handle.is_nullptr());
            } // travel down the tree
            return std::make_pair(handle, history);
        }

        /**
         * @brief Calculates a possible index a given pair can be inserted at.
         * @param starting_block The leaf block to start looking from.
         * @param target The desired key-val pair.
         * @return pair of the mutable handle to the block and the position within
         * that block.
         * @note This adjusted version assumes that the descent is precise!
         */
        std::pair<MutableHandle, size_t> get_insertion_pos(const MutableHandle &starting_block,
                                                           const leaf_storage_t_ &target) const {
            const MutableHandle leaf = starting_block;
            auto leaf_ptr = leaf.const_ref<LeafNode>().as_raw_ptr();
            // MutableHandle last_leaf_handle;
            // size_t last_leaf_size;
            // while (!leaf_ptr->sibling.is_nullptr()) {
            //   if (leaf_ptr->data[leaf_ptr->size - 1] >= target)
            //     break;
            //   last_leaf_handle = leaf;
            //   last_leaf_size = leaf_ptr->size;
            //   leaf = leaf_ptr->sibling;
            //   leaf_ptr = leaf.const_ref<LeafNode>().as_raw_ptr();
            // }
            const auto insertion_pos = lower_bound(*leaf_ptr, target);
            // if (insertion_pos == 0 && !last_leaf_handle.is_nullptr())
            //   return std::make_pair(last_leaf_handle, last_leaf_size);
            // else
            return std::make_pair(leaf, insertion_pos);
        }

        // Auxiliary functions dealing with overflow and underflow

        bool handle_leaf_overflow(const stack_frame_t_ &frame) {
            // three slots are needed to perform this function
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t insert_at_pos = frame.second;
            auto old_node_href = parent_node_href->children[insert_at_pos].template ref<LeafNode>();
            // dup the leaf node
            const MutableHandle new_node_handle = PersistentMemory::create_mutable_and_init<LeafNode>();
            auto new_node_href = new_node_handle.ref<LeafNode>();
            // update the sizes
            const auto new_leaf_size = (new_node_href->size = old_node_href->size / 2);
            const auto remaining_size = (old_node_href->size -= new_leaf_size);
            // migrate the contents
            // [Boost] Consider removing memset to boost performance around 1x
            array::migrate(new_node_href->data, old_node_href->data + remaining_size, new_leaf_size);
            // change the sib pointers
            new_node_href->sibling = old_node_href->sibling;
            old_node_href->sibling = new_node_handle;
            // insert into parent
            // auto key_for_parent = new_node_href->data[0].first;
            // auto val_for_parent_idx = impl::get_value_for_index_node(new_node_href->data[0].second);
            array::insert_at(parent_node_href->data, parent_node_href->size, insert_at_pos + 1,
                             impl::get_hashed_pair(new_node_href->data[0]));
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
            // update the sizes
            new_node_href->layer = old_node_href->layer;
            const size_t new_node_size = new_node_href->size = old_node_href->size / 2;
            const size_t old_node_size = old_node_href->size -= new_node_size;
            // migrate the contents
            array::migrate(new_node_href->data, old_node_href->data + old_node_size, new_node_size);
            array::migrate(new_node_href->children, old_node_href->children + old_node_size, new_node_size);
            // insert into parent
            array::insert_at(parent_node_href->data, parent_node_href->size, insert_at_pos + 1, new_node_href->data[0]);
            array::insert_at(parent_node_href->children, parent_node_href->size, insert_at_pos + 1, new_node_handle);
            ++parent_node_href->size;
            return parent_node_href->size >= IndexNode::split_threshold;
        }

        void handle_root_overflow(const node_type &root_node_is) {
            const auto new_root_handle = PersistentMemory::create_mutable_and_init<IndexNode>();
            auto new_root_href = new_root_handle.template ref<IndexNode>();
            new_root_href->layer = tree_height.val++;
            new_root_href->size = 1;
            if (root_node_is == node_type::leaf) {
                // MODIFIED: Convert leaf_storage_t_ to index_storage_t_
                const auto &leaf_data_zero = root_handle.val.const_ref<LeafNode>()->data[0];
                new_root_href->data[0] = impl::get_hashed_pair(leaf_data_zero);
            } else { // root_node_is == node_type::index
                new_root_href->data[0] = root_handle.val.const_ref<IndexNode>()->data[0];
            }
            new_root_href->children[0] = root_handle.val;
            root_handle.val = new_root_handle;
            if (root_node_is == node_type::index)
                handle_index_overflow({new_root_handle, 0});
            else // root_node_is == node_type::leaf
                handle_leaf_overflow({new_root_handle, 0});
        }

        /**
         * @brief Merge a leaf's right sibling into the node.
         * @param node_id The number of the node in the sequence of the parent.
         * @note Side effects: the parent will be updated regarding size and proper
         * data. The function assumes that a right leaf exists.
         */
        void merge_leaf_with_right(const MutableHandle &parent_handle, const size_t &node_id) {
            auto parent_node_href = parent_handle.ref<IndexNode>();
            assert(node_id < parent_node_href->size);
            auto old_node_href = parent_node_href->children[node_id].template ref<LeafNode>();
            auto right_node_handle = parent_node_href->children[node_id + 1];
            auto right_node_href = right_node_handle.template ref<LeafNode>();
            // migrate the contents
            array::migrate(old_node_href->data + old_node_href->size, right_node_href->data, right_node_href->size);
            old_node_href->size += right_node_href->size;
            // migrate the sibling
            old_node_href->sibling = right_node_href->sibling;
            right_node_href->sibling.set_nullptr();
            // change the parent
            array::remove_at(parent_node_href->data, parent_node_href->size, node_id + 1);
            array::remove_at(parent_node_href->children, parent_node_href->size, node_id + 1);
            --parent_node_href->size;
            // remove the page
            PersistentMemory::remove<LeafNode>(right_node_handle);
        }

        /**
         * @brief Merge an index's right sibling into the node.
         * @param node_id The number of the node in the sequence of the parent.
         * @note Side effects: the parent will be updated regarding size and proper
         * data. The function assumes that a right leaf exists.
         */
        void merge_index_with_right(const MutableHandle &parent_handle, const size_t &node_id) {
            auto parent_node_href = parent_handle.ref<IndexNode>();
            auto old_node_href = parent_node_href->children[node_id].template ref<IndexNode>();
            auto right_node_handle = parent_node_href->children[node_id + 1];
            auto right_node_href = right_node_handle.template ref<IndexNode>();
            // migrate the contents
            array::migrate(old_node_href->data + old_node_href->size, right_node_href->data, right_node_href->size);
            array::migrate(old_node_href->children + old_node_href->size, right_node_href->children,
                           right_node_href->size);
            old_node_href->size += right_node_href->size;
            // change the parent
            array::remove_at(parent_node_href->data, parent_node_href->size, node_id + 1);
            array::remove_at(parent_node_href->children, parent_node_href->size, node_id + 1);
            --parent_node_href->size;
            // remove the page
            PersistentMemory::remove<LeafNode>(right_node_handle);
        }

        /**
         * @return Whether going on is needed.
         */
        bool handle_leaf_underflow(const stack_frame_t_ &frame) {
            // A. borrow if possible
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t old_child_at_pos = frame.second;
            auto old_child_handle = parent_node_href->children[old_child_at_pos];
            auto old_child_href = old_child_handle.template ref<LeafNode>();
            // A1. borrow from left
            if (old_child_at_pos != 0 &&
                parent_node_href->children[old_child_at_pos - 1].template const_ref<LeafNode>()->size >
                    LeafNode::merge_threshold + 1) {
                auto left_child_href = parent_node_href->children[old_child_at_pos - 1].template ref<LeafNode>();
                const auto to_insert = left_child_href->data[--left_child_href->size];
                array::insert_at(old_child_href->data, old_child_href->size++, 0, to_insert);
                // update the parent
                parent_node_href->data[old_child_at_pos] = impl::get_hashed_pair(to_insert);
                return false;
            }
            // A2. borrow from right
            if (old_child_at_pos < parent_node_href->size - 1 &&
                parent_node_href->children[old_child_at_pos + 1].template const_ref<LeafNode>()->size >
                    LeafNode::merge_threshold + 1) {
                auto right_child_href = parent_node_href->children[old_child_at_pos + 1].template ref<LeafNode>();
                const auto to_insert = right_child_href->data[0];
                array::remove_at(right_child_href->data, right_child_href->size, 0);
                --(right_child_href->size);
                old_child_href->data[old_child_href->size++] = to_insert;
                // update the parent
                parent_node_href->data[old_child_at_pos + 1] =
                    impl::get_hashed_pair(right_child_href->data[0]); // this is the new data
                return false;
            }
            // B. merge with sibling if not
            if (old_child_at_pos < parent_node_href->size - 1)
                merge_leaf_with_right(frame.first, old_child_at_pos);
            else
                merge_leaf_with_right(frame.first, old_child_at_pos - 1);
            return parent_node_href->size <= IndexNode::merge_threshold;
        }

        bool handle_index_underflow(const stack_frame_t_ &frame) {
            // A. borrow if possible
            auto parent_node_href = frame.first.ref<IndexNode>();
            const size_t old_child_at_pos = frame.second;
            auto old_child_handle = parent_node_href->children[old_child_at_pos];
            auto old_child_href = old_child_handle.template ref<IndexNode>();
            // A1. borrow from left
            if (old_child_at_pos != 0 &&
                parent_node_href->children[old_child_at_pos - 1].template const_ref<IndexNode>()->size >
                    IndexNode::merge_threshold + 1) {
                auto left_child_href = parent_node_href->children[old_child_at_pos - 1].template ref<IndexNode>();
                // push the new data
                const auto left_size = left_child_href->size;
                auto data_to_insert = left_child_href->data[left_size - 1];
                auto child_to_insert = left_child_href->children[left_size - 1];
                array::insert_at(old_child_href->data, old_child_href->size, 0, data_to_insert);
                array::insert_at(old_child_href->children, old_child_href->size, 0, child_to_insert);
                ++old_child_href->size;
                // remove the old data
                left_child_href->data[left_size - 1].~index_storage_t_();
                left_child_href->children[left_size - 1].~MutableHandle();
                --left_child_href->size;
                // update parent
                parent_node_href->data[old_child_at_pos] = data_to_insert;
                return false;
            }
            // A2. borrow from right
            if (old_child_at_pos < parent_node_href->size - 1 &&
                parent_node_href->children[old_child_at_pos + 1].template const_ref<IndexNode>()->size >
                    IndexNode::merge_threshold + 1) {
                auto right_child_href = parent_node_href->children[old_child_at_pos + 1].template ref<IndexNode>();
                // push the new data
                auto data_to_insert = right_child_href->data[0];
                auto child_to_insert = right_child_href->children[0];
                const auto old_size = old_child_href->size;
                old_child_href->data[old_size] = data_to_insert;
                old_child_href->children[old_size] = child_to_insert;
                ++old_child_href->size;
                // remove the old data
                array::remove_at(right_child_href->data, right_child_href->size, 0);
                array::remove_at(right_child_href->children, right_child_href->size, 0);
                --right_child_href->size;
                // update parent
                parent_node_href->data[old_child_at_pos + 1] =
                    right_child_href->data[0]; // the new data (the new minimum)
                return false;
            }
            // B. merge if not
            if (old_child_at_pos < parent_node_href->size - 1)
                merge_index_with_right(frame.first, old_child_at_pos);
            else
                merge_index_with_right(frame.first, old_child_at_pos - 1);
            return parent_node_href->size <= IndexNode::merge_threshold;
        }

        void handle_root_underflow(const node_type &root_node_is) {
            // this is simple: the root will underflow only when it contains exactly
            // one element
            tree_height.val -= 1;
            const auto old_root_handle = root_handle.val;
            if (root_node_is == node_type::index) {
                root_handle.val = old_root_handle.const_ref<IndexNode>()->children[0];
                PersistentMemory::remove<IndexNode>(old_root_handle);
            } else {
                root_handle.val.set_nullptr();
                PersistentMemory::remove<LeafNode>(old_root_handle);
            }
        }

      public:
        BPlusTree() = default;
        ~BPlusTree() = default;

        [[nodiscard]] size_t size() const {
            return tree_size.val;
        }

        /**
         * @brief Find all entries registered under a given key and perform an
         * action sequentially on them, ordered by value.
         * @param key The key to match.
         * @param function The function to perform.
         */
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
            // finally, handle should point to a leaf node
            const LeafNode *leaf_node_ref = handle.const_ref<LeafNode>().as_raw_ptr();
            size_t cur = lower_bound(*leaf_node_ref, key);
            while (true) {
                for (; cur < leaf_node_ref->size; ++cur) {
                    if (leaf_node_ref->data[cur].first != key)
                        return;
                    function(leaf_node_ref->data[cur].second);
                }
                // reset cur and move on to the next node
                cur = 0;
                handle = leaf_node_ref->sibling;
                if (handle.is_nullptr())
                    return;
                leaf_node_ref = handle.const_ref<LeafNode>().as_raw_ptr();
            }
        }

        /**
         * @brief Find all entries registered under a key and return them in a
         * norb::vector.
         * @param key The key to match.
         * @return A vector containing all found entries.
         */
        [[nodiscard]] vector<val_t> find_all(const idx_t &key) const {
            vector<val_t> ret;
            const auto lambda = [&ret](const val_t &val) { ret.push_back(val); };
            find_all_do(key, lambda);
            return ret;
        }

        /**
         * @brief Register a key-value pair.
         * @param key The key to register under.
         * @param val The value to register.
         */
        void insert(const idx_t &key, const val_t &val) {
            ++tree_size.val;
            if (tree_height.val == 0) {
                // create the first node and insert the element
                root_handle.val = PersistentMemory::create_mutable_and_init<LeafNode>();
                LeafNode &node = *root_handle.val.ref<LeafNode>();
                node.data[node.size++] = norb::make_pair(key, val);
                ++tree_height.val;
                return;
            }
            auto index_for_descent = norb::make_pair(key, impl::get_hashed_value(val));
            auto [handle, history] = stack_descend_to_leaf(index_for_descent);
            auto [leaf_node_handle, within_leaf_node_pos] = get_insertion_pos(handle, norb::make_pair(key, val));
            auto leaf_node_href = leaf_node_handle.template ref<LeafNode>();
            array::insert_at(leaf_node_href->data, leaf_node_href->size, within_leaf_node_pos,
                             norb::make_pair(key, val));
            ++leaf_node_href->size;
            // if exceeds the upperbound, travel up
            int cur = history.size() - 1;
            bool go_on = false;
            if (leaf_node_href->size >= LeafNode::split_threshold && cur >= 0)
                go_on = handle_leaf_overflow(history[cur--]);
            while (cur >= 0 && go_on)
                go_on = handle_index_overflow(history[cur--]);
            // handle root
            if (tree_height.val == 1 &&
                root_handle.val.template const_ref<LeafNode>()->size >= LeafNode::split_threshold)
                handle_root_overflow(node_type::leaf);
            else if (tree_height.val > 1 && root_handle.val.const_ref<IndexNode>()->size >= IndexNode::split_threshold)
                handle_root_overflow(node_type::index);
        }

        /**
         * @brief Remove a key-value pair.
         * @param key The key to match.
         * @param val The value to match.
         * @return Whether the removal is successful.
         */
        bool remove(const idx_t &key, const val_t &val) {
            if (tree_height.val == 0)
                return false;
            auto index_for_descent = norb::make_pair(key, impl::get_hashed_value(val));
            auto [handle, history] = stack_descend_to_leaf(index_for_descent);
            auto [leaf_node_handle, within_leaf_node_pos] = get_insertion_pos(handle, norb::make_pair(key, val));
            if (const auto leaf_node_const_href = leaf_node_handle.template const_ref<LeafNode>();
                leaf_node_const_href->size <= within_leaf_node_pos ||
                leaf_node_const_href->data[within_leaf_node_pos].first != key ||
                leaf_node_const_href->data[within_leaf_node_pos].second != val)
                return false;
            // the value exists and the pair should be removed
            --tree_size.val;
            const auto leaf_node_href = leaf_node_handle.template ref<LeafNode>();
            array::remove_at(leaf_node_href->data, leaf_node_href->size, within_leaf_node_pos);
            --leaf_node_href->size;
            int cur = history.size() - 1; // cursor on the history stack
            bool go_on = false;
            if (leaf_node_href->size <= LeafNode::merge_threshold && cur >= 0)
                go_on = handle_leaf_underflow(history[cur--]);
            if (!go_on)
                return true;
            while (cur >= 0) {
                go_on = handle_index_underflow(history[cur--]);
                if (!go_on)
                    return true;
            }
            if (tree_height.val == 1) {
                if (root_handle.val.const_ref<LeafNode>()->size <= 1)
                    handle_root_underflow(node_type::leaf);
            } else {
                if (root_handle.val.const_ref<IndexNode>()->size <= 1)
                    handle_root_underflow(node_type::index);
            }
            return true;
        }

        int remove_all(const idx_t &key) {
            const auto vals = find_all(key);
            for (const auto &val: vals) {
                remove(key, val);
            }
            return vals.size();
        }

        // This function is used for debugging, and is not called in src.cpp
        void traverse(const bool &do_check = false) const requires (Ostreamable<idx_t> and Ostreamable<val_t>){
            std::cout << "--- Traversing B+ Tree (" << this << ") ---" << std::endl;
            std::cout << "[Info] Size: " << tree_size.val << ", Height: " << tree_height.val << std::endl;

            if (root_handle.val.is_nullptr()) {
                std::cout << "[Tree] Empty" << std::endl;
                std::cout << "--- End Traversal ---" << std::endl << std::endl;
                return;
            }

            std::cout << "[Info] Root Page ID: " << root_handle.val.page_id << std::endl;

            std::queue<std::pair<MutableHandle, size_t>> q; // Store {Handle, Level}
            q.emplace(root_handle.val, 0);                  // Start with root at level 0

            size_t current_level = 0;
            std::cout << "Level " << current_level << ":" << std::endl;

            while (!q.empty()) {
                auto [current_handle, node_level] = q.front();
                q.pop();

                // Check if we moved to a new level
                if (node_level > current_level) {
                    current_level = node_level;
                    std::cout << "Level " << current_level << ":" << std::endl;
                }

                // Determine node type based on level
                // Leaves are at level tree_height - 1
                bool is_leaf = (node_level == tree_height.val - 1);

                std::cout << "  Node (Page ID: " << current_handle.page_id << ") ";

                if (is_leaf) {
                    // --- Print Leaf Node ---
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

                    // Optional checks for leaf node (if do_check is true)
                    if (do_check) {
                        // Check key order
                        for (size_t i = 0; i + 1 < node_ref->size; ++i) {
                            assert(node_ref->data[i] <= node_ref->data[i + 1] && "Leaf key order violation");
                        }
                        // Check size constraints (except for root if it's the only node)
                        if (tree_height.val > 1) { // Not root
                            assert(node_ref->size >= LeafNode::merge_threshold && "Leaf underflow violation");
                        }
                        assert(node_ref->size <= LeafNode::node_capacity && "Leaf overflow violation");
                    }

                } else {
                    // --- Print Index Node ---
                    assert(!current_handle.is_nullptr() && "Corrupted handle");
                    auto node_ref = current_handle.const_ref<IndexNode>();

                    std::cout << "[Index] Size: " << node_ref->size << " (Layer: " << node_ref->layer
                              << ") | Children/Keys: ";

                    // Print children and keys interleaved
                    for (size_t i = 0; i < node_ref->size; ++i) {
                        // Print child pointer
                        std::cout << "C" << i << ":";
                        if (node_ref->children[i].is_nullptr()) {
                            std::cout << "NULL";
                        } else {
                            std::cout << node_ref->children[i].page_id;
                            // Enqueue child for the next level
                            q.push({node_ref->children[i], node_level + 1});
                        }
                        std::cout << "[" << node_ref->data[i].first << node_ref->data[i].second << ", "
                                  << node_ref->data[i + 1].first << node_ref->data[i + 1].second << ") ";
                    }
                    std::cout << std::endl;

                    // Optional checks for index node (if do_check is true)
                    if (do_check) {
                        // Check key order
                        for (size_t i = 0; i + 1 < node_ref->size; ++i) {
                            //! assert(node_ref->data[i] <= node_ref->data[i + 1] &&
                            //!        "Index key order violation");
                        }
                        // Check size constraints (except for root)
                        if (node_level > 0) { // Not root
                            assert(node_ref->size >= IndexNode::merge_threshold && "Index underflow violation");
                        }
                        assert(node_ref->size <= IndexNode::node_capacity && "Index overflow violation");
                        // Basic check: Ensure children handles are not null (unless error
                        // state)
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