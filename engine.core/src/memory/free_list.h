
#pragma once
#include "defines.h"
#include "logger/logger.h"

namespace C3D
{
    class C3D_API FreeList
    {
        struct Node
        {
            u64 offset;
            u64 size;
            Node* next;

            void Invalidate()
            {
                offset = INVALID_ID_U64;
                size   = 0;
                next   = nullptr;
            }
        };

    public:
        FreeList() = default;

        bool Create(void* memory, u64 memorySizeForNodes, u64 smallestPossibleAllocation, u64 managedSize);
        void Destroy();

        bool Resize(void* newMemory, u64 newSize, void** outOldMemory);
        bool Clear();

        bool AllocateBlock(u64 size, u64* outOffset) const;
        bool FreeBlock(u64 size, u64 offset) const;

        [[nodiscard]] u64 FreeSpace() const;

        /** @brief Checks if memory block of first is exactly adjacent to second. */
        static bool AreExactlyAdjacent(const Node* first, const Node* second);

        static constexpr u64 GetMemoryRequirement(u64 usableSize, u64 smallestPossibleAllocation);

    private:
        [[nodiscard]] Node* GetNode() const;

        Node* m_nodes        = nullptr;
        mutable Node* m_head = nullptr;

        /** @brief Amount of nodes this list holds. */
        u64 m_totalNodes = 0;
        /** @brief The size of the memory block that holds the nodes. */
        u64 m_nodesSize = 0;
        /** @brief The size of the smallest allocation a user could possibly make with this freelist. */
        u64 m_smallestPossibleAllocation = 0;
        /** @brief The amount of memory that this freelist is used for. */
        u64 m_totalManagedSize = 0;
    };

    constexpr u64 FreeList::GetMemoryRequirement(const u64 usableSize, const u64 smallestPossibleAllocation)
    {
        auto elementCount = usableSize / (smallestPossibleAllocation * sizeof(Node));
        if (elementCount < 20) elementCount = 20;
        return elementCount * sizeof(Node);
    }
}  // namespace C3D
