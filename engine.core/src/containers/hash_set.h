
#pragma once
#include "asserts/asserts.h"
#include "defines.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    constexpr auto HASH_SET_DEFAULT_CAPACITY    = 32;
    constexpr auto HASH_SET_DEFAULT_LOAD_FACTOR = 0.75;

    /**
     * @brief Implementation of a HashSet with Open-Adressing using Robin Hood probing and backshift deletion.
     *
     * @tparam Key The Key type used to index into the HashSet
     * @tparam Value The Value type used with this HashSet
     * @tparam HashFunc The HashFunction used to hash Keys before inserting them
     * @tparam LF Determines when the HashSet needs to grow in size (0.0 < LoadFactor <= 1.0, where a LoadFactor of 0.5 means that
     * when the HashSet is 50% full we grow the HashSet)
     * @tparam Allocator The allocator used by this HashSet
     */
    template <class Value, class HashFunc = std::hash<Value>, double LF = HASH_SET_DEFAULT_LOAD_FACTOR, class Allocator = DynamicAllocator>
    class HashSet
    {
        static_assert(LF > 0.0, "The Load Factor of a HashSet must be > 0.0");
        static_assert(LF <= 1.0, "The Load Factor of a HashSet must be <= 1.0");

        class HashSetIterator
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

            HashSetIterator() = default;

            explicit HashSetIterator(const HashSet* map) : m_index(INVALID_ID_U64), m_map(map) { FindNextOccupiedIndex(); }

            HashSetIterator(const HashSet* map, const u64 currentIndex) : m_index(currentIndex), m_map(map) { FindNextOccupiedIndex(); }

            // Dereference operator
            Reference operator*() const { return m_map->m_nodes[m_index].value; }

            // Pre and post-increment operators
            HashSetIterator& operator++()
            {
                FindNextOccupiedIndex();
                return *this;
            }

            HashSetIterator operator++(int) { return HashSetIterator(m_map, m_index); }

            bool operator==(const HashSetIterator& other) const { return m_map == other.m_map && m_index == other.m_index; }

            bool operator!=(const HashSetIterator& other) const { return m_map != other.m_map || m_index != other.m_index; }

        private:
            u64 m_index          = 0;
            const HashSet* m_map = nullptr;
        };

    public:
        struct Node
        {
            Value value;
            bool occupied = false;
        };

        HashSet(Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) {}

        HashSet(const HashSet& other) { Copy(other); }
        HashSet& operator=(const HashSet& other)
        {
            Copy(other);
            return *this;
        }

        HashSet(HashSet&&)            = delete;
        HashSet& operator=(HashSet&&) = delete;

        ~HashSet() { Destroy(); }

        void Create()
        {
            // Allocate memory for all the buckets
            m_nodes    = m_allocator->Allocate<Node>(MemoryType::HashSet, HASH_SET_DEFAULT_CAPACITY);
            m_capacity = HASH_SET_DEFAULT_CAPACITY;
        }

        /**
         * @brief Clears all the entries from the HashSet without destroying underlying memory
         * meaning you can keep using the HashSet as if it was just freshly created
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
                        node.value.~Value();
                        node.occupied = false;
                    }
                }
                // Reset the number of items
                m_count = 0;
            }
        }

        /**
         * @brief Destroys the HashSet. This clears the HashSet entirely and also deletes it's internal memory.
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
         * @brief Inserts the provided Value into the HashSet. If Value is already in the HashSet it gets ignored.
         *
         * @param value The value you want to insert
         */
        void Insert(Value value)
        {
            C3D_ASSERT_DEBUG_MSG(m_nodes != nullptr, "Tried Insert() before HashSet.Create() was called.");

            // Make sure that we grow our HashSet when we reach our Load Factor
            if (m_count >= m_capacity * LF)
            {
                Grow();
            }

            // Turn our value into an initial index
            u64 index = IndexFor(value);
            // Keep track of our PSL (Probe Sequence Length, the number of slots from our initial desired spot for our Value)
            u64 psl = 0;

            for (;;)
            {
                auto& current = m_nodes[index];

                // First we check if the slot is occupied
                if (!current.occupied)
                {
                    // We have found an empty slot so we copy over our value
                    current.value = std::move(value);
                    // Mark this slot as occupied
                    current.occupied = true;
                    // Increase our count since we have added an item
                    m_count++;
                    return;
                }

                // Slot is already full so let's check it's value
                if (current.value == value)
                {
                    // We already have the same value, so we do nothing
                    return;
                }

                // This slot does not match our value so we move on
                u64 desiredIndex = IndexFor(current.value);
                // Calculate the PSL for the slot we are currently checking
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);

                if (psl > currentPsl)
                {
                    // Our PSL is greater then the PSL of the slot we are currently checking, so we swap
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
         * @brief Deletes the value from the HashSet. If the value is not part of the HashSet we do nothing.
         *
         * @param value The value you want to remove
         */
        void Delete(const Value& value)
        {
            C3D_ASSERT_DEBUG_MSG(m_nodes != nullptr, "Tried Delete() before HashSet.Create() was called.");

            // Get the initial index for our value
            auto index = IndexFor(value);
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

                if (current.value == value)
                {
                    // We have found our value so we can decrease the count and do a backwards shift
                    m_count--;
                    BackwardsShift(index);
                    return;
                }

                // This slot does not contain our value so we move on
                u64 desiredIndex = IndexFor(current.value);
                // Calculate the PSL for our current slot
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);
                if (currentPsl < psl)
                {
                    // The PSL of our current slot is smaller than the PSL of the Value we are looking for
                    // This means that by definition our value is not in this HashSet since it would have swapped places on insert
                    return;
                }

                // Move on to the next slot
                psl++;
                index = Mod(index + 1);
            }
        }

        /**
         * @brief Checks if the provided Value is present in the HashSet.
         *
         * @param Value The value you want to check for
         * @return True if the value is present; false otherwise
         */
        [[nodiscard]] bool Has(const Value& value) const
        {
            // Get the initial index for our Value
            u64 index = IndexFor(value);
            // The PSL initially will be 0
            u64 psl = 0;
            for (;;)
            {
                auto& current = m_nodes[index];
                // If the slot is empty we can immediatly return false

                if (!current.occupied) return false;
                // Check if this slot contains our key
                if (current.value == value) return true;

                // Our slot does not contain our key so we move on
                u64 desiredIndex = IndexFor(current.value);
                // Calculate the PSL for our current slot
                u64 currentPsl = Mod(index + m_capacity - desiredIndex);
                if (currentPsl < psl)
                {
                    // The PSL of our current slot is smaller than the PSL of the Value we are looking for
                    // This means that by definition our value is not in this HashSet since it would have swapped places on insert
                    return false;
                }

                // Move on to the next slot
                psl++;
                index = Mod(index + 1);
            }
        }

        /**
         * @brief Checks if the provided Value is present in the HashSet.
         *
         * @param Value The value you want to check for
         * @return True if the value is present; false otherwise
         */
        [[nodiscard]] bool Contains(const Value& value) const { return Has(value); }

        [[nodiscard]] HashSetIterator begin() const { return HashSetIterator(this); }

        [[nodiscard]] HashSetIterator end() const { return HashSetIterator(this, m_capacity); }

        [[nodiscard]] u64 Capacity() const { return m_capacity; }

        [[nodiscard]] u64 Count() const { return m_count; }

        [[nodiscard]] double LoadFactor() const { return LF; }

    private:
        void Create(u64 capacity)
        {
            if (m_nodes == nullptr && m_capacity == 0)
            {
                // Allocate memory for all the nodes
                m_nodes    = m_allocator->Allocate<Node>(MemoryType::HashSet, capacity);
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
            m_nodes = m_allocator->Allocate<Node>(MemoryType::HashSet, m_capacity);
            // Iterate over all our nodes and insert them again (to rehash with our new capacity)
            for (u64 i = 0; i < oldCapacity; ++i)
            {
                auto& oldNode = oldNodes[i];
                if (oldNode.occupied)
                {
                    // We had an element in our previous nodes here
                    Insert(oldNode.value);

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

                // Delete the current element by marking it as unoccupied and calling the destructor for the Value
                current.occupied = false;
                current.value.~Value();
                // Get the next index
                u64 nextIndex = Mod(index + 1);
                // Get the slot associated with that index
                auto& next = m_nodes[nextIndex];
                // If this slot is empty we are done
                if (!next.occupied) return;
                // Calculate the desired index for the next slot
                u64 nextDesiredIndex = IndexFor(next.value);
                if (nextIndex == nextDesiredIndex)
                {
                    // Since the next slot is already at it's desired index. By definition there can be no following nodes that we need to
                    // shift backwards since any following nodes would have taken this slots place during insert.
                    return;
                }
                // Shift the next slot into our current slot by copying the value
                current.value    = next.value;
                current.occupied = true;
                // Move on to the next slot
                index = nextIndex;
            }
        }

        void Copy(const HashSet& other)
        {
            if (m_capacity > 0 && m_nodes != nullptr)
            {
                // We already have some data which we must first destroy
                Destroy();
            }

            m_allocator = other.m_allocator;

            if (other.m_capacity > 0 && other.m_nodes)
            {
                // The other HashSet has actual data which we need to copy
                // By first creating an empty HashSet
                Create(other.m_capacity);
                // And then iterating over the other HashSet and copying it's nodes
                for (u64 i = 0; i < other.m_capacity; i++)
                {
                    // Check if the node in other is occupied
                    if (other.m_nodes[i].occupied)
                    {
                        // If it is we copy over the values
                        m_nodes[i].occupied = true;
                        m_nodes[i].value    = other.m_nodes[i].value;
                    }
                }
            }
        }

        /**
         * @brief Returns the index associated with the provided value
         *
         * @param value The value you want to know the index for
         * @return u64 The index
         */
        u64 IndexFor(const Value& value) const { return Mod(HashFunc()(value)); }

        /**
         * @brief Takes a more efficient mod of the provided index.
         *
         * The capacity of this HashSet is always a power of 2 which means that we can use & (m_capacity - 1) instead of modulo (%)
         * to get an index into our array which is quite a bit faster. Since if m_capacity == 32 -> 100000 subtract 1 -> 011111
         * and if we AND (&) with this we mask out all higher bits essentially arriving at the modulo operator again
         *
         * @param index The index you want to take the Mod of
         * @return u64 The modded index
         */
        u64 Mod(u64 index) const { return index & (m_capacity - 1); }

        /** @brief The underlying array of nodes. */
        Node* m_nodes = nullptr;

        /** @brief The total number of nodes in this HashSet. */
        u64 m_capacity = 0;
        /** @brief The number of items stored in this HashSet. */
        u64 m_count = 0;
        /** @brief The hash function used to compute the index into the array of nodes. */
        HashFunc m_hashCompute;
        /** @brief A pointer to the allocator to be used by this HashSet. */
        Allocator* m_allocator = nullptr;
    };
}  // namespace C3D

template <>
struct std::hash<const char*>
{
    size_t operator()(const char* key) const noexcept
    {
        size_t hash         = 0;
        const size_t length = std::strlen(key);

        for (size_t i = 0; i < length; i++)
        {
            hash ^= static_cast<size_t>(key[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};
