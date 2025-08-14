
#pragma once
#include "asserts/asserts.h"
#include "defines.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    constexpr auto HASH_MAP_DEFAULT_CAPACITY    = 32;
    constexpr auto HASH_MAP_DEFAULT_LOAD_FACTOR = 0.75;

    /**
     * @brief Implementation of a HashMap with Open-Adressing using Robin Hood probing and backshift deletion.
     *
     * @tparam Key The Key type used to index into the HashMap
     * @tparam Value The Value type used with this HashMap
     * @tparam HashFunc The HashFunction used to hash Keys before inserting them
     * @tparam LF Determines when the HashMap needs to grow in size (0.0 < LoadFactor <= 1.0, where a LoadFactor of 0.5 means that
     * when the HashMap is 50% full we grow the HashMap)
     * @tparam Allocator The allocator used by this HashMap
     */
    template <class Key, class Value, class HashFunc = std::hash<Key>, double LF = HASH_MAP_DEFAULT_LOAD_FACTOR, class Allocator = DynamicAllocator>
    class HashMap
    {
        static_assert(LF > 0.0, "The Load Factor of a HashMap must be > 0.0");
        static_assert(LF <= 1.0, "The Load Factor of a HashMap must be <= 1.0");

        class HashMapIterator
        {
            void FindNextOccupiedIndex()
            {
                // Iterate over our elements and find the next element that is also occupied
                // We start at the element after our current element (which is at m_index)
                for (auto i = m_index + 1; i < m_map->m_capacity; i++)
                {
                    if (m_map->m_nodes[i].occupied)
                    {
                        // We found a new element that is occupied so we store it's index as our new m_index
                        m_index = i;
                        return;
                    }
                }

                // If we get to this part we could not find a next occupied element
                // so we set our index to the end of the internal array
                m_index = m_map->m_capacity;
            }

        public:
            using DifferenceType    = std::ptrdiff_t;
            using Pointer           = Value*;
            using Reference         = Value&;
            using iterator_category = std::forward_iterator_tag;

            HashMapIterator() = default;

            explicit HashMapIterator(const HashMap* map) : m_index(INVALID_ID_U64), m_map(map) { FindNextOccupiedIndex(); }

            HashMapIterator(const HashMap* map, const u64 currentIndex) : m_index(currentIndex), m_map(map) { FindNextOccupiedIndex(); }

            // Dereference operator
            Reference operator*() const { return m_map->m_nodes[m_index].value; }

            // Pre and post-increment operators
            HashMapIterator& operator++()
            {
                FindNextOccupiedIndex();
                return *this;
            }

            HashMapIterator operator++(int) { return HashMapIterator(m_map, m_index); }

            bool operator==(const HashMapIterator& other) const { return m_map == other.m_map && m_index == other.m_index; }

            bool operator!=(const HashMapIterator& other) const { return m_map != other.m_map || m_index != other.m_index; }

        private:
            u64 m_index          = 0;
            const HashMap* m_map = nullptr;
        };

    public:
        struct Node
        {
            Key key;
            Value value;
            bool occupied = false;
        };

        HashMap(Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) {}

        HashMap(const HashMap& other) { Copy(other); }
        HashMap& operator=(const HashMap& other)
        {
            Copy(other);
            return *this;
        }

        HashMap(HashMap&&)            = delete;
        HashMap& operator=(HashMap&&) = delete;

        ~HashMap() { Destroy(); }

        void Create()
        {
            // Allocate memory for all the buckets
            m_nodes    = m_allocator->Allocate<Node>(MemoryType::HashMap, HASH_MAP_DEFAULT_CAPACITY);
            m_capacity = HASH_MAP_DEFAULT_CAPACITY;
        }

        /**
         * @brief Clears all the entries from the hashmap without destroying underlying memory
         * meaning you can keep using the hashmap as if it was just freshly created
         */
        void Clear()
        {
            if (m_nodes)
            {
                // Clear all the occupied buckets
                for (u64 i = 0; i < m_capacity; ++i)
                {
                    auto& node = m_nodes[i];
                    if (node.occupied)
                    {
                        node.key.~Key();
                        node.value.~Value();
                        node.occupied = false;
                    }
                }
                // Reset the number of items
                m_count = 0;
            }
        }

        /**
         * @brief Destroys the Hashmap. This clears the Hashmap entirely and also deletes it's internal memory
         */
        void Destroy()
        {
            // First we clear (which calls the destructor for all active keys and values)
            Clear();
            // Then we free our actual memory
            if (m_nodes)
            {
                m_allocator->Free(m_nodes);
                m_nodes    = nullptr;
                m_capacity = 0;
            }
        }

        /**
         * @brief Inserts/Sets the provided <Key, Value> in the HashMap
         * If there is a <Key, Value> pair with the same Key already, it's value will be overwritten
         *
         * @param key The key you want to use
         * @param value The value you want to insert
         */
        void Set(Key key, Value value) { Insert(key, value); }

        /**
         * @brief Inserts/Sets the provided <Key, Value> in the HashMap
         * If there is a <Key, Value> pair with the same Key already, it's value will be overwritten
         *
         * @param key The key you want to use
         * @param value The value you want to insert
         */
        void Insert(Key key, Value value)
        {
#ifdef _DEBUG
            C3D_ASSERT_MSG(m_nodes != nullptr, "Tried Insert() before HashMap.Create() was called.");
#endif
            // Make sure that we grow our HashMap when we reach our Load Factor
            if (m_count >= m_capacity * LF)
            {
                Grow();
            }

            // Turn our key into an initial index
            u64 index = IndexFor(key);
            // Keep track of our PSL (Probe Sequence Length, the number of slots from our initial desired spot for our <Key, Value> pair)
            u64 psl = 0;

            for (;;)
            {
                auto& current = m_nodes[index];

                // First we check if the slot is occupied
                if (!current.occupied)
                {
                    // We have found an empty slot so we copy over our key and value
                    current.key   = std::move(key);
                    current.value = std::move(value);
                    // Mark this slot as occupied
                    current.occupied = true;
                    // Increase our count since we have added an item
                    m_count++;
                    return;
                }

                // Slot is already full so let's check it's key
                if (current.key == key)
                {
                    // We have found our matching item so let's replace it's value
                    current.value = value;
                    // No need to increase our count since this key was already present
                    return;
                }

                // This slot does not match our key so we move on
                u64 desiredIndex = IndexFor(current.key);
                // Calculate the PSL for the slot we are currently checking
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);

                if (psl > currentPsl)
                {
                    // Our PSL is greater then the PSL of the slot we are currently checking, so we swap
                    std::swap(key, current.key);
                    std::swap(value, current.value);
                    // Now our PSL is equal to the PSL of the slot we just swapped for
                    psl = currentPsl;
                }

                // We will move to the next slot
                // So we increment our PSL
                psl++;
                // And we increase our index
                index = Mod(index + 1);
            }
        }

        /**
         * @brief Deletes the <Key, Value> pair with the provided Key.
         * If the <Key, Value> pair is not found, nothing will happen.
         *
         * @param key The Key of the <Key, Value> pair you want to remove
         */
        void Delete(const Key& key)
        {
#ifdef _DEBUG
            C3D_ASSERT_MSG(m_nodes != nullptr, "Tried Delete() before HashMap.Create() was called.");
#endif

            // Get the initial index for our key
            auto index = IndexFor(key);
            // The PSL initially will be 0
            u64 psl = 0;
            for (;;)
            {
                auto& current = m_nodes[index];

                if (!current.occupied)
                {
                    // The slot is empty so there is nothing to delete
                    return;
                }

                if (current.key == key)
                {
                    // We have found our key so we can decrease the count and do a backwards shift
                    m_count--;
                    BackwardsShift(index);
                    return;
                }

                // This slot does not contain our key so we move on
                u64 desiredIndex = IndexFor(current.key);
                // Calculate the PSL for our current slot
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);
                if (currentPsl < psl)
                {
                    // The PSL of our current slot is smaller than the PSL of the Key we are looking for
                    // This means that by definition our key is not in this HashMap since it would have swapped places on insert
                    return;
                }

                // Move on to the next slot
                psl++;
                index = Mod(index + 1);
            }
        }

        /**
         * @brief Gets the Value out of the <Key, Value> pair with the provided Key.
         *
         * @param key The Key you want to search for
         * @return A reference to the value belonging to the provided Key
         */
        Value& Get(const Key& key)
        {
            // Get the initial index for our Key
            u64 index = IndexFor(key);
            // The PSL initially will be 0
            u64 psl = 0;
            for (;;)
            {
                auto& current = m_nodes[index];
                // If the slot is empty the <Key, Value> pair does not exist
                if (!current.occupied)
                {
                    C3D_FAIL("The provided key does not exist");
                }
                // Check if this slot contains our key
                if (current.key == key)
                {
                    // Return our value since we have found our Key
                    return current.value;
                }

                // Our slot does not contain our key so we move on
                u64 desiredIndex = IndexFor(current.key);
                // Calculate the PSL for our current slot
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);
                if (currentPsl < psl)
                {
                    // The PSL of our current slot is smaller than the PSL of the Key we are looking for
                    // This means that by definition our key is not in this HashMap since it would have swapped places on insert
                    C3D_FAIL("The provided key does not exist");
                }

                // Move on to the next slot
                psl++;
                index = Mod(index + 1);
            }
        }

        /**
         * @brief Gets the Value out of the <Key, Value> pair with the provided Key.
         *
         * @param key The Key you want to search for
         * @return A const reference to the value belonging to the provided Key
         */
        [[nodiscard]] const Value& Get(const Key& key) const
        {
            // Get the initial index for our Key
            u64 index = IndexFor(key);
            // The PSL initially will be 0
            u64 psl = 0;
            for (;;)
            {
                auto& current = m_nodes[index];
                // If the slot is empty the <Key, Value> pair does not exist
                if (!current.occupied)
                {
                    C3D_FAIL("The provided key does not exist");
                }
                // Check if this slot contains our key
                if (current.key == key)
                {
                    // Return our value since we have found our Key
                    return current.value;
                }

                // Our slot does not contain our key so we move on
                u64 desiredIndex = IndexFor(current.key);
                // Calculate the PSL for our current slot
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);
                if (currentPsl < psl)
                {
                    // The PSL of our current slot is smaller than the PSL of the Key we are looking for
                    // This means that by definition our key is not in this HashMap since it would have swapped places on insert
                    C3D_FAIL("The provided key does not exist");
                }

                // Move on to the next slot
                psl++;
                index = Mod(index + 1);
            }
        }

        /**
         * @brief Checks if the provided Key is present in the HashMap
         *
         * @param key The Key you want to check for.
         * @return True if the key is present; false otherwise
         */
        [[nodiscard]] bool Has(const Key& key) const
        {
            // Get the initial index for our Key
            u64 index = IndexFor(key);
            // The PSL initially will be 0
            u64 psl = 0;
            for (;;)
            {
                auto& current = m_nodes[index];
                // If the slot is empty we can immediatly return false

                if (!current.occupied) return false;
                // Check if this slot contains our key
                if (current.key == key) return true;

                // Our slot does not contain our key so we move on
                u64 desiredIndex = IndexFor(current.key);
                // Calculate the PSL for our current slot
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);
                if (currentPsl < psl)
                {
                    // The PSL of our current slot is smaller than the PSL of the Key we are looking for
                    // This means that by definition our key is not in this HashMap since it would have swapped places on insert
                    return false;
                }

                // Move on to the next slot
                psl++;
                index = Mod(index + 1);
            }
        }

        [[nodiscard]] bool Contains(const Key& key) const { return Has(key); }

        Value& operator[](const Key& key) { return Get(key); }

        [[nodiscard]] const Value& operator[](const Key& key) const { return Get(key); }

        [[nodiscard]] HashMapIterator begin() const { return HashMapIterator(this); }

        [[nodiscard]] HashMapIterator end() const { return HashMapIterator(this, m_capacity); }

        [[nodiscard]] u64 Capacity() const { return m_capacity; }

        [[nodiscard]] u64 Count() const { return m_count; }

        [[nodiscard]] double LoadFactor() const { return LF; }

    private:
        void Create(u64 capacity)
        {
            if (m_nodes == nullptr && m_capacity == 0)
            {
                // Allocate memory for all the nodes
                m_nodes    = m_allocator->Allocate<Node>(MemoryType::HashMap, capacity);
                m_capacity = capacity;
            }
        }

        void Grow()
        {
            // Take a copy of our old capacity
            auto oldCapacity = m_capacity;
            // Also take a pointer to our old nodes
            auto oldNodes = m_nodes;
            // Reset our count to 0
            m_count = 0;
            // Now grow our capacity by multiplying by 2 (to always ensure we are at a power of 2)
            m_capacity *= 2;
            // Allocate new space for our nodes
            m_nodes = m_allocator->Allocate<Node>(MemoryType::HashMap, m_capacity);
            // Iterate over all our nodes and insert them again (to rehash with our new capacity)
            for (u64 i = 0; i < oldCapacity; ++i)
            {
                auto& oldNode = oldNodes[i];
                if (oldNode.occupied)
                {
                    // We had an element in our previous nodes here
                    Insert(oldNode.key, oldNode.value);

                    oldNode.key.~Key();
                    oldNode.value.~Value();
                }
            }

            // Free our old data
            m_allocator->Free(oldNodes);
        }

        void BackwardsShift(u64 index)
        {
            for (;;)
            {
                auto& current = m_nodes[index];

                // Delete the current element by marking it as unoccupied and calling the destructors for Key and Value
                current.occupied = false;
                current.key.~Key();
                current.value.~Value();
                // Get the next index
                u64 nextIndex = Mod(index + 1);
                // Get the slot associated with that index
                auto& next = m_nodes[nextIndex];
                // If this slot is empty we are done
                if (!next.occupied) return;
                // Calculate the desired index for the next slot
                u64 nextDesiredIndex = IndexFor(next.key);
                if (nextIndex == nextDesiredIndex)
                {
                    // Since the next slot is already at it's desired index. By definition there can be no following nodes that we need to
                    // shift backwards since any following nodes would have taken this slots place during insert.
                    return;
                }
                // Shift the next slot into our current slot by copying key and value
                current.key      = next.key;
                current.value    = next.value;
                current.occupied = true;
                // Move on to the next slot
                index = nextIndex;
            }
        }

        void Copy(const HashMap& other)
        {
            if (m_capacity > 0 && m_nodes != nullptr)
            {
                // We already have some data which we must first destroy
                Destroy();
            }

            m_allocator = other.m_allocator;

            if (other.m_capacity > 0 && other.m_nodes)
            {
                // The other HashMap has actual data which we need to copy
                // By first creating an empty HashMap
                Create(other.m_capacity);
                // And then iterating over the other HashMap and copying it's nodes
                for (u64 i = 0; i < other.m_capacity; i++)
                {
                    // Check if the node in other is occupied
                    if (other.m_nodes[i].occupied)
                    {
                        // If it is we copy over the values
                        m_nodes[i].occupied = true;
                        m_nodes[i].key      = other.m_nodes[i].key;
                        m_nodes[i].value    = other.m_nodes[i].value;
                    }
                }
            }
        }

        /**
         * @brief Returns the index associated with the provided key
         *
         * @param key The key you want to know the index for
         * @return u64 The index
         */
        u64 IndexFor(const Key& key) const { return Mod(HashFunc()(key)); }

        /**
         * @brief Takes a more efficient mod of the provided index.
         *
         * The capacity of this HashMap is always a power of 2 which means that we can use & (m_capacity - 1) instead of modulo (%)
         * to get an index into our array which is quite a bit faster. Since if m_capacity == 32 -> 100000 subtract 1 -> 011111
         * and if we AND (&) with this we mask out all higher bits essentially arriving at the modulo operator again
         *
         * @param index The index you want to take the Mod of
         * @return u64 The modded index
         */
        u64 Mod(u64 index) const { return index & (m_capacity - 1); }

        /** @brief The underlying array of nodes. */
        Node* m_nodes = nullptr;

        /** @brief The total number of nodes in this HashMap. */
        u64 m_capacity = 0;
        /** @brief The number of items stored in this HashMap. */
        u64 m_count = 0;
        /** @brief The hash function used to compute the index into the array of nodes. */
        HashFunc m_hashCompute;
        /** @brief A pointer to the allocator to be used by this HashMap. */
        Allocator* m_allocator = nullptr;
    };
}  // namespace C3D
