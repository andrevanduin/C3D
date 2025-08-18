
#pragma once
#include <algorithm>
#include <type_traits>

#include "defines.h"
#include "memory/global_memory_system.h"

namespace C3D
{
    constexpr u64 MULTIPLIER = 97;

    template <class T>
    class C3D_API HashTable
    {
    public:
        HashTable()                       = default;
        HashTable(const HashTable& other) = delete;
        HashTable(HashTable&& other)      = delete;

        HashTable& operator=(const HashTable& other) = delete;
        HashTable& operator=(HashTable&& other)      = delete;

        ~HashTable() { Destroy(); }

        bool Create(u32 elementCount)
        {
            if (elementCount == 0)
            {
                Logger::Error("[HASHTABLE] - Element count must be a positive non-zero value.", elementCount);
                return false;
            }

            if (elementCount < 128)
            {
                Logger::Warn("[HASHTABLE] - Element count of {} is low. This might cause collisions!", elementCount);
            }

            m_elementCount = elementCount;
            m_elements     = Memory.Allocate<T>(MemoryType::HashTable, elementCount);
            return true;
        }

        bool Fill(const T& value)
        {
            if (std::is_pointer<T>())
            {
                ERROR_LOG("Should not be used with pointer types");
                return false;
            }
            std::fill_n(m_elements, m_elementCount, value);
            return true;
        }

        bool FillDefault()
        {
            if (std::is_pointer<T>())
            {
                ERROR_LOG("Should not be used with pointer types");
                return false;
            }

            std::fill_n(m_elements, m_elementCount, T());
            return true;
        }

        void Destroy()
        {
            if (m_elements && m_elementCount != 0)
            {
                // Call destructors for every element
                std::destroy_n(m_elements, m_elementCount);

                Memory.Free(m_elements);
                m_elementCount = 0;
                m_elements     = nullptr;
            }
        }

        bool Set(const char* name, const T& value)
        {
            if (!name)
            {
                ERROR_LOG("requires valid name and value.");
                return false;
            }

            u64 index         = Hash(name);
            m_elements[index] = value;
            return true;
        }

        template <u64 Capacity>
        bool Set(const CString<Capacity>& name, const T& value)
        {
            if (name.Empty())
            {
                ERROR_LOG("requires valid name and value.");
                return false;
            }

            u64 index         = Hash(name.Data());
            m_elements[index] = value;
            return true;
        }

        T Get(const char* name)
        {
            if (!name)
            {
                ERROR_LOG("requires valid name.");
                return m_elements[0];
            }
            return m_elements[Hash(name)];
        }

        template <u64 Capacity>
        T Get(const CString<Capacity>& name)
        {
            if (name.Empty())
            {
                ERROR_LOG("requires valid name.");
                return m_elements[0];
            }
            return m_elements[Hash(name.Data())];
        }
        static u64 GetMemoryRequirement(u64 elementCount) { return sizeof(T) * static_cast<u64>(elementCount); }

    private:
        u64 Hash(const char* name) const
        {
            u64 hash = 0;
            while (*name)
            {
                // Add every char in the name to the previous value of hash multiplied by a prime
                hash = hash * MULTIPLIER + *name;
                name++;
            }
            // Mod against element count to make sure hash is valid index in our array
            return hash % m_elementCount;
        }

        u32 m_elementCount = 0;
        T* m_elements      = nullptr;
    };
}  // namespace C3D
