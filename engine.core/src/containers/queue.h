
#pragma once
#include "const_iterator.h"
#include "defines.h"
#include "iterator.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    /**
     * @brief Implementation of a Queue. Items are pushed to the back of the queue.
     * When you pop/peek items you get them back in the order you pushed them.
     *
     * @tparam Type
     * @tparam Allocator
     */
    template <class Type, class Allocator = DynamicAllocator>
    class C3D_API Queue
    {
        static_assert(std::is_base_of_v<BaseAllocator<Allocator>, Allocator>, "Allocator must derive from BaseAllocator");

        constexpr static auto DEFAULT_CAPACITY = 4;
        constexpr static auto RESIZE_FACTOR    = 1.5f;

    public:
        using value_type      = Type;
        using reference       = value_type&;
        using difference_type = ptrdiff_t;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = Iterator<Type>;
        using const_iterator  = ConstIterator<Type>;

        Queue(Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) {}

        Queue(const Queue& other) { Copy(other); }

        Queue(Queue&& other) noexcept { Move(std::move(other)); }

        Queue& operator=(const Queue& other)
        {
            Copy(other);
            return *this;
        }

        Queue& operator=(Queue&& other) noexcept
        {
            Move(std::move(other));
            return *this;
        }

        /**
         * @brief Creates the array with enough memory allocated for the provided initial capacity.
         * No initialization is done on the internal memory.
         */
        Queue(const u64 initialCapacity, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) { Reserve(initialCapacity); }

        ~Queue() { Destroy(); }

        /**
         * @brief Reserves enough memory for the provided initial capacity.
         * The stack will still have the original size and no elements will be created or added
         */
        void Reserve(u64 initialCapacity = DEFAULT_CAPACITY)
        {
            if (m_capacity >= initialCapacity)
            {
                // Reserve is not needed since our capacity is already as large or larger
                return;
            }

            // We allocate enough memory for the new capacity
            pointer newElements = m_allocator->template Allocate<Type>(MemoryType::Queue, initialCapacity);

            if (m_elements)
            {
                // We already have a pointer
                if (m_count != 0)
                {
                    // We have elements in our array already which we need to copy over
                    // Since T can have an arbitrarily complex copy constructor we need to copy all elements over
                    std::copy_n(m_elements, m_capacity, newElements);
                }

                // We delete our old memory
                m_allocator->Free(m_elements);
            }

            // We set our new capacity
            m_capacity = initialCapacity;
            // We set our elements pointer to the newly allocated memory
            m_elements = newElements;
        }

        /**
         * @brief Resizes the array internally to have capacity == size
         * This operation causes a reallocation (and thus copying of elements) if capacity != size
         */
        void ShrinkToFit()
        {
            if (m_count == m_capacity)
            {
                // No need to shrink since the size and capacity already match
                return;
            }

            if (m_count == 0)
            {
                // We have no elements so we can simply free our existing memory and stop
                Free();
                return;
            }

            // Allocate exactly enough space for our current elements
            pointer newElements = m_allocator->template Allocate<Type>(MemoryType::Queue, m_count);
            // Copy over the elements from our already allocated memory
            // Keeping in mind that elements start at head and end at tail but these may be at arbitrary positions
            for (u64 i = m_head, newElementsIndex = 0; i < m_head + m_count; ++i, ++newElementsIndex)
            {
                newElements[newElementsIndex] = m_elements[i % m_capacity];
            }

            // Free our old memory
            m_allocator->Free(m_elements);
            // Our element pointer can now be copied over
            m_elements = newElements;
            // Our capacity is now equal to our count (count remains unchanged)
            m_capacity = m_count;
        }

        /** @brief Destroys the underlying memory allocated by this queue. */
        void Destroy()
        {
            // Call destructor for all elements
            std::destroy_n(m_elements, m_capacity);
            // Free the underlying memory
            Free();
        }

        /**
         * @brief Adds the provided element to the back of the queue.
         * This will cause a resize if size + 1 >= capacity.
         */
        void Enqueue(const Type& element)
        {
            if (!m_elements || m_count >= m_capacity)
            {
                // If we have not initialized our element array or if
                // we have reached our capacity we need to resize
                GrowthFactorReAlloc();
            }

            // Increment the tail
            m_tail++;
            // Ensure that our tail wraps around to the front if we reach our capacity
            m_tail %= m_capacity;
            // Copy the element into our elements array
            m_elements[m_tail] = element;
            // Increment the amount of elements in our array
            m_count++;
        }

        template <class... Args>
        Type& Enqueue(Args&&... args)
        {
            if (!m_elements || m_count >= m_capacity)
            {
                // If we have not initialized our element array or if
                // we have reached our capacity we need to resize
                GrowthFactorReAlloc();
            }

            // Increment the tail
            m_tail++;
            // Ensure that our tail wraps around to the front if we reach our capacity
            m_tail %= m_capacity;
            // Construct element T (in-place) with the provided arguments
            new (&m_elements[m_tail]) Type(std::forward<Args>(args)...);
            // Increment the amount of elements in our array
            m_count++;
            // Return a reference to the newly added element
            return m_elements[m_tail];
        }

        /**
         * @brief Gets and removes the first element in the queue.
         */
        Type Pop()
        {
            // Decrease our size by one since we are removing an element
            m_count--;
            // Make a copy of the element that should be returned
            auto element = m_elements[m_head];
            // Destruct the element in our array since we've copied it now
            m_elements[m_head].~Type();
            // Increment the head
            m_head++;
            // Ensure that our head wraps around to the front if we reach the end
            m_head %= m_capacity;
            // Return the element
            return element;
        }

        /**
         * @brief Gets a reference to the first element in the queue.
         */
        Type& PeekHead() { return m_elements[m_head]; }

        /**
         * @brief Gets a reference to the last element in the queue.
         */
        Type& PeekTail() { return m_elements[m_tail]; }

        /**
         * @brief Gets a const-reference to the first element in the queue.
         */
        const Type& PeekHead() const { return m_elements[m_head]; }

        /**
         * @brief Gets a const-reference to the last element in the queue.
         */
        const Type& PeekTail() const { return m_elements[m_tail]; }

        /**
         * @brief Clears all elements in the stack (calling the destructor for every element)
         * Does not delete the memory and the capacity will remain the same.
         */
        void Clear() noexcept
        {
            // Destroy all the elements
            std::destroy_n(m_elements, m_capacity);
            // We set our count to 0, reset our head and tail
            m_count = 0;
            m_head  = 0;
            m_tail  = -1;
            // But the capacity remains the same since we don't free
        }

        /**
         * @brief Set a pointer to an allocator (must have a base type of BaseAllocator)
         * This method must be used if the allocator could not be set during initialization
         * because the global memory system was not yet setup for example.
         * This method can also be used if the user want's to change the underlying allocator used for this stack.
         * Keep in mind that this allocator needs to be of the same type as the initial allocator to avoid issues.
         */
        void SetAllocator(Allocator* allocator) { m_allocator = allocator; }

        [[nodiscard]] pointer GetData() const { return m_elements; }

        [[nodiscard]] u64 Count() const noexcept { return m_count; }

        [[nodiscard]] bool Empty() const noexcept { return m_count == 0; }

        [[nodiscard]] u64 Capacity() const noexcept { return m_capacity; }

    private:
        /**
         * @brief Copies over the elements from the provided Queue.
         * This is a destructive operation that will first delete all the memory in the
         * stack if there is any and resize the stack to the capacity and size of the provided stack.
         */
        void Copy(const Queue<Type, Allocator>& other)
        {
            // If we have any memory allocated we have to free it first
            Free();

            // We copy the allocator pointer from the other array so we use the correct one
            m_allocator = other.m_allocator;

            if (other.m_elements && other.m_capacity > 0)
            {
                // We allocate enough memory for the provided count
                m_elements = m_allocator->template Allocate<Type>(MemoryType::Queue, other.m_capacity);
                // Copy over the elements from other
                std::copy_n(other.m_elements, other.m_capacity, m_elements);
            }

            m_count    = other.m_count;
            m_capacity = other.m_capacity;
            m_head     = other.m_head;
            m_tail     = other.m_tail;
        }

        /**
         * @brief Internal method to move other Queue into this one
         */
        void Move(Queue&& other) noexcept
        {
            // Take the allocator from other
            m_allocator = other.m_allocator;

            // Take the members of other
            m_elements = other.m_elements;
            m_capacity = other.m_capacity;
            m_count    = other.m_count;
            m_head     = other.m_head;
            m_tail     = other.m_tail;

            // Null out "other" to ensure we don't double free
            other.m_elements = nullptr;
            other.m_capacity = 0;
            other.m_count    = 0;
            other.m_head     = 0;
            other.m_tail     = -1;
        }

        /**
         * @brief ReAlloc our dynamic array to a greater size by our growth factor
         * If our capacity is currently 0 this will ReAlloc to our default capacity
         */
        void GrowthFactorReAlloc()
        {
            // Increase our capacity by the resize factor
            auto newCapacity = static_cast<u64>(static_cast<f32>(m_capacity) * RESIZE_FACTOR);
            if (newCapacity == 0) newCapacity = DEFAULT_CAPACITY;

            ReAlloc(newCapacity);
        }

        /**
         * @brief ReAlloc our dynamic array to a greater size by our growth factor
         * This method will always make the array grow at least as large as minCapacity
         * If our capacity is currently 0 this will ReAlloc to our default capacity
         */
        void GrowthFactorReAlloc(const u64 minCapacity)
        {
            // Set our initial new capacity to our current capacity (or default capacity if current capacity is 0)
            auto newCapacity = m_capacity == 0 ? DEFAULT_CAPACITY : m_capacity;
            // Keep growing our capacity until it is at least as large as minCapacity
            while (minCapacity > newCapacity)
            {
                newCapacity = static_cast<u64>(static_cast<f32>(newCapacity) * RESIZE_FACTOR);
            }
            // Actually ReAlloc our array
            ReAlloc(newCapacity);
        }

        void ReAlloc(u64 capacity)
        {
            // Allocate memory for the new capacity
            auto newElements = m_allocator->template Allocate<Type>(MemoryType::Queue, capacity);
            // Keep a copy of the count since we might set m_count to 0 in free()
            auto newCount = m_count;

            // If there exists an element pointer and we actually have some elements
            if (m_elements && m_count > 0)
            {
                // Copy over old elements to the newly allocated block of memory
                // Note: we may not use std::memcpy here since T could have an arbitrarily complex copy constructor
                std::copy_n(m_elements, m_capacity, newElements);

                // Free the old memory
                Free();
            }

            // Set our new element pointer, capacity and count
            m_elements = newElements;
            m_capacity = capacity;
            m_count    = newCount;
        }

        void Free()
        {
            if (m_elements && m_capacity != 0)
            {
                m_allocator->Free(m_elements);
                m_elements = nullptr;
                m_capacity = 0;
                m_count    = 0;
            }
        }

        /** @brief An array of elements in this RingQueue. */
        Type* m_elements = nullptr;
        /** @brief The amount of elements that we have space allocated for in this Queue. */
        u64 m_capacity = 0;
        /** @brief The amount of elements currently contained in this Queue. */
        u64 m_count = 0;
        /** @brief The index into the list where the head is currently. */
        u64 m_head = 0;
        /** @brief The index into the list where the tail is currently. */
        u64 m_tail = INVALID_ID_U64;
        /** @brief A pointer to the allocator used by this Queue. */
        Allocator* m_allocator = nullptr;
    };
}  // namespace C3D