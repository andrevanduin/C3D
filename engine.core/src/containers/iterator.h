
// ReSharper disable CppInconsistentNaming
#pragma once
#include <cstddef>
#include <iterator>
#include <utility>

namespace C3D
{
    template <typename T>
    class Iterator
    {
    public:
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        Iterator() : m_ptr(nullptr) {}

        Iterator(pointer ptr) : m_ptr(ptr) {}

        Iterator(const Iterator& other) : m_ptr(other.m_ptr) {}

        Iterator(Iterator&& other) noexcept : m_ptr(other.m_ptr) {}

        Iterator& operator=(const Iterator& other)
        {
            m_ptr = other.m_ptr;
            return *this;
        }

        Iterator& operator=(Iterator&& other) noexcept
        {
            std::swap(m_ptr, other.m_ptr);
            return *this;
        }

        ~Iterator() { m_ptr = nullptr; }

        Iterator& operator+=(difference_type diff)
        {
            m_ptr += diff;
            return *this;
        }

        Iterator& operator-=(difference_type diff)
        {
            m_ptr -= diff;
            return *this;
        }

        reference operator*() const { return *m_ptr; }

        pointer operator->() const { return m_ptr; }

        [[nodiscard]] pointer GetInternalPointer() const { return m_ptr; }

        reference operator[](difference_type index) const { return m_ptr[index]; }

        Iterator& operator++()
        {
            ++m_ptr;
            return *this;
        }

        Iterator& operator--()
        {
            --m_ptr;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator<T> temp = *this;
            ++m_ptr;
            return temp;
        }

        Iterator operator--(int)
        {
            Iterator<T> temp = *this;
            --m_ptr;
            return temp;
        }

        difference_type operator-(const Iterator& other) const { return m_ptr - other.m_ptr; }

        Iterator operator+(difference_type diff) const { return Iterator(m_ptr + diff); }

        Iterator operator-(difference_type diff) const { return Iterator(m_ptr - diff); }

        friend Iterator operator+(difference_type left, const Iterator& right) { return Iterator(left + right.m_ptr); }

        friend Iterator operator-(difference_type left, const Iterator& right) { return Iterator(left - right.m_ptr); }

        bool operator==(const Iterator& other) const { return m_ptr == other.m_ptr; }

        bool operator!=(const Iterator& other) const { return m_ptr != other.m_ptr; }

        bool operator<(const Iterator& other) const { return m_ptr < other.m_ptr; }

        bool operator>(const Iterator& other) const { return m_ptr > other.m_ptr; }

        bool operator<=(const Iterator& other) const { return m_ptr <= other.m_ptr; }

        bool operator>=(const Iterator& other) const { return m_ptr >= other.m_ptr; }

    private:
        pointer m_ptr;
    };
}  // namespace C3D