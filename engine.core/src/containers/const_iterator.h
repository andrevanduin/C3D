
// ReSharper disable CppInconsistentNaming
#pragma once
#include "iterator.h"

namespace C3D
{
	template <typename T>
	class ConstIterator
	{
	public:
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;

		ConstIterator() : m_ptr(nullptr) {}

		ConstIterator(pointer ptr) : m_ptr(ptr) {}

		ConstIterator(const_pointer ptr) : m_ptr(ptr) {}

		ConstIterator(const ConstIterator& other) : m_ptr(other.m_ptr) {}

		ConstIterator(const Iterator<T>& other) : m_ptr(other.GetInternalPointer()) {}

		ConstIterator(ConstIterator&& other) noexcept : m_ptr(other.m_ptr) {}

		ConstIterator& operator=(const ConstIterator& other)
		{
			m_ptr = other.m_ptr;
			return *this;
		}

		ConstIterator& operator=(ConstIterator&& other) noexcept
		{
			std::swap(m_ptr, other.m_ptr);
			return *this;
		}

		~ConstIterator()
		{
			m_ptr = nullptr;
		}

		ConstIterator& operator+=(difference_type diff)
		{
			m_ptr += diff;
			return *this;
		}

		ConstIterator& operator-=(difference_type diff)
		{
			m_ptr -= diff;
			return *this;
		}

		reference operator*() const
		{
			return *m_ptr;
		}

		pointer operator->() const
		{
			return m_ptr;
		}

		const_reference operator[](difference_type index) const
		{
			return m_ptr[index];
		}

		ConstIterator& operator++()
		{
			++m_ptr;
			return *this;
		}

		ConstIterator& operator--()
		{
			--m_ptr;
			return *this;
		}

		ConstIterator operator++(int)
		{
			Iterator<T> temp = *this;
			++m_ptr;
			return temp;
		}

		ConstIterator operator--(int)
		{
			Iterator<T> temp = *this;
			--m_ptr;
			return temp;
		}

		difference_type operator-(const ConstIterator& other) const
		{
			return m_ptr - other.m_ptr;
		}

		ConstIterator operator+(difference_type diff) const
		{
			return ConstIterator(m_ptr + diff);
		}

		ConstIterator operator-(difference_type diff) const
		{
			return ConstIterator(m_ptr - diff);
		}

		friend ConstIterator operator+(difference_type left, const ConstIterator& right)
		{
			return ConstIterator(left + right.m_ptr);
		}

		friend ConstIterator operator-(difference_type left, const ConstIterator& right)
		{
			return ConstIterator(left - right.m_ptr);
		}

		bool operator==(const ConstIterator& other) const
		{
			return m_ptr == other.m_ptr;
		}

		bool operator!=(const ConstIterator& other) const
		{
			return m_ptr != other.m_ptr;
		}

		bool operator<(const ConstIterator& other) const
		{
			return m_ptr < other.m_ptr;
		}

		bool operator>(const ConstIterator& other) const
		{
			return m_ptr > other.m_ptr;
		}

		bool operator<=(const ConstIterator& other) const
		{
			return m_ptr <= other.m_ptr;
		}

		bool operator>=(const ConstIterator& other) const
		{
			return m_ptr >= other.m_ptr;
		}

	private:
		pointer m_ptr;
	};
}