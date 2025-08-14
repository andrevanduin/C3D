
#pragma once
#include "defines.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    template <class Type, u64 CCapacity>
    class RingQueue
    {
    public:
        RingQueue() = default;

        RingQueue(const RingQueue& other) { Copy(other); }

        RingQueue& operator=(const RingQueue& other)
        {
            Copy(other);
            return *this;
        }

        RingQueue(RingQueue&& other) { Move(std::move(other)); }

        RingQueue& operator=(RingQueue&& other)
        {
            Move(std::move(other));
            return *this;
        }

        ~RingQueue() { Clear(); }

        void Clear()
        {
            // Destroy all created elements
            // Keeping in mind that elements start at head and end at tail but these may be at arbitrary positions
            for (u64 i = m_head; i < m_head + m_count; ++i)
            {
                m_elements[i % CCapacity].~Type();
            }

            // Reset count, head and tail
            m_count = 0;
            m_head  = 0;
            m_tail  = -1;
        }

        void Enqueue(const Type& element)
        {
            C3D_ASSERT_DEBUG_MSG(m_count < CCapacity, "Queue is full.");

            // Increment the tail
            m_tail++;
            // Ensure that our tail wraps around to the front if we reach our capacity
            m_tail %= CCapacity;
            // Add the element to our array
            m_elements[m_tail] = element;
            // Increment the amount of elements in our array
            m_count++;
        }

        template <class... Args>
        Type& Enqueue(Args&&... args)
        {
            C3D_ASSERT_DEBUG_MSG(m_count < CCapacity, "Queue is full.");

            // Increment the tail
            m_tail++;
            // Ensure that our tail wraps around to the front if we reach our capacity
            m_tail %= CCapacity;
            // Construct element T (in-place) with the provided arguments
            new (&m_elements[m_tail]) Type(std::forward<Args>(args)...);
            // Increment the amount of elements in our array
            m_count++;

            // Return a reference to the newly added element
            return m_elements[m_tail];
        }

        Type Dequeue()
        {
            C3D_ASSERT_DEBUG_MSG(m_count > 0, "Can't dequeue an item when the queue is empty.")

            // Get the element at the head
            auto element = m_elements[m_head];
            // Increment the head
            m_head++;
            // Ensure that our head wraps around to the front if we reach our capacity
            m_head %= CCapacity;
            // Decrease the size of our elem
            m_count--;
            // Return our element
            return element;
        }

        Type Pop() { return Dequeue(); }

        const Type& Peek() const
        {
            C3D_ASSERT_DEBUG_MSG(m_count > 0, "Can't peek an item when the queue is empty.");

            // Return a reference to the element at the head
            return m_elements[m_head];
        }

        Type* GetData() noexcept { return m_elements; }
        const Type* GetData() const noexcept { return m_elements; }

        u64 Count() const noexcept { return m_count; }
        constexpr u64 Capacity() const noexcept { return CCapacity; }

        bool Empty() const noexcept { return m_count == 0; }

    private:
        void Copy(const RingQueue& other)
        {
            // Copy count, head and tail pointer
            m_count = other.m_count;
            m_head  = other.m_head;
            m_tail  = other.m_tail;

            // Copy over the elements from other
            std::copy_n(other.m_elements, CCapacity, m_elements);
        }

        void Move(RingQueue&& other)
        {
            // Copy count, head and tail pointer
            m_count = other.m_count;
            m_head  = other.m_head;
            m_tail  = other.m_tail;

            // Move over the elements from other
            // Keeping in mind that elements start at head and end at tail but these may be at arbitrary positions
            for (u64 i = m_head; i < m_head + m_count; ++i)
            {
                m_elements[i % CCapacity] = std::move(other.m_elements[i % CCapacity]);
            }

            // Ensure the other RingQueue doesn't try to call the destructors on it's elements again
            other.m_count = 0;
            other.m_head  = 0;
            other.m_tail  = -1;
        }

        /** @brief An array of elements in this RingQueue. */
        Type m_elements[CCapacity];
        /** @brief The amount of elements currently contained in this RingQueue. */
        u64 m_count = 0;
        /** @brief The index into the list where the head is currently. */
        u64 m_head = 0;
        /** @brief The index into the list where the tail is currently. */
        u64 m_tail = INVALID_ID_U64;
    };
}  // namespace C3D
