
#pragma once
#include "dynamic_array.h"
#include "identifiers/handle.h"
#include "iterator.h"

namespace C3D
{
    /** @brief A table that can be used by systems that rely on handles to resources. */
    template <typename Type, typename Allocator = DynamicAllocator>
    class C3D_API HandleTable
    {
        using value_type      = Type;
        using reference       = value_type&;
        using difference_type = ptrdiff_t;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = Iterator<Type>;
        using const_iterator  = ConstIterator<Type>;

    public:
        bool Create(u32 capacity, Allocator* allocator = BaseAllocator<Allocator>::GetDefault())
        {
            // Ensure we are using the provided allocator
            m_items.SetAllocator(allocator);

            if (capacity == 0)
            {
                ERROR_LOG("Capacity must be > 0.");
                return false;
            }

            m_items.Reserve(capacity);
            return true;
        }

        void Destroy() { m_items.Destroy(); }

        template <typename... Args>
        Handle<Type> Acquire(Args&&... args)
        {
            // Iterate over our items and find an empty slot
            for (u32 i = 0; i < m_items.Size(); ++i)
            {
                auto& item = m_items[i];

                // Find a slot that has an item with an invalid uuid (empty)
                if (!item.uuid)
                {
                    item = Type(std::forward<Args>(args)...);

                    // Create a new unique id
                    auto uuid = UUID::Create();
                    // Store it off in the item
                    item.uuid = uuid;
                    // Return a handle to this item
                    return Handle<Type>(i, uuid);
                }
            }

            // No empty slots available so let's add the item to the end of array

            // Create a new unique id
            auto uuid = UUID::Create();
            // Our index will be equal to the size of our m_items array
            auto index = m_items.Size();

            // Create the item
            Type item = Type(std::forward<Args>(args)...);
            // store off the uuid on the item
            item.uuid = uuid;
            // and push it to the back of our m_items array
            m_items.PushBack(item);

            // Finally we return a handle to this item
            return Handle<Type>(index, uuid);
        }

        void Release(Handle<Type> handle)
        {
            auto& item = m_items[handle.index];
            if (item.uuid == handle.uuid)
            {
                // The handle is not stale so let's release the item
                item.uuid.Invalidate();
                return;
            }

            // The handle is stale since the item at this index no longer matches the item associated with the handle
            ERROR_LOG("Handle with index: '{}' and uuid: '{}' is stale.", handle.index, handle.uuid);
        }

        Type& operator[](Handle<Type> handle) { return m_items[handle.index]; }
        const Type& operator[](Handle<Type> handle) const { return m_items[handle.index]; }

        iterator begin() { return m_items.begin(); }
        const_iterator begin() const { return m_items.begin(); }
        const_iterator cbegin() const { return m_items.cbegin(); }

        iterator end() { return m_items.end(); }
        const_iterator end() const { return m_items.end(); }
        const_iterator cend() const { return m_items.cend(); }

    private:
        DynamicArray<Type, Allocator> m_items;
    };
}  // namespace C3D