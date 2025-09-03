
#pragma once
#include "const_iterator.h"
#include "defines.h"
#include "iterator.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    template <class T, class Allocator = DynamicAllocator>
    class C3D_API DynamicArray
    {
        static_assert(std::is_base_of_v<BaseAllocator<Allocator>, Allocator>, "Allocator must derive from BaseAllocator");

    public:
        constexpr static auto default_capacity = 4;
        constexpr static auto resize_factor    = 1.5f;

        using value_type      = T;
        using reference       = value_type&;
        using difference_type = ptrdiff_t;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = Iterator<T>;
        using const_iterator  = ConstIterator<T>;

        DynamicArray(const Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator) {}

        DynamicArray(const DynamicArray& other) { Copy(other); }

        DynamicArray(DynamicArray&& other) noexcept
            : m_capacity(other.Capacity()), m_size(other.Size()), m_elements(other.GetData()), m_allocator(other.m_allocator)
        {
            other.m_capacity = 0;
            // Take all data from other and null out other so it does not call Free() twice on the sa
            other.m_size     = 0;
            other.m_elements = nullptr;
        }

        DynamicArray& operator=(const DynamicArray& other)
        {
            Copy(other);
            return *this;
        }

        DynamicArray& operator=(DynamicArray&& other) noexcept
        {
            Free();

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

        template <u64 Count>
        DynamicArray& operator=(const T (&elements)[Count])
        {
            Copy(elements, Count);
            return *this;
        }

        /**
         * @brief Creates the array with initialSize default constructed elements
         */
        explicit DynamicArray(const u64 initialSize, const Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            Resize(initialSize);
        }

        DynamicArray(std::initializer_list<T> list, const Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            Copy(list.begin(), list.size());
        }

        DynamicArray(const T* elements, const u64 count, const Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            Copy(elements, count);
        }

        ~DynamicArray() { Free(); }

        /**
         * @brief Reserves enough memory for the provided initial capacity.
         * The array will still have the original size and no elements will be created or added
         */
        void Reserve(u64 initialCapacity = default_capacity)
        {
            if (m_capacity >= initialCapacity)
            {
                // Reserve is not needed since our capacity is already as large or larger
                // Logger::Trace("[DYNAMIC_ARRAY] - Reserve() - Was called with initialCapacity <= currentCapacity.
                // Doing nothing");
                return;
            }

            // We allocate enough memory for the new capacity
            T* newElements = m_allocator->template Allocate<T>(MemoryType::DynamicArray, initialCapacity);

            if (m_elements)
            {
                // We already have a pointer
                if (m_size != 0)
                {
                    // We have elements in our array already which we need to copy over
                    // Since T can have an arbitrarily complex copy constructor we need to copy all elements over
                    std::copy_n(begin(), m_size, iterator(newElements));
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
         * @brief Resizes the array to at least the requested size.
         * There are 3 scenarios:
         * - requested_size == current_size: Nothing will change
         * - requested_size > current_size: The array will reallocate memory and default construct elements in all newly created empty slots up to
         * requested_size - 1
         * - requested_size < current_size: The elements after requested_size will be destroyed
         *
         * In all cases after calling this method the size will equal the requested size
         */
        void Resize(const u64 size)
        {
            // No need to do anything
            if (size == m_size) return;

            if (m_size < size)
            {
                // We need to grow
                Reserve(size);
                // All new empty slots (from m_size onwards) up to the new size should be filled with default elements
                const auto extraCount = size - m_size;
                std::uninitialized_default_construct_n(begin() + m_size, extraCount);
            }
            else
            {
                // We need to shrink so we destroy elements are size
                std::destroy_n(begin() + size, m_size - size);
            }

            // Finally our size is now equal to the requested size
            m_size = size;
        }

        /**
         * @brief Resizes the array to have enough memory for the requested size
         * The array will fill all newly created empty slots up to size - 1 with the provided value.
         */
        void ResizeAndFill(const u64 size, T value)
        {
            // Reserve enough capacity
            Reserve(size);
            // All new empty slots (from m_size onwards) up to the new size should be filled with default elements
            const auto extraCount = size - m_size;
            std::memset(m_elements + m_size, value, extraCount * sizeof(T));
            // Since we default constructed all elements up-to provided size we now also have size elements
            m_size = size;
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
            T* newElements = m_allocator->template Allocate<T>(MemoryType::DynamicArray, m_size);
            // Copy over the elements from our already allocated memory
            std::copy_n(begin(), m_size, iterator(newElements));
            // Free our old memory
            m_allocator->Free(m_elements);
            // Our element pointer can now be copied over
            m_elements = newElements;
            // Our capacity is now equal to our size (size remains unchanged)
            m_capacity = m_size;
        }

        /** @brief Destroys the underlying memory allocated by this dynamic array. */
        void Destroy() { Free(); }

        /** @brief Gets a reference to the element at the provided index. */
        T& operator[](u64 index) { return m_elements[index]; }

        const T& operator[](u64 index) const { return m_elements[index]; }

        /**
         * @brief Gets a reference to the element at the provided index.
         * Includes an out-of-range check
         */
        T& At(u64 index)
        {
            if (index >= m_size)
            {
                throw std::out_of_range("Index >= Size()");
            }
            return m_elements[index];
        }

        /**
         * @brief Gets a const reference to the element at the provided index.
         * Includes an out-of-range check
         */
        [[nodiscard]] const T& At(u64 index) const
        {
            if (index >= m_size) throw std::out_of_range("Index >= Size()");
            return m_elements[index];
        }

        /** @brief Returns a reference to the first element in the array. */
        T& Front() { return m_elements[0]; }

        /** @brief Returns a const reference to the first element in the array. */
        [[nodiscard]] const T& Front() const { return m_elements[0]; }

        /** @brief Returns a reference to the last element in the array. */
        T& Back() { return m_elements[m_size - 1]; }

        /** @brief Removes and then returns the last element from the array. */
        T PopBack()
        {
            auto element = m_elements[m_size - 1];
            Erase(m_size - 1);
            return element;
        }

        /** @brief Returns a const reference to the last element in the array. */
        [[nodiscard]] const T& Back() const { return m_elements[m_size - 1]; }

        /**
         * @brief Adds the provided element to the back of the array.
         * This will cause a resize if size + 1 >= capacity.
         */
        void PushBack(const T& element)
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
         * @brief Adds an element with the provided arguments in-place (so without copies)
         * This will cause a resize if size + 1 >= capacity
         */
        template <class... Args>
        T& EmplaceBack(Args&&... args)
        {
            if (!m_elements || m_size >= m_capacity)
            {
                // If we have not initialized our element array or if
                // we have reached our capacity we need to resize
                GrowthFactorReAlloc();
            }

            // Construct element T (in-place) with the provided arguments
            new (&m_elements[m_size]) T(std::forward<Args>(args)...);
            // Return a reference to the newly added element and then increment size
            return m_elements[m_size++];
        }

        /**
         * @brief Adds an element at the provided iterator
         * This will cause a resize if size + 1 >= capacity
         */
        void Insert(const_iterator iterator, const T& element)
        {
            // Get the index at which we want to insert
            // We have to do this before resizing because that will invalidate our iterator
            const auto index = iterator - begin();

            if (!m_elements || m_size >= m_capacity)
            {
                // If we have not initialized our element array or if
                // we have reached our capacity we need to resize
                GrowthFactorReAlloc();
            }

            // Move all elements from index to end(), one spot to the right
            std::move_backward(begin() + index, end(), end() + 1);
            // Finally insert the element at index
            m_elements[index] = element;
        }

        /**
         * @brief Adds a range of elements at the provided iterator
         * This will cause a resize if size + range.Size() >= capacity
         */
        void Insert(const_iterator it, iterator first, iterator last)
        {
            // Get the index at which we want to insert
            // We have to do this before resizing because that will invalidate our iterator
            const auto index = it - begin();
            // Get the size of our range
            const auto rangeSize = last - first;
            // Get the minimum size we need for our array with the range inserted
            const auto newMinSize = m_size + rangeSize;
            // Grow our array if it is not yet initialized or if it's too small
            if (!m_elements || newMinSize > m_capacity)
            {
                GrowthFactorReAlloc(m_capacity + rangeSize);
            }

            // Move all elements from index to end(), one spot to the right
            std::move_backward(begin() + index, end(), end() + rangeSize);
            // Finally insert the range at index
            std::move(first, last, begin() + index);
            // Update our size
            m_size += rangeSize;
        }

        /**
         * @brief Adds a range of elements at the provided iterator
         * This will cause a resize if size + range.Size() >= capacity
         */
        void Insert(const_iterator it, const_iterator first, const_iterator last)
        {
            // Get the index at which we want to insert
            // We have to do this before resizing because that will invalidate our iterator
            const auto index = it - begin();
            // Get the size of our range
            const auto rangeSize = last - first;
            // Get the minimum size we need for our array with the range inserted
            const auto newMinSize = m_size + rangeSize;
            // Grow our array if it is not yet initialized or if it's too small
            if (!m_elements || newMinSize > m_capacity)
            {
                GrowthFactorReAlloc(m_capacity + rangeSize);
            }

            // Move all elements from index to end(), one spot to the right
            std::move_backward(begin() + index, end(), end() + rangeSize);
            // Finally insert the range at index
            std::move(first, last, begin() + index);
            // Update our size
            m_size += rangeSize;
        }

        /**
         * @brief Emplaces an element at the provided iterator
         * This will cause a resize if size + 1 >= capacity
         */
        template <class... Args>
        void Emplace(const_iterator iterator, Args&&... args)
        {
            // Get the index at which we want to insert
            // We have to do this before resizing because that will invalidate our iterator
            const auto index = iterator - begin();

            if (!m_elements || m_size >= m_capacity)
            {
                // If we have not initialized our element array or if
                // we have reached our capacity we need to resize
                GrowthFactorReAlloc();
            }

            // Move all elements from index to end(), one spot to the right
            std::move_backward(begin() + index, end(), end() + 1);
            // Finally insert the element at index
            new (&m_elements[index]) T(std::forward<Args>(args)...);
        }

        /**
         * @brief Copies over the elements at the provided pointer
         * This is a destructive operation that will first delete all the memory in the
         * dynamic array if there is any. This will resize the dynamic array to count.
         */
        void Copy(const T* elements, u64 count)
        {
            // If we have any memory allocated we have to free it first
            Free();

            if (count > 0)
            {
                // We allocate enough memory for the provided count
                m_elements = m_allocator->template Allocate<T>(MemoryType::DynamicArray, count);
                m_capacity = count;

                if (elements)
                {
                    // Then we copy over the elements from the provided pointer into our newly allocated memory
                    std::copy_n(elements, count, m_elements);
                    m_size = count;
                }
            }
        }

        /**
         * @brief Copies over the elements from the provided DynamicArray
         * This is a destructive operation that will first delete all the memory in the
         * dynamic array if there is any and resize the dynamic array to the capacity and size of the provided array
         */
        void Copy(const DynamicArray& other)
        {
            // If we have any memory allocated we have to free it first
            Free();

            // We copy the allocator pointer from the other array so we use the correct one
            m_allocator = other.m_allocator;

            if (other.m_size > 0)
            {
                // We allocate enough memory for the provided count
                m_elements = m_allocator->template Allocate<T>(MemoryType::DynamicArray, other.m_size);
                // Then we copy over the elements from the provided pointer into our newly allocated memory
                std::copy_n(other.begin(), other.m_size, begin());
            }

            m_size     = other.m_size;
            m_capacity = other.m_size;
        }

        /**
         * @brief Checks if the array contains the provided element.
         *
         * @param element The element you want to search for.
         * @return True if the array contains the element, false otherwise
         */
        bool Contains(const T& element) { return std::find(begin(), end(), element) != end(); }

        /**
         * @brief Returns the first element of the array. Will throw if the array is empty.
         *
         * @return A reference to the first element in the array
         */
        T& First() { return m_elements[0]; }

        /**
         * @brief Returns the first element of the array. Will throw if the array is empty.
         *
         * @return A const reference to the first element in the array
         */
        const T& First() const { return m_elements[0]; }

        /**
         * @brief Returns the last element of the array. Will throw if the array is empty.
         *
         * @return A reference to the last element in the array
         */
        T& Last() { return m_elements[m_size - 1]; }

        /**
         * @brief Returns the last element of the array. Will throw if the array is empty.
         *
         * @return A const reference to the last element in the array
         */
        const T& Last() const { return m_elements[m_size - 1]; }

        /**
         * @brief Clears all elements in the array (calling the destructor for every element)
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
         * @brief Resets the dynamic array to an initial state. Keep in mind that this does not free any memory.
         * So if you call this method and the memory it's using is not somehow freed elsewhere you will leak.
         */
        void Reset()
        {
            m_elements = nullptr;
            m_size     = 0;
            m_capacity = 0;
        }

        /**
         * @brief Removes the element at the provided iterator from the array.
         * If you remove an element all elements to the right of it will be copied over
         * and moved to the left one spot effectively shrinking the size by 1 but keeping the same capacity.
         */
        void Erase(iterator iterator)
        {
            // First we get an index into the array for the provided iterator
            const auto index = iterator - begin();
            // Then we destruct this element
            m_elements[index].~T();

            // We need to move all elements after the erased element one spot to the left
            // in order for the dynamic array to be contiguous again
            // We start moving from 1 element to the right of our erased element
            const auto moveStart = begin() + index + 1;
            // We move them one spot to the left (starting at our erased element)
            const auto dest = begin() + index;
            std::move(moveStart, end(), dest);

            // Decrease the size by one for the removed element
            m_size--;
        }

        /**
         * @brief Removes the element at the provided index from the array.
         * If you remove an element all elements to the right of it will be copied over
         * and moved to the left one spot effectively shrinking the size by 1 but keeping the same capacity.
         */
        void Erase(u64 index)
        {
            // Deconstruct the element at the provided index
            m_elements[index].~T();

            // We need to move all elements after the erased element one spot to the left
            // in order for the dynamic array to be contiguous again
            // We start moving from 1 element to the right of our erased element
            const auto moveStart = begin() + index + 1;
            // We move them one spot to the left (starting at our erased element)
            const auto dest = begin() + index;
            std::move(moveStart, end(), dest);

            // Decrease the size by one for the removed element
            m_size--;
        }

        /** @brief Removes the first matching element with item from the dynamic array.
         * If you want to remove all matching items in the array use RemoveAll() instead.
         *
         * @return True if item was found (and removed), false otherwise
         */
        bool Remove(const T& item)
        {
            for (u64 i = 0; i < m_size; i++)
            {
                if (m_elements[i] == item)
                {
                    // Deconstruct the found element
                    m_elements[i].~T();
                    // We need to move all elements after the erased element one spot to the left
                    // in order for the dynamic array to be contiguous again
                    // We start moving from 1 element to the right of our erased element
                    const auto moveStart = begin() + i + 1;
                    // We move them one spot to the left (starting at our erased element)
                    const auto dest = begin() + i;
                    std::move(moveStart, end(), dest);

                    // Decrease the size by one for the removed element
                    m_size--;
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Set a pointer to an allocator (must have a base type of BaseAllocator)
         * This method must be used if the allocator could not be set during initialization
         * because the global memory system was not yet setup for example.
         * This method can also be used if the user want's to change the underlying allocator used for this dynamic
         * array. Keep in mind that this allocator needs to be of the same type as the initial allocator. Also keep in
         * mind that this is a destructive operation since it will clear and free anything inside the array.
         */
        void SetAllocator(const Allocator* allocator)
        {
            // Free any memory that is currently in our array since we will swap allocator
            Free();
            m_allocator = allocator;
        }

        [[nodiscard]] T* GetData() const { return m_elements; }

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
        /**
         * @brief ReAlloc our dynamic array to a greater size by our growth factor
         * If our capacity is currently 0 this will ReAlloc to our default capacity
         */
        void GrowthFactorReAlloc()
        {
            // Increase our capacity by the resize factor
            auto newCapacity = static_cast<u64>(static_cast<f32>(m_capacity) * resize_factor);
            // Ensure our new size > 1
            if (newCapacity <= 1) newCapacity = default_capacity;

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
            auto newCapacity = m_capacity == 0 ? default_capacity : m_capacity;
            // Keep growing our capacity until it is at least as large as minCapacity
            while (minCapacity > newCapacity)
            {
                newCapacity = static_cast<u64>(static_cast<f32>(newCapacity) * resize_factor);
            }
            // Actually ReAlloc our array
            ReAlloc(newCapacity);
        }

        void ReAlloc(u64 capacity)
        {
            // Allocate memory for the new capacity
            T* newElements = m_allocator->template Allocate<T>(MemoryType::DynamicArray, capacity);
            u64 newSize    = 0;

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
                // Call destructor for every element
                std::destroy_n(m_elements, m_size);
                // Free the memory
                m_allocator->Free(m_elements);
                // Reset everything to initial values
                m_elements = nullptr;
                m_capacity = 0;
                m_size     = 0;
            }
        }

        u64 m_capacity = 0;
        u64 m_size     = 0;

        T* m_elements                = nullptr;
        const Allocator* m_allocator = nullptr;
    };
}  // namespace C3D
