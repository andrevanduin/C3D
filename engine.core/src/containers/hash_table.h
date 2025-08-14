
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
        HashTable();
        HashTable(const HashTable& other) = delete;
        HashTable(HashTable&& other)      = delete;

        HashTable& operator=(const HashTable& other) = delete;
        HashTable& operator=(HashTable&& other)      = delete;

        ~HashTable();

        bool Create(u32 elementCount);

        bool Fill(const T& value);
        bool FillDefault();

        void Destroy();

        bool Set(const char* name, const T& value);

        template <u64 Capacity>
        bool Set(const CString<Capacity>& name, const T& value);

        T Get(const char* name);

        template <u64 Capacity>
        T Get(const CString<Capacity>& name);

        static u64 GetMemoryRequirement(u64 elementCount);

    private:
        u64 Hash(const char* name) const;

        u32 m_elementCount;
        T* m_elements;
    };

    template <class T>
    HashTable<T>::HashTable() : m_elementCount(0), m_elements(nullptr)
    {}

    template <class T>
    HashTable<T>::~HashTable()
    {
        Destroy();
    }

    template <class T>
    bool HashTable<T>::Create(const u32 elementCount)
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

    template <class T>
    bool HashTable<T>::Fill(const T& value)
    {
        if (std::is_pointer<T>())
        {
            Logger::Error("[HASHTABLE] - Fill() - Should not be used with pointer types");
            return false;
        }
        std::fill_n(m_elements, m_elementCount, value);
        return true;
    }

    template <class T>
    bool HashTable<T>::FillDefault()
    {
        if (std::is_pointer<T>())
        {
            Logger::Error("[HASHTABLE] - FillDefault() - Should not be used with pointer types");
            return false;
        }

        std::fill_n(m_elements, m_elementCount, T());
        return true;
    }

    template <class T>
    void HashTable<T>::Destroy()
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

    template <class T>
    bool HashTable<T>::Set(const char* name, const T& value)
    {
        if (!name)
        {
            Logger::Error("[HASHTABLE] - Set() - requires valid name and value.");
            return false;
        }

        u64 index         = Hash(name);
        m_elements[index] = value;
        return true;
    }

    template <class T>
    template <u64 Capacity>
    bool HashTable<T>::Set(const CString<Capacity>& name, const T& value)
    {
        if (name.Empty())
        {
            Logger::Error("[HASHTABLE] - Set() - requires valid name and value.");
            return false;
        }

        u64 index         = Hash(name.Data());
        m_elements[index] = value;
        return true;
    }

    template <class T>
    T HashTable<T>::Get(const char* name)
    {
        if (!name)
        {
            Logger::Error("[HASHTABLE] - Get() - requires valid name.");
            return m_elements[0];
        }
        return m_elements[Hash(name)];
    }

    template <class T>
    template <u64 Capacity>
    T HashTable<T>::Get(const CString<Capacity>& name)
    {
        if (name.Empty())
        {
            Logger::Error("[HASHTABLE] - Get() - requires valid name.");
            return m_elements[0];
        }
        return m_elements[Hash(name.Data())];
    }

    template <class T>
    u64 HashTable<T>::GetMemoryRequirement(const u64 elementCount)
    {
        return sizeof(T) * static_cast<u64>(elementCount);
    }

    template <class T>
    u64 HashTable<T>::Hash(const char* name) const
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
}  // namespace C3D
