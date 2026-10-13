// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cassert>
#include <list>
#include <optional>
#include <unordered_map>

namespace evmone
{
/// Least Recently Used (LRU) cache.
///
/// A map of Key to Value with a fixed capacity. When the cache is full, a newly inserted entry
/// replaces (evicts) the least recently used entry.
/// All operations have O(1) complexity.
template <typename Key, typename Value>
class LRUCache
{
    struct LRUEntry
    {
        /// Reference to the existing key in the map.
        ///
        /// This is needed to get the LRU element in the map when eviction is needed.
        ///  Pointers to node-based map entries are always valid.
        /// TODO: Optimal solution would be to use the map iterator. They are also always valid
        ///   because the map capacity is reserved up front and rehashing never happens.
        ///   However, the type definition would be recursive: Map(List(Map::iterator)), so we need
        ///   to use some kind of type erasure. We prototyped such implementation, but decided not
        ///   to include it in the first version. Similar solution is also described in
        ///   https://stackoverflow.com/a/54808013/725174.
        const Key& key;

        /// The cached value.
        Value value;
    };

    using LRUList = std::list<LRUEntry>;
    using LRUIterator = typename LRUList::iterator;
    using Map = std::unordered_map<Key, LRUIterator>;

    /// The fixed capacity of the cache.
    const size_t capacity_;

    /// The list to order the cache entries by the usage. The front element is the least recently
    /// used entry.
    ///
    /// In classic implementations the order in the list is reversed (the front element is the most
    /// recently used entry). We decided to keep the order as is because
    /// it simplifies the implementation and better fits the underlying list structure.
    ///
    /// TODO: The intrusive list works better here but such implementation variant has been omitted
    ///   from the initial version.
    LRUList lru_list_;

    /// The map of Keys to Values indirectly via the LRU list.
    ///
    /// The Value doesn't have to be in the LRU list but instead can be placed in the map directly
    /// next to the LRU iterator. We decided to keep this classic layout because we didn't notice
    /// any performance difference.
    Map map_;

    /// Marks an element as the most recently used by moving it to the back of the LRU list.
    void move_to_back(LRUIterator it) noexcept { lru_list_.splice(lru_list_.end(), lru_list_, it); }

public:
    /// Constructs the LRU cache with the given capacity.
    ///
    /// @param capacity  The fixed capacity of the cache. It must not be 0.
    explicit LRUCache(size_t capacity) : capacity_{capacity}
    {
        assert(capacity_ != 0);

        // Reserve map to the full capacity to prevent any rehashing.
        map_.reserve(capacity);
    }

    /// Clears the cache by deleting all the entries.
    void clear() noexcept
    {
        map_.clear();
        lru_list_.clear();
    }


    /// Retrieves the copy of the value associated with the specified key.
    ///
    /// @param key  The key of the entry to retrieve.
    /// @return     An optional containing the copy of the value if the key is found,
    ///             or an empty optional if not.
    std::optional<Value> get(const Key& key) noexcept
    {
        if (const auto it = map_.find(key); it != map_.end())
        {
            move_to_back(it->second);
            return it->second->value;
        }
        return {};
    }

    /// Inserts or updates the value associated with the specified key.
    ///
    /// @param key    The key of the entry to insert or update.
    /// @param value  The value to associate with the key.
    void put(Key key, Value value)
    {
        // Implementation is split into two variants: cache full or not.
        // Once the cache is full, its size never shrinks. Therefore, from now on this variant is
        // always executed.

        if (map_.size() == capacity_)
        {
            // When the cache is full, avoid the erase-emplace pattern by using the map's node API.

            using std::swap;  // for the ADL-aware swap usage

            // Get the least recently used element.
            auto lru_it = lru_list_.begin();

            // Extract the map node with the to-be-evicted element and reuse it for the new
            // key-value pair. This makes the operation allocation-free.
            auto node = map_.extract(lru_it->key);  // node.key() is LRU key
            swap(node.key(), key);                  // node.key() is insert key, key is LRU key
            if (auto [it, inserted, node2] = map_.insert(std::move(node)); !inserted)
            {
                // Failed re-insertion means the element with the new key is already in the cache.
                // Roll back the eviction by re-inserting the node with the original key back.
                // node2 is the same node passed to the insert() with unchanged .key().
                swap(key, node2.key());  // key is existing insert key, node2.key() is LRU key
                map_.insert(std::move(node2));

                // Returned iterator points to the element matching the key
                // which value must be updated.
                lru_it = it->second;
            }
            lru_it->value = std::move(value);  // Replace/update the value.
            move_to_back(lru_it);
        }
        else
        {
            // The cache is not full. Insert the new element into the cache.
            if (const auto [it, inserted] = map_.try_emplace(std::move(key)); !inserted)
            {
                // If insertion failed, the key is already in the cache, so only update the value.
                it->second->value = std::move(value);
                move_to_back(it->second);
            }
            else
            {
                // After successful insertion also create the LRU list entry and connect it with
                // the map entry. This reference is valid and unchanged through
                // the whole cache lifetime.
                it->second = lru_list_.emplace(lru_list_.end(), it->first, std::move(value));
            }
        }
    }
};

}  // namespace evmone
