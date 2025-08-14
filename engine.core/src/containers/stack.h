
#pragma once
#include "const_iterator.h"
#include "defines.h"
#include "iterator.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    template <class Type, class Allocator = DynamicAllocator>
    class C3D_API Stack
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

        Stack(Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) {}

        Stack(const Stack& other) { Copy(other); }

        Stack(Stack&& other) noexcept : m_capacity(other.Capacity()), m_size(other.Size()), m_elements(other.GetData()), m_allocator(other.m_allocator)
        {
            other.m_capacity = 0;
            // Take all data from other and null out other so it does not call Free() twice on the sa
            other.m_size     = 0;
            other.m_elements = nullptr;
        }

        Stack& operator=(const Stack& other)
        {
            if (this != &other)
            {
                Copy(other);
            }
            return *this;
        }

        Stack& operator=(Stack&& other) noexcept
        {
            m_elements  = other.GetData();
            m_allocator = other.m_allocator;
            m_size      = other.Size();
            m_capacity  = other.Capacity();

            // Null out "other" to ensure we don't double free
            other.m_capacity = 0;
            other.m_size     = 0;
            other.m_elements = nullptr;

            return *this;
        }

        /**
         * @brief Creates the array with enough memory allocated for the provided initial capacity.
         * No initialization is done on the internal memory.
         */
        Stack(const u64 initialCapacity, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) { Reserve(initialCapacity); }

        Stack(std::initializer_list<Type> list, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) { Copy(list.begin(), list.size(), allocator); }

        ~Stack() { Destroy(); }

        /**
         * @brief Reserves enough memory for the provided initial capacity.
         * The stack will still have the original size and no elements will be created or added
         */
        void Reserve(u64 initialCapacity = DEFAULT_CAPACITY)
        {
            if (m_capacity >= initialCapacity)
            {
                // Reserve is not needed since our capacity is already as large or larger
                // Logger::Trace("[STACK] - Reserve() - Was called with initialCapacity <= currentCapacity. Doing nothing");
                return;
            }

            // Logger::Trace("[STACK] - Reserve({})", initialCapacity);

            // We allocate enough memory for the new capacity
            pointer newElements = m_allocator->template Allocate<Type>(MemoryType::Stack, initialCapacity);
            u64 newSize         = 0;

            if (m_elements)
            {
                // We already have a pointer
                if (m_size != 0)
                {
                    // We have elements in our array already which we need to copy over
                    // Since T can have an arbitrarily complex copy constructor we need to copy all elements over
                    std::copy_n(begin(), m_size, Iterator(newElements));
                    // Our new size will be equal to the size of whatever we had before the Reserve() call
                    newSize = m_size;
                }

                // We delete our old memory
                m_allocator->Free(m_elements);
            }

            // We set our new capacity
            m_capacity = initialCapacity;
            // We copy over our size (since it might have been cleared by our Free() call
            m_size = newSize;
            // We set our elements pointer to the newly allocated memory
            m_elements = newElements;
        }

        /**
         * @brief Resizes the array to have enough memory for the requested size
         * The array will default construct elements in all newly created empty slots up to size - 1
         */
        void Resize(const u64 size)
        {
            // Reserve enough capacity
            Reserve(size);
            // All new empty slots up to capacity should be filled with default elements
            std::uninitialized_default_construct_n(begin(), m_capacity);
            // Since we default constructed all elements up-to capacity we now also have capacity elements
            m_size = m_capacity;
        }

        /**
         * @brief Resizes the array internally to have capacity = size
         * This operation causes a reallocation (and thus copying of elements) if capacity != size
         */
        void ShrinkToFit()
        {
            if (m_size == m_capacity)
            {
                // No need to shrink since the size and capacity already match
                return;
            }

            if (m_size == 0)
            {
                // We have no elements so we can simply free our existing memory and stop
                Free();
                return;
            }

            // Allocate exactly enough space for our current elements
            pointer newElements = m_allocator->template Allocate<Type>(MemoryType::Stack, m_size);
            // Copy over the elements from our already allocated memory
            std::copy_n(begin(), m_size, Iterator(newElements));
            // Free our old memory
            m_allocator->Free(m_elements);
            // Our element pointer can now be copied over
            m_elements = newElements;
            // Our capacity is now equal to our size (size remains unchanged)
            m_capacity = m_size;
        }

        /** @brief Destroys the underlying memory allocated by this dynamic array. */
        void Destroy()
        {
            // Call destructor for all elements
            for (size_t i = 0; i < m_size; i++)
            {
                m_elements[i].~Type();
            }
            Free();
        }

        /**
         * @brief Adds the provided element to the top of the stack.
         * This will cause a resize if size + 1 >= capacity.
         */
        void Push(const Type& element)
        {
            if (!m_elements || m_size >= m_capacity)
            {
                // If we have not initialized our element array or if
                // we have reached our capacity we need to resize
                GrowthFactorReAlloc();
            }

            m_elements[m_size] = element;
            m_size++;
        }

        /**
         * @brief Gets and removes the top element from the stack.
         */
        Type Pop()
        {
            // Decrease our size by one since we are removing an element
            m_size--;
            // Make a copy of the element that should be returned
            auto element = m_elements[m_size];
            // Destruct the element in our array since we've copied it now
            m_elements[m_size].~Type();
            return element;
        }

        /**
         * @brief Gets a reference to the top element of the stack.
         */
        Type& Peek() { return m_elements[m_size]; }

        /**
         * @brief Gets a const-reference to the top element of the stack.
         */
        const Type& Peek() const { return m_elements[m_size]; }

        /**
         * @brief Copies over the elements from the provided Stack
         * This is a destructive operation that will first delete all the memory in the
         * stack if there is any and resize the stack to the capacity and size of the provided stack
         */
        void Copy(const Stack<Type, Allocator>& other)
        {
            // If we have any memory allocated we have to free it first
            Free();

            // We copy the allocator pointer from the other array so we use the correct one
            m_allocator = other.m_allocator;

            if (other.m_size > 0)
            {
                // We allocate enough memory for the provided count
                m_elements = m_allocator->template Allocate<Type>(MemoryType::Stack, other.m_size);
                // Then we copy over the elements from the provided pointer into our newly allocated memory
                // Note: Again we may not use std::memcpy here since T could have an arbitrarily complex copy constructor
                for (u64 i = 0; i < other.m_size; i++)
                {
                    m_elements[i] = other[i];
                }
            }

            m_size     = other.m_size;
            m_capacity = other.m_size;
        }

        /**
         * @brief Clears all elements in the stack (calling the destructor for every element)
         * Does not delete the memory and the capacity will remain the same.
         */
        void Clear() noexcept
        {
            // Destroy all the elements
            std::destroy(begin(), end());
            // We set our size to 0 but our capacity remains the same
            m_size = 0;
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

        [[nodiscard]] u64 Size() const noexcept { return m_size; }

        [[nodiscard]] i64 SSize() const noexcept { return static_cast<i64>(m_size); }

        [[nodiscard]] bool Empty() const noexcept { return m_size == 0; }

        [[nodiscard]] u64 Capacity() const noexcept { return m_capacity; }

        [[nodiscard]] iterator begin() noexcept { return iterator(m_elements); }

        [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(m_elements); }

        [[nodiscard]] const_iterator cbegin() const noexcept { return iterator(m_elements); }

        [[nodiscard]] iterator end() noexcept { return iterator(m_elements + m_size); }

        [[nodiscard]] const_iterator end() const noexcept { return const_iterator(m_elements + m_size); }

        [[nodiscard]] const_iterator cend() const noexcept { return const_iterator(m_elements + m_size); }

    private:
        Type operator[](u64 index) const { return m_elements[index]; }

        /**
         * @brief Copies over the provided amount of elements from the provided pointer
         * This is a destructive operation that will first delete all the memory in the
         * stack if there is any and resize the stack to the capacity and size that is provided
         */
        void Copy(const Type* elements, const u64 count, Allocator* allocator)
        {
            // If we have any memory allocated we have to free it first
            Free();

            // We copy the allocator pointer from the other source so we use the correct one
            m_allocator = allocator;

            if (count > 0)
            {
                // We allocate enough memory for the provided count
                m_elements = m_allocator->template Allocate<Type>(MemoryType::Stack, count);
                // Then we copy over the elements from the provided pointer into our newly allocated memory
                // Note: Again we may not use std::memcpy here since T could have an arbitrarily complex copy constructor
                for (u64 i = 0; i < count; i++)
                {
                    m_elements[i] = elements[i];
                }
            }

            m_size     = count;
            m_capacity = count;
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
            auto newElements = m_allocator->template Allocate<Type>(MemoryType::Stack, capacity);
            u64 newSize      = 0;

            // If there exists an element pointer
            if (m_elements && m_size > 0)
            {
                // Copy over old elements to the newly allocated block of memory
                // Note: we may not use std::memcpy here since T could have an arbitrarily complex copy constructor
                std::copy_n(begin(), m_size, iterator(newElements));
                newSize = m_size;

                // Free the old memory
                Free();
            }

            // Set our new element pointer, capacity and size
            m_elements = newElements;
            m_capacity = capacity;
            m_size     = newSize;
        }

        void Free()
        {
            if (m_elements && m_capacity != 0)
            {
                m_allocator->Free(m_elements);
                m_elements = nullptr;
                m_capacity = 0;
                m_size     = 0;
            }
        }

        u64 m_capacity = 0;
        u64 m_size     = 0;

        Type* m_elements       = nullptr;
        Allocator* m_allocator = nullptr;
    };
}  // namespace C3D