
#pragma once
#include <fmt/format.h>

#include <iostream>

#include "containers/dynamic_array.h"
#include "defines.h"
#include "logger/logger.h"
#include "math/math_types.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    constexpr u8 SSO_CAPACITY  = 16;
    constexpr u8 SSO_THRESHOLD = SSO_CAPACITY - 1;
    constexpr u8 MEMORY_TYPE   = SSO_CAPACITY - 1;

    constexpr char SSO_USE_STACK = '\0';
    constexpr char SSO_USE_HEAP  = '\1';

    constexpr const char* TRUE_VALUE  = "true";
    constexpr const char* FALSE_VALUE = "false";

    // Times how much the string is increased every time a resize is required
    constexpr f64 STRING_RESIZE_FACTOR = 1.5;

    template <class Allocator>
    class C3D_API BasicString
    {
        static_assert(std::is_base_of_v<BaseAllocator<Allocator>, Allocator>, "Allocator needs to be have a base type of BaseAllocator");

    public:
        using value_type      = char;
        using reference       = value_type&;
        using difference_type = ptrdiff_t;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = Iterator<char>;
        using const_iterator  = ConstIterator<char>;

    private:
        void Init(const u64 length)
        {
            m_size = length;

            if (length > SSO_THRESHOLD)
            {
                // The length of the string is larger than our threshold for SSO so we use dynamic allocation
                const auto capacity     = length + 1;  // Extra byte for our '\0' char
                m_sso.capacity          = capacity;
                m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;

                m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, capacity);
            }
            else
            {
                // The length of the string is smaller than our threshold for SSO so we use the stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                m_data                  = m_sso.data;
            }
        }

        void Resize(const u64 requiredSize)
        {
            if (m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)
            {
                // The string is already allocated on the heap
                if (requiredSize > m_sso.capacity)
                {
                    // The new capacity is larger than the old so we need to allocate more space
                    const auto newCapacity = static_cast<u64>(std::ceil(static_cast<f64>(requiredSize) * STRING_RESIZE_FACTOR));

                    // Allocate enough space for the new data
                    const auto newData = m_allocator->template Allocate<char>(MemoryType::C3DString, newCapacity);
                    // Copy over the old data (including the final '0' byte)
                    std::memcpy(newData, m_data, m_size + 1);
                    // Now we free our current pointer
                    m_allocator->Free(m_data);
                    // And set our data pointer to the newly allocated block
                    m_data = newData;
                    // Finally we store our new capacity
                    m_sso.capacity = newCapacity;
                }
            }
            else
            {
                // The string is currently on the stack
                if (requiredSize > SSO_THRESHOLD)
                {
                    // Our new capacity is too large for SSO
                    const auto newCapacity = static_cast<u64>(std::ceil(static_cast<f64>(requiredSize) * STRING_RESIZE_FACTOR));
                    // Allocate enough space for the new capacity
                    m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, newCapacity);
                    // Copy over the old data from the stack to the heap (including the final '0' byte)
                    std::memcpy(m_data, m_sso.data, m_size + 1);
                    // Now we set our MEMORY_TYPE to HEAP since we just grew past our stack limit
                    m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
                    // Finally we store our capacity since on the stack we weren't using it yet
                    m_sso.capacity = newCapacity;
                }
            }
        }

        void Free()
        {
            // We only need to cleanup if we are using the heap
            if (m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP && m_data)
            {
                m_allocator->Free(m_data);
                m_data = nullptr;
            }
        }

        /** @brief
         * Private constructor to build a string of given length with given capacity.
         * It is up to the caller to initialize the string with actual data.
         */
        BasicString(const u64 size, const u64 capacity, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_size(size), m_allocator(allocator)
        {
            if (capacity > SSO_THRESHOLD)
            {
                // We need to allocate the memory on the heap.
                m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, capacity);
                // Keep track of the capacity
                m_sso.capacity = capacity;
                // Set the flag bit to Heap
                m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
            }
            else
            {
                // The capacity is small enough for SSO
                m_data = m_sso.data;
                // Set the flag bit to stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
            }
        }

    public:
        /** @brief Creates empty string with 1 null byte. (Will use SSO) */
        BasicString(Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            // Point our data pointer to the stack memory
            m_data = m_sso.data;
            // Set the size to 0 since the string is empty
            m_size = 0;

            m_sso.data[0]           = '\0';
            m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
        }

        /** @brief Creates empty string with 1 null byte. (Will use SSO) */
        explicit BasicString(decltype(nullptr), Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            // Point our data pointer to the stack memory
            m_data = m_sso.data;
            // Set the size to 0 since we got passed a nullptr
            m_size = 0;

            m_sso.data[0]           = '\0';
            m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
        }

        BasicString(const char* value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            if (value == nullptr)
            {
                m_size                  = 0;
                m_sso.data[0]           = '\0';
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                return;
            }

            Init(std::strlen(value));
            std::memcpy(m_data, value, m_size);
        }

        BasicString(const char* value, const u64 size, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            Init(size);
            std::memcpy(m_data, value, m_size);
        }

        explicit BasicString(const bool value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            if (value)
            {
                Init(4);  // We need space for 4 chars "true"
                std::memcpy(m_data, TRUE_VALUE, m_size);
            }
            else
            {
                Init(5);  // We need space for 5 char "false"
                std::memcpy(m_data, FALSE_VALUE, m_size);
            }
        }

        explicit BasicString(const u32 value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            const auto buffer = std::to_string(value);
            Init(buffer.size());
            std::memcpy(m_data, buffer.data(), m_size);
        }

        explicit BasicString(const i32 value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            const auto buffer = std::to_string(value);
            Init(buffer.size());
            std::memcpy(m_data, buffer.data(), m_size);
        }

        explicit BasicString(const u64 value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            const auto buffer = std::to_string(value);
            Init(buffer.size());
            std::memcpy(m_data, buffer.data(), m_size);
        }

        explicit BasicString(const i64 value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            const auto buffer = std::to_string(value);
            Init(buffer.size());
            std::memcpy(m_data, buffer.data(), m_size);
        }

        explicit BasicString(const f32 value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            const auto buffer = std::to_string(value);
            Init(buffer.size());
            std::memcpy(m_data, buffer.data(), m_size);
        }

        explicit BasicString(const f64 value, Allocator* allocator = BaseAllocator<Allocator>::GetDefault()) : m_allocator(allocator)
        {
            const auto buffer = std::to_string(value);
            Init(buffer.size());
            std::memcpy(m_data, buffer.data(), m_size);
        }

        BasicString(const BasicString& other) : m_size(other.m_size), m_allocator(other.m_allocator)
        {
            // Check 'other' is stack or heap allocated
            if (other.m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)  // Heap allocated
            {
                // Reserve enough capacity for the space that the other string is taking (Extra byte for '\0'
                // character).
                m_sso.capacity = m_size + 1;
                // Set our MEMORY_TYPE bit to use the heap
                m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
                // Allocate enough space on the heap for our entire capacity
                m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, m_sso.capacity);
                // Copy over the contents from 'other' (we use our capacity instead of our size since we also want the
                // trailing '\0' byte from 'other')
                std::memcpy(m_data, other.m_data, m_sso.capacity);
            }
            else  // Stack allocated
            {
                // We point our data pointer to our stack space
                m_data = m_sso.data;
                // Set our MEMORY_TYPE bit to use the stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                // We can copy over the data from 'other' (we use size + 1 to also copy the '\0' byte)
                std::memcpy(&m_sso.data, &other.m_sso.data, m_size + 1);
            }
        }

        BasicString(BasicString&& other) noexcept : m_size(other.m_size), m_allocator(other.m_allocator)
        {
            // Check 'other' is stack or heap allocated
            if (other.m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)  // Heap allocated
            {
                // We can simply copy the capacity over
                m_sso.capacity = other.m_sso.capacity;
                // Set our MEMORY_TYPE bit to use the heap
                m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
                // We take ownership of the data pointer
                m_data = other.m_data;
                // Set 'other' MEMORY_TYPE to USE_STACK so we don't double free
                other.m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
            }
            else  // Stack allocated
            {
                // We point our data pointer to our stack space
                m_data = m_sso.data;
                // Set our MEMORY_TYPE bit to use the stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                // We can copy over the data from 'other' (we use size + 1 to also copy the '\0' byte)
                std::memcpy(&m_sso.data, &other.m_sso.data, m_size + 1);
            }
        }

        BasicString& operator=(const BasicString& other)
        {
            if (this == &other) return *this;

            // We need to free this string if there is any dynamically allocated memory
            Free();

            // Copy over the size
            m_size = other.m_size;
            // Copy over the allocator
            m_allocator = other.m_allocator;

            // Check 'other' is stack or heap allocated
            if (other.m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)  // Heap allocated
            {
                // Reserve enough capacity for the space that the other string is taking (Extra byte for '\0'
                // character).
                m_sso.capacity = m_size + 1;
                // Set our MEMORY_TYPE bit to use the heap
                m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
                // Allocate enough space on the heap for our entire capacity
                m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, m_sso.capacity);
                // Copy over the contents from 'other' (we use our capacity instead of our size since we also want the
                // trailing '\0' byte from 'other')
                std::memcpy(m_data, other.m_data, m_sso.capacity);
            }
            else  // Stack allocated
            {
                // We point our data pointer to our stack space
                m_data = m_sso.data;
                // Set our MEMORY_TYPE bit to use the stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                // We can copy over the data from 'other' (we use size + 1 to also copy the '\0' byte)
                std::memcpy(&m_sso.data, &other.m_sso.data, m_size + 1);
            }

            return *this;
        }

        BasicString& operator=(BasicString&& other) noexcept
        {
            // We need to free this string if there is any dynamically allocated memory
            Free();

            // Copy over the size
            m_size = other.m_size;
            // Copy over the allocator
            m_allocator = other.m_allocator;

            // Check 'other' is stack or heap allocated
            if (other.m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)  // Heap allocated
            {
                // We can simply copy the capacity over
                m_sso.capacity = other.m_sso.capacity;
                // Set our MEMORY_TYPE bit to use the heap
                m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
                // We take ownership of the data pointer
                m_data = other.m_data;
                // Set 'other' MEMORY_TYPE to USE_STACK so we don't double free
                other.m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
            }
            else  // Stack allocated
            {
                // We point our data pointer to our stack space
                m_data = m_sso.data;
                // Set our MEMORY_TYPE bit to use the stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                // We can copy over the data from 'other' (we use size + 1 to also copy the '\0' byte)
                std::memcpy(&m_sso.data, &other.m_sso.data, m_size + 1);
            }
            return *this;
        }

        BasicString& operator=(const char* other)
        {
            // Copy over the size
            auto newSize = std::strlen(other);

            // Check if 'other' needs to be stack or heap allocated
            if (newSize > SSO_THRESHOLD)
            {
                // We need the heap
                const auto newCapacity = newSize + 1;  // Extra byte for our '\0' char

                if (m_sso.data[MEMORY_TYPE] == SSO_USE_STACK)
                {
                    // We are currently using the stack so we need to allocate
                    m_sso.capacity          = newCapacity;
                    m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;

                    m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, newCapacity);
                    std::memcpy(m_data, other, newCapacity);
                }
                else
                {
                    // We are already using the heap
                    if (m_sso.capacity >= newCapacity)
                    {
                        // We have enough capacity already
                        std::memcpy(m_data, other, newCapacity);
                    }
                    else
                    {
                        // We don't have enough space. So let's free first
                        Free();

                        m_sso.capacity = newCapacity;
                        m_data         = m_allocator->template Allocate<char>(MemoryType::C3DString, newCapacity);
                        std::memcpy(m_data, other, newCapacity);
                    }
                }

                m_size = newSize;
            }
            else
            {
                // We can use the stack
                if (m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)
                {
                    // We are currently using the heap so let's free that memory first
                    Free();
                }

                // We point our data pointer to our stack space
                m_data = m_sso.data;
                // We save off our new size
                m_size = newSize;
                // Set our MEMORY_TYPE bit to use the stack
                m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
                // We can copy over the data from 'other' (we use size + 1 to also copy the '\0' byte)
                std::memcpy(&m_sso.data, other, m_size + 1);
            }
            return *this;
        }

        ~BasicString() { Free(); }

        /** @brief Reserve enough space in the string to hold the provided capacity. */
        void Reserve(const u64 capacity)
        {
            if (m_sso.data[MEMORY_TYPE] == SSO_USE_HEAP)
            {
                // The string is already allocated on the heap
                if (capacity > m_sso.capacity)
                {
                    // The new capacity is larger than the old so we need to allocate more space

                    // Allocate enough space for the new data
                    const auto newData = m_allocator->template Allocate<char>(MemoryType::C3DString, capacity);
                    // Copy over the old data (including the final '0' byte)
                    if (m_size > 0) std::memcpy(newData, m_data, m_size + 1);
                    // Now we free our current pointer
                    m_allocator->Free(m_data);
                    // And set our data pointer to the newly allocated block
                    m_data = newData;
                }
            }
            else
            {
                // The string is currently on the stack
                if (capacity > SSO_THRESHOLD)
                {
                    // Our new capacity is too large for SSO

                    // Allocate enough space for the new capacity
                    m_data = m_allocator->template Allocate<char>(MemoryType::C3DString, capacity);
                    // Copy over the old data from the stack to the heap (including the final '0' byte)
                    if (m_size > 0) std::memcpy(m_data, m_sso.data, m_size + 1);
                    // Now we set our MEMORY_TYPE to HEAP since we just grew past our stack limit
                    m_sso.data[MEMORY_TYPE] = SSO_USE_HEAP;
                    // Finally we store our capacity since on the stack we weren't using it yet
                    m_sso.capacity = capacity;
                }
            }
        }

        /** @brief Use this method if you want to read content from a file into this string.
         * This method allocates enough space for capacity and sets the internal size to capacity - 1
         * It also makes sure that the final character is a null-terminator
         *
         * @param capacity The capacity required for the content (including null-terminator)
         */
        void PrepareForReadFromFile(u64 capacity)
        {
            // Reserve enough space for the incoming text
            Reserve(capacity);
            // Set the size so we can properly read in the content from a file
            m_size = capacity - 1;
            // Ensure our string is still null-terminated
            m_data[m_size] = '\0';
        }

        /** @brief Clear out the string so it's empty. Does not change the capacity. */
        void Clear()
        {
            m_size    = 0;
            m_data[0] = '\0';
        }

        /** @brief Completely destroy the string and it's contents.
         * This sets the size to 0 and frees it's internal memory (if any is allocated).
         */
        void Destroy()
        {
            Free();

            // Point our data pointer to the stack memory
            m_data = m_sso.data;
            // Set the size to 0 since the string is empty
            m_size = 0;

            m_sso.data[0]           = '\0';
            m_sso.data[MEMORY_TYPE] = SSO_USE_STACK;
        }

        /** @brief Builds this string from the format and the provided arguments.
         * Internally uses fmt::format to build out the string.
         * The formatted output will appended to the back of the string.
         */
        template <typename... Args>
        void Format(const char* format, Args&&... args)
        {
            fmt::format_to(std::back_inserter(*this), format, std::forward<Args>(args)...);
        }

        /** @brief Builds a string from the format and the provided arguments.
         * Internally uses fmt::format to build out the string.
         */
        template <typename... Args>
        static BasicString<DynamicAllocator> FromFormat(const char* format, Args&&... args)
        {
            BasicString<DynamicAllocator> buffer;
            fmt::vformat_to(std::back_inserter(buffer), format, fmt::make_format_args(args...));
            return buffer;
        }

        /** @brief Returns a string made from count * character */
        static BasicString<DynamicAllocator> Repeat(char c, u32 count)
        {
            BasicString<DynamicAllocator> str;
            str.Reserve(count);
            for (u32 i = 0; i < count; ++i) str.Append(c);
            return str;
        }

        /* @brief Append the provided string to the end of this string. */
        void Append(const BasicString& other)
        {
            // Calculate the totalSize that we will require for our appended string
            const u64 requiredSize = m_size + other.m_size;
            // Resize our internal buffer if it is needed (including '\0' byte)
            Resize(requiredSize + 1);
            // Copy data from the other string into our newly allocated buffer
            std::memcpy(m_data + m_size, other.Data(), other.m_size);
            // Add '\0' character at the end
            m_data[requiredSize] = '\0';
            // Store our new size
            m_size = requiredSize;
        }

        /* @brief Append the provided string to the end of this string. */
        template <u64 CCapacity>
        void Append(const CString<CCapacity>& other)
        {
            // Get size of other
            const u64 otherSize = other.Size();
            // Calculate the totalSize that we will require for our appended string
            const u64 requiredSize = m_size + otherSize;
            // Resize our internal buffer if it is needed (including '\0' byte)
            Resize(requiredSize + 1);
            // Copy data from the other string into our newly allocated buffer
            std::memcpy(m_data + m_size, other.Data(), otherSize);
            // Add '\0' character at the end
            m_data[requiredSize] = '\0';
            // Store our new size
            m_size = requiredSize;
        }

        /* @brief Append the provided const char* to the end of this string. */
        void Append(const char* other)
        {
            // Get the length of the other string
            const u64 otherLength = std::strlen(other);
            // Calculate the totalSize that we will require for our appended string
            const u64 requiredSize = m_size + otherLength;
            // Resize our internal buffer if it is needed (including '\0' byte)
            Resize(requiredSize + 1);
            // Copy data from the other string into our newly allocated buffer
            std::memcpy(m_data + m_size, other, otherLength);
            // Add '\0' character at the end
            m_data[requiredSize] = '\0';
            // Store our new size
            m_size = requiredSize;
        }

        /** @brief Append the provided const char to the end of this string. */
        void Append(const char c)
        {
            // Calculate the totalSize that we will require for our appended string
            const u64 requiredSize = m_size + 1;
            // Resize our internal buffer if it is needed (including '\0' byte)
            Resize(requiredSize + 1);
            // Copy char to the end of our buffer
            m_data[m_size] = c;
            // Add '\0' character at the end
            m_data[requiredSize] = '\0';
            // Store our new size
            m_size = requiredSize;
        }

        /** @brief Insert the provided const char at the provided index. */
        void Insert(u32 index, const char c)
        {
            // Calculate the totalSize that we will require for our inserted string
            const u64 requiredSize = m_size + 1;
            // Resize our internal buffer if it is needed (including '\0' byte)
            Resize(requiredSize + 1);
            // Move all chars from the index one to the right
            std::move_backward(begin() + index, end(), end() + 1);
            // Copy char to the provided index
            m_data[index] = c;
            // Ensure we null-terminate our string
            m_data[requiredSize] = '\0';
            // Store our new size
            m_size = requiredSize;
        }

        /** @brief Insert the provided string at the provided index. */
        void Insert(u32 index, const BasicString& str)
        {
            // Calculate the totalSize that we will require for our inserted string
            const u64 requiredSize = m_size + str.Size();
            // Resize our internal buffer if it is needed (including '\0' byte)
            Resize(requiredSize + 1);
            // Move all chars from the index str.Size() spaces to the right
            std::move_backward(begin() + index, end(), end() + str.Size());
            // Copy the chars from str into our data
            std::memcpy(m_data + index, str.Data(), str.Size());
            // Ensure we null-terminate our string
            m_data[requiredSize] = '\0';
            // Store our new size
            m_size = requiredSize;
        }

        /** @brief Added to support using default std::back_inserter(). */
        void push_back(const char c) { Append(c); }

        /** @brief Removes the last count characters from the string.
         * If count > String::Size(), String::Size() characters will be removed (leaving you with an empty string).
         */
        void RemoveLast(const u64 count = 1)
        {
            u64 newSize = 0;
            if (count < m_size)
            {
                newSize = m_size - count;
            }

            m_data[newSize] = '\0';
            m_size          = newSize;
        }

        /** @brief Removes a char at the provided at the provided index. */
        void RemoveAt(u32 index)
        {
            if (index < m_size && m_size > 0)
            {
                // Move the chars in the range [index + 1, end] to index
                std::move(begin() + index + 1, end(), begin() + index);
                // Decrement our size
                m_size--;
                // Ensure we end in a '\0'
                m_data[m_size] = '\0';
            }
        }

        /** @brief Removes the chars in the range [rangeStart, rangeEnd). */
        void RemoveRange(u32 rangeStart, u32 rangeEnd)
        {
            if (m_size > 0 && rangeEnd <= m_size && rangeStart < rangeEnd)
            {
                if (rangeEnd < m_size)
                {
                    // Move the chars in the range [rangeEnd, end()] to rangeStart
                    std::move(begin() + rangeEnd, end(), begin() + rangeStart);
                }

                // Decrease our size by the delta
                m_size -= (rangeEnd - rangeStart);
                // Ensure we end in a '\0'
                m_data[m_size] = '\0';
            }
        }

        /** @brief Splits the string at the given delimiter. */
        [[nodiscard]] DynamicArray<BasicString> Split(const char delimiter, const bool trimEntries = true, const bool skipEmpty = true) const
        {
            DynamicArray<BasicString> elements;
            BasicString current;
            for (u64 i = 0; i < m_size; i++)
            {
                if (m_data[i] == delimiter)
                {
                    if (!skipEmpty || !current.Empty())
                    {
                        if (trimEntries) current.Trim();

                        elements.PushBack(current);
                        current.Clear();
                    }
                }
                else
                {
                    current.Append(m_data[i]);
                }
            }

            if (!current.Empty())
            {
                if (trimEntries) current.Trim();
                elements.PushBack(current);
            }
            return elements;
        }

        /** @brief Removes all starting whitespace characters from the string. */
        void TrimLeft()
        {
            u64 newStart = 0;
            // Find the first non-space character
            while (std::isspace(m_data[newStart]))
            {
                newStart++;
            }
            // If the first character is a non-space character we do nothing
            if (newStart == 0) return;
            // Decrement the size by however many characters we have removed
            m_size -= newStart;
            // Copy over the remaining characters
            std::memcpy(m_data, m_data + newStart, m_size);
            // Add a null termination character
            m_data[m_size] = '\0';
        }

        /** @brief Removes all the trailing whitespace characters from the string. */
        void TrimRight()
        {
            u64 newSize = m_size;
            // Find the first no-space character at the end
            while (std::isspace(m_data[newSize - 1]))
            {
                newSize--;
            }
            // If the last character is a non-space character we do nothing
            if (newSize == m_size) return;
            // We save off our new size
            m_size = newSize;
            // Set the null termination character to end our string
            m_data[m_size] = '\0';
        }

        /** @brief Remove all the starting and trailing whitespace characters from the string. */
        void Trim()
        {
            TrimLeft();
            TrimRight();
        }

        /** @brief Checks if string starts with provided character sequence case-sensitive. */
        [[nodiscard]] bool StartsWith(const BasicString& sequence) const
        {
            // If our string is shorter than the sequence our string can not start with the sequence.
            if (m_size < sequence.Size()) return false;

            return std::equal(begin(), begin() + sequence.Size(), sequence.begin(), sequence.end());
        }

        /** @brief Checks if string starts with provided character sequence case-insensitive. */
        [[nodiscard]] bool StartsWithI(const BasicString& sequence) const
        {
            // If our string is shorter than the sequence our string can not start with the sequence.
            if (m_size < sequence.Size()) return false;

            return std::equal(begin(), begin() + sequence.Size(), sequence.begin(), sequence.end(),
                              [](const char a, const char b) { return std::tolower(a) == std::tolower(b); });
        }

        /** @brief Checks if string starts with provided character. */
        [[nodiscard]] bool StartsWith(const char c) const
        {
            // If we have an empty string it can not start with a char.
            if (m_size == 0) return false;

            return m_data[0] == c;
        }

        /** @brief Checks if string ends in the provided character sequence. */
        [[nodiscard]] bool EndsWith(const BasicString& sequence) const
        {
            // If our string is shorter than the sequence our string can not start with the sequence.
            if (m_size < sequence.Size()) return false;

            return std::equal(end() - sequence.Size(), end(), sequence.begin(), sequence.end());
        }

        /** @brief Checks if string ends with the provided character. */
        [[nodiscard]] bool EndsWith(const char c) const
        {
            // If we have an empty string it can not end with a char.
            if (m_size == 0) return false;

            return m_data[m_size - 1] == c;
        }

        /** @brief Find the first occurence of the provided char. If no occurence exists end() is returned.
         *
         * @param c Char that you want to look for
         * @return const_iterator to where the first instance of the char can be found.
         */
        [[nodiscard]] const_iterator Find(const char c) const
        {
            for (i32 i = 0; i < m_size; i++)
            {
                if (m_data[i] == c)
                {
                    return const_iterator(m_data + i);
                }
            }
            return end();
        }

        /** @brief Find the last occurence of the provided char. If no occurence exists end() is returned.
         *
         * @param c Char that you want to look for
         * @return const_iterator to where the last instance of the char can be found.
         */
        [[nodiscard]] const_iterator FindLastOf(const char c) const
        {
            for (i32 i = m_size - 1; i >= 0; i--)
            {
                if (m_data[i] == c)
                {
                    return const_iterator(m_data + i);
                }
            }
            return end();
        }

        /** @brief Find the last occurence in the string where the predicate holds.
         * If the predicate does not hold for any character in the string end() is returned
         *
         * @param p The predicate you want to check
         * @return const_iterator to where the last instance in the string is where the predicate holds.
         */
        template <typename Predicate>
        [[nodiscard]] const_iterator FindLastWhere(Predicate p) const
        {
            for (i32 i = m_size - 1; i >= 0; i--)
            {
                if (p(m_data[i]))
                {
                    return const_iterator(m_data + i);
                }
            }
            return end();
        }

        [[nodiscard]] bool Contains(const char c) const { return std::find(begin(), end(), c) != end(); }

        /**
         * @brief Compares the string with the other const char case-sensitive
         *
         * @param other The const char* you want to compare against
         * @return True if equal (case-sensitive), false otherwise
         */
        [[nodiscard]] bool Equals(const char* other) const { return std::strcmp(m_data, other) == 0; }

        /**
         * @brief Compares the string with the other const char case-sensitive upto n characters
         *
         * @param other The const char* you want to compare against
         * @param n The amount of characters you want to check upto
         * @return True if equal (case-sensitive), false otherwise
         */
        [[nodiscard]] bool NEquals(const char* other, const int n) const { return std::strncmp(m_data, other, n) == 0; }

        /**
         * @brief Compares the string with the other const char case-insensitive
         *
         * @param other The const char* you want to compare against
         * @return True if equal (case-insensitive), false otherwise
         */
        [[nodiscard]] bool IEquals(const char* other) const
        {
#ifdef C3D_PLATFORM_WINDOWS
            return _stricmp(m_data, other) == 0;
#elif defined(C3D_PLATFORM_LINUX)
            return strcasecmp(m_data, other) == 0;
#endif
        }

        /**
         * @brief Compares the string with the other const char case-insensitive upto n characters
         *
         * @param other The const char* you want to compare against
         * @param n The amount of characters you want to check upto
         * @return True if equal (case-insensitive), false otherwise
         */
        [[nodiscard]] bool NIEquals(const char* other, const int n) const
        {
#ifdef C3D_PLATFORM_WINDOWS
            return _strnicmp(m_data, other, n) == 0;
#elif defined(C3D_PLATFORM_LINUX)
            return strncasecmp(m_data, other, n) == 0;
#endif
        }

        /**
         * @brief Check if another string matches case-sensitive
         *
         * @param other The other string you want to compare against
         * @return True if equal (case-sensitive), false otherwise
         */
        [[nodiscard]] bool Equals(const BasicString& other) const { return std::equal(begin(), end(), other.begin(), other.end()); }

        /**
         * @brief Check if another string matches case-insensitive
         *
         * @param other The other string you want to compare against
         * @return True if equal (case-insensitive), false otherwise
         */
        [[nodiscard]] bool IEquals(const BasicString& other) const
        {
            return std::equal(begin(), end(), other.begin(), other.end(), [](const char a, const char b) { return tolower(a) == tolower(b); });
        }

        /** @brief Returns the utf8 codepoint at the given index.
         * Will set advance to the amount of characters that need to be skipped to get the next character
         */
        [[nodiscard]] i32 ToCodepoint(const u64 index, u8& advance) const
        {
            const int codepoint = static_cast<u8>(m_data[index]);
            if (codepoint >= 0 && codepoint < 0x7F)
            {
                // Singe-byte character
                advance = 1;
                return codepoint;
            }

            if ((codepoint & 0xE0) == 0xC0)
            {
                // Double-byte character
                advance = 2;
                return ((codepoint & 0b00011111) << 6) + m_data[index + 1] & 0b00111111;
            }

            if ((codepoint & 0xF0) == 0xE0)
            {
                // Triple-byte character
                advance = 3;
                return ((codepoint & 0b00001111) << 12) + ((m_data[index + 1] & 0b00111111) << 6) + (m_data[index + 2] & 0b00111111);
            }

            if ((codepoint & 0xF8) == 0xF0)
            {
                // Four-byte character
                advance = 4;
                return ((codepoint & 0b00000111) << 18) + ((m_data[index + 1] & 0b00111111) << 12) + ((m_data[index + 2] & 0b00111111) << 6) +
                       (m_data[index + 3] & 0b00111111);
            }

            ERROR_LOG("Invalid 5 or 6-byte character in String.");
            advance = 1;
            return -1;
        }

        /** @brief Gets a substring starting at the provided start iterator until the provided end iterator. */
        [[nodiscard]] BasicString<Allocator> SubStr(const_iterator startIt, const_iterator endIt) const
        {
            // Calculate the size of our new substring
            const auto size = endIt - startIt;
            // Create the new string here on the stack (RVO will make sure this string gets moved)
            // We increase the capacity by 1 since we need to add a '\0' byte
            BasicString<Allocator> sub(size, size + 1);
            // Copy the elements from our current string into the new substring
            std::copy_n(startIt, size, sub.m_data);
            // Add the ending '\0' byte
            sub.m_data[size] = '\0';
            return sub;
        }

        /** @brief Gets a substring starting at the provided start iterator until the end of the string. */
        [[nodiscard]] BasicString<Allocator> SubStr(const_iterator startIt) const { return SubStr(startIt, cend()); }

        /**
         * @brief Gets a substring from the provided start to the end
         *
         * @param start The index at which we start taking the substring
         * @param end The index at which we stop taking the substring (upto the end of the string if = 0)
         * @return A newly created substring from start to end
         */
        [[nodiscard]] BasicString<Allocator> SubStr(const u64 start, const u64 end = 0) const
        {
            // If the provided index is 0 or larger than our total string we simply take the end of the string as an end
            // index
            const auto endIndex = (end == 0 || end > Size()) ? Size() : end;
            const auto size     = endIndex - start;
            // Create the new string here on the stack (RVO will make sure this string gets moved)
            // We increase the capacity by 1 since we need to add a '\0' byte
            BasicString<Allocator> sub(size, size + 1);
            // Copy the elements from our current string starting at start (size characters in total) into the new
            // string
            std::copy_n(m_data + start, size, sub.m_data);
            // Add the ending '\0' byte
            sub.m_data[size] = '\0';
            return sub;
        }

        /** @brief Converts string to a i64. */
        [[nodiscard]] i64 ToI64(const i32 base = 10) const { return std::strtoll(m_data, nullptr, base); }

        /** @brief Converts string to a u64. */
        [[nodiscard]] u64 ToU64(const i32 base = 10) const { return std::strtoull(m_data, nullptr, base); }

        /** @brief Converts string to a f64. */
        [[nodiscard]] f64 ToF64() const { return std::strtod(m_data, nullptr); }

        /** @brief Converts string to a f32. */
        [[nodiscard]] f32 ToF32() const { return std::strtof(m_data, nullptr); }

        /** @brief Converts string to an i32 in the provided base. */
        [[nodiscard]] i32 ToI32(const i32 base = 10) const { return std::strtol(m_data, nullptr, base); }

        /** @brief Converts string to an u32 in the provided base. */
        [[nodiscard]] u32 ToU32(const i32 base = 10) const { return std::strtoul(m_data, nullptr, base); }

        /** @brief Converts string to an i16 in the provided base. */
        [[nodiscard]] i16 ToI16(const i32 base = 10) const { return static_cast<i16>(std::strtol(m_data, nullptr, base)); }

        /** @brief Converts string to an u16 in the provided base. */
        [[nodiscard]] u16 ToU16(const i32 base = 10) const { return static_cast<u16>(std::strtoul(m_data, nullptr, base)); }

        /** @brief Converts string to an i8 in the provided base. */
        [[nodiscard]] i8 ToI8(const i32 base = 10) const { return static_cast<i8>(std::strtol(m_data, nullptr, base)); }

        /** @brief Converts string to an u8 in the provided base. */
        [[nodiscard]] u8 ToU8(const i32 base = 10) const { return static_cast<u8>(std::strtoul(m_data, nullptr, base)); }

        /** @brief Converts string to a boolean value. */
        [[nodiscard]] bool ToBool() const
        {
            if (IEquals("1") || IEquals("true")) return true;
            return false;
        }

        /** @brief Converts string to a vec3. */
        [[nodiscard]] vec2 ToVec2() const
        {
            vec2 vec = {};
#ifdef C3D_PLATFORM_WINDOWS
            if (sscanf_s(m_data, "%f %f", &vec.x, &vec.y) == -1)
            {
                throw std::invalid_argument("The string does not contain a valid vec2 representation.");
            }
#elif defined(C3D_PLATFORM_LINUX)
            if (scanf(m_data, "%f %f", &vec.x, &vec.y) == -1)
            {
                throw std::invalid_argument("The string does not contain a valid vec2 representation.");
            }
#endif
            return vec;
        }

        /** @brief Converts string to a vec3. */
        [[nodiscard]] vec3 ToVec3() const
        {
            vec3 vec = {};
#ifdef C3D_PLATFORM_WINDOWS
            if (sscanf_s(m_data, "%f %f %f", &vec.x, &vec.y, &vec.z) == -1)
            {
                throw std::invalid_argument("The string does not contain a valid vec3 representation.");
            }
#elif defined(C3D_PLATFORM_LINUX)
            if (scanf(m_data, "%f %f %f", &vec.x, &vec.y, &vec.z) == -1)
            {
                throw std::invalid_argument("The string does not contain a valid vec3 representation.");
            }
#endif
            return vec;
        }

        /** @brief Converts string to a vec4. */
        [[nodiscard]] vec4 ToVec4() const
        {
            vec4 vec = {};
#ifdef C3D_PLATFORM_WINDOWS
            if (sscanf_s(m_data, "%f %f %f %f", &vec.x, &vec.y, &vec.z, &vec.w) == -1)
            {
                throw std::invalid_argument("The string does not contain a valid vec4 representation.");
            }
#elif defined(C3D_PLATFORM_LINUX)
            if (scanf(m_data, "%f %f %f %f", &vec.x, &vec.y, &vec.z, &vec.w) == -1)
            {
                throw std::invalid_argument("The string does not contain a valid vec4 representation.");
            }
#endif
            return vec;
        }

        /** @brief Gets the number of characters currently in the string (excluding the null-terminator). */
        [[nodiscard]] u64 Size() const { return m_size; }

        /* @brief Get the size of the string while keeping UTF8 multi-byte characters into account.
         * Warning does not support 5 or 6-byte characters!
         */
        [[nodiscard]] u64 SizeUtf8() const
        {
            u64 size = 0;
            for (u64 i = 0; i < m_size; size++)
            {
                const i32 c = m_data[i];

                if (c >= 0 && c < 127)
                    i++;  // 1-byte character
                else if ((c & 0xE0) == 0xC0)
                    i += 2;  // 2-byte character
                else if ((c & 0xF0) == 0xE0)
                    i += 3;  // 3-byte character
                else if ((c & 0xF8) == 0xF0)
                    i += 4;  // 4-byte character
                else
                {
                    ERROR_LOG("Invalid 5 or 6-byte character in String.");
                    return 0;
                }
            }
            return size;
        }

        /** @brief Gets the number of characters currently in the string (excluding the null-terminator). */
        [[nodiscard]] u64 Length() const { return m_size; }

        /** @brief Checks if the string is currently empty. */
        [[nodiscard]] bool Empty() const { return m_size == 0; }

        [[nodiscard]] bool EmptyOrWhitespace() const
        {
            if (m_size == 0) return true;
            for (u32 i = 0; i < m_size; i++)
            {
                if (!std::isspace(m_data[i])) return false;
            }
            return true;
        }

        /** @brief Returns a pointer to the internal character array. */
        [[nodiscard]] const char* Data() const { return m_data; }

        /** @brief Returns a pointer to the internal character array. */
        [[nodiscard]] char* Data() { return m_data; }

        /** @brief Returns an iterator pointing to the start of the character array. */
        [[nodiscard]] iterator begin() noexcept { return iterator(m_data); }

        /** @brief Returns a const_iterator pointing to the start of the character array. */
        [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(m_data); }

        /** @brief Returns a const_iterator pointing to the start of the character array. */
        [[nodiscard]] const_iterator cbegin() const noexcept { return const_iterator(m_data); }

        /** @brief Returns an iterator pointing to the element right after the last character in the character array. */
        [[nodiscard]] iterator end() noexcept { return iterator(m_data + m_size); }

        /** @brief Returns a const_iterator pointing to the element right after the last character in the character
         * array. */
        [[nodiscard]] const_iterator end() const noexcept { return const_iterator(m_data + m_size); }

        /** @brief Returns a const_iterator pointing to the element right after the last character in the character
         * array. */
        [[nodiscard]] const_iterator cend() const noexcept { return const_iterator(m_data + m_size); }

        /** @brief Returns the first char in the string by reference. */
        [[nodiscard]] char& First() const { return m_data[0]; }

        /** @brief Returns the last char in the string by reference. */
        [[nodiscard]] char& Last() const { return m_data[m_size - 1]; }

        explicit operator const char*() const { return m_data; }

        /** @brief Returns the char at the provided index by reference. */
        char& operator[](const u64 index) const { return m_data[index]; }

        /** @brief Returns the char at the provided index by reference.
         * Performs bounds checking internally.
         */
        [[nodiscard]] char& At(const u64 index) const
        {
            assert(index < m_size);
            return m_data[index];
        }

        /** @brief Checks if the string is empty. Will return true if the string is empty and false otherwise. */
        bool operator!() const { return m_size == 0; }

        /** @brief Checks if the string is empty. Will return false if the string is empty and true otherwise. */
        explicit operator bool() const { return m_size != 0; }

        /** @brief Operator overload for comparing with a const char pointer. */
        bool operator==(const char* other) const { return std::strcmp(m_data, other) == 0; }

        /** @brief Operator overload for comparing with another string. */
        bool operator==(const BasicString& other) const { return std::equal(begin(), end(), other.begin(), other.end()); }

        /** @brief Operator overload for inequality with a const char pointer. */
        bool operator!=(const char* other) const { return std::strcmp(m_data, other) != 0; }

        /** @brief Operator overload for inequality with another string. */
        bool operator!=(const BasicString& other) const { return !std::equal(begin(), end(), other.begin(), other.end()); }

        /** @brief Operator overload for appending another string to this string. */
        BasicString& operator+=(const BasicString& other)
        {
            Append(other);
            return *this;
        }

        template <u64 CCapacity>
        BasicString& operator+=(const CString<CCapacity>& other)
        {
            Append(other);
            return *this;
        }

        /** @brief Operator overload for appending const char* to this string. */
        BasicString& operator+=(const char* other)
        {
            Append(other);
            return *this;
        }

        /** @brief Operator overload for appending a u64 to this string. */
        BasicString& operator+=(u64 number)
        {
            Append(BasicString(number));
            return *this;
        }

        BasicString& operator+=(char c)
        {
            Append(c);
            return *this;
        }

        /** @brief Concatenate two strings. */
        C3D_API BasicString friend operator+(const BasicString& left, const BasicString& right)
        {
            // Our new size will be the size of both strings combined
            const u64 size = left.m_size + right.m_size;
            // Allocate memory for this size in our new String (capacity contains 1 extra byte for '\0' char)
            BasicString<Allocator> s(size, size + 1, left.m_allocator);

            // Copy the contents from the left to the start
            std::memcpy(s.m_data, left.m_data, left.m_size);
            // Copy the contents from the right to right after the left part
            std::memcpy(s.m_data + left.m_size, right.m_data, right.m_size);

            return s;
        }

        /** @brief Concatenate two strings with rvalue optimization for the left side. */
        C3D_API BasicString friend operator+(BasicString&& left, const BasicString& right)
        {
            // Our new size will be the size of both strings combined
            const u64 newSize = left.m_size + right.m_size;
            // Instead of creating a new string we can resize left since it is an rvalue anyway
            // Add an extra byte for the '\0' byte
            left.Resize(newSize + 1);
            // Since we kept the left side we only need to copy the right side over
            std::memcpy(left.m_data + left.m_size, right.m_data, right.m_size);
            // Now we update length of the left side
            left.m_size = newSize;
            // We need to add a '\0' byte at the end
            left[left.m_size] = '\0';
            // We can std::move left since it will be destroyed anyway
            return std::move(left);
        }

        /** @brief Concatenate a string with a const char*. */
        C3D_API BasicString friend operator+(const BasicString& left, const char* right)
        {
            // Get the size of the right
            const u64 rightSize = std::strlen(right);
            // Our new size will be the size of both strings combined
            const u64 size = left.m_size + rightSize;
            // Allocate memory for this size in our new String (capacity contains 1 extra byte for '\0' char)
            BasicString<Allocator> s(size, size + 1, left.m_allocator);

            // Copy the contents from the left to the start
            std::memcpy(s.m_data, left.m_data, left.m_size);
            // Copy the contents from the right to right after
            std::memcpy(s.m_data + left.m_size, right, rightSize);

            return s;
        }

        /** @brief Concatenate a string with a const char*, with rvalue optimization for the left side. */
        C3D_API BasicString friend operator+(BasicString&& left, const char* right)
        {
            // Get the size of right
            const u64 rightSize = std::strlen(right);
            // Our new size will be the size of both strings combined
            const u64 newSize = left.m_size + rightSize;
            // Instead of creating a new string we can resize left since it is an rvalue anyway
            // Add an extra byte for the '\0' byte
            left.Resize(newSize + 1);
            // Since we kept the left side we only need to copy the right side over
            std::memcpy(left.m_data + left.m_size, right, rightSize);
            // Now we update length of the left side
            left.m_size = newSize;
            // We need to add a '\0' byte at the end
            left[left.m_size] = '\0';
            // We can std::move left since it will be destroyed anyway
            return std::move(left);
        }

        /** @brief Concatenate a const char* with a string */
        C3D_API BasicString friend operator+(const char* left, const BasicString& right)
        {
            // Get the size of left
            const u64 leftSize = std::strlen(left);
            // Our new size will be the size of both strings combined
            const u64 size = leftSize + right.m_size;
            // Allocate memory for this size in our new String (capacity contains 1 extra byte for '\0' char)
            BasicString<Allocator> s(size, size + 1, right.m_allocator);

            // Copy the contents from the left to the start
            std::memcpy(s.m_data, left, leftSize);
            // Copy the contents from the right to right after
            std::memcpy(s.m_data + leftSize, right.Data(), right.m_size);
            // We need to add a '\0' byte at the end
            s[s.m_size] = '\0';
            return s;
        }

    private:
        char* m_data = nullptr;
        u64 m_size   = 0;

        /** @brief Our internal SSO data to store strings on the stack if they are small enough.
         * For large strings we store our current capacity in here and a '\1' in data[SSO_SIZE]
         * For small strings we store our string in data and a '\0' in data[SSO_SIZE]
         */
        union {
            u64 capacity;
            char data[SSO_CAPACITY] = {};
        } m_sso;

        Allocator* m_allocator;
    };

    using String = BasicString<DynamicAllocator>;

    template <class Allocator>
    C3D_API std::ostream& operator<<(std::ostream& cout, const BasicString<Allocator>& str)
    {
        cout << str.Data();
        return cout;
    }
}  // namespace C3D

template <class Allocator>
struct fmt::formatter<C3D::BasicString<Allocator>>
{
    template <typename ParseContext>
    static auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const C3D::BasicString<Allocator>& str, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "{}", str.Data());
    }
};

template <class Allocator>
struct std::hash<C3D::BasicString<Allocator>>
{
    size_t operator()(const C3D::BasicString<Allocator>& key) const noexcept
    {
        size_t hash = 0;
        for (const auto c : key)
        {
            hash ^= static_cast<size_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};
