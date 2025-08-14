
#include "free_list.h"

namespace C3D
{
    bool FreeList::Create(void* memory, const u64 memorySizeForNodes, const u64 smallestPossibleAllocation, const u64 managedSize)
    {
        m_smallestPossibleAllocation = smallestPossibleAllocation;
        m_totalNodes                 = memorySizeForNodes / sizeof(Node);
        m_nodesSize                  = memorySizeForNodes;
        m_totalManagedSize           = managedSize;

        m_nodes = static_cast<Node*>(memory);
        std::memset(m_nodes, 0, m_nodesSize);

        // Set all our nodes to invalid
        for (u64 i = 0; i < m_totalNodes; i++)
        {
            m_nodes[i].size   = 0;
            m_nodes[i].next   = nullptr;
            m_nodes[i].offset = INVALID_ID_U64;
        }

        // Set our head as the first node with all the memory available
        m_head         = &m_nodes[0];
        m_head->size   = m_totalManagedSize;
        m_head->offset = 0;

        TRACE("Created FreeList with: {} nodes to manage: {} bytes of memory.", m_totalNodes, m_totalManagedSize);
        return true;
    }

    void FreeList::Destroy()
    {
        // Zero out our nodes
        std::memset(m_nodes, 0, m_nodesSize);
        m_nodes = nullptr;
        // NOTE: We can't free our memory since the user of this class is responsible for the memory we are using.
    }

    bool FreeList::Resize(void* newMemory, const u64 newSize, void** outOldMemory)
    {
        if (m_totalManagedSize > newSize) return false;

        *outOldMemory             = m_nodes;
        const auto sizeDifference = newSize - m_totalManagedSize;
        const auto oldSize        = m_totalManagedSize;
        const auto oldHead        = m_head;

        m_nodes = static_cast<Node*>(newMemory);

        m_totalNodes       = newSize / m_smallestPossibleAllocation;
        m_totalManagedSize = newSize;

        // Invalidate the size and offset for all nodes (except for the head)
        for (u64 i = 1; i < m_totalNodes; i++)
        {
            m_nodes[i].offset = INVALID_ID_U64;
            m_nodes[i].size   = INVALID_ID_U64;
        }

        // Store our new head
        m_head = &m_nodes[0];

        // Copy over the nodes
        Node* newListNode   = m_head;
        const Node* oldNode = oldHead;
        if (!oldHead)
        {
            // There is no head meaning the entire list was allocated
            // We set the head to be the at the start of new memory
            // and size equal to the difference in size between the new and the old block
            m_head->offset = oldSize;
            m_head->size   = sizeDifference;
            m_head->next   = nullptr;
        }
        else
        {
            // Iterate over all nodes
            while (oldNode)
            {
                // Get a new node, copy the offset/size, and set next to it
                Node* newNode     = GetNode();
                newNode->offset   = oldNode->offset;
                newNode->size     = oldNode->size;
                newNode->next     = nullptr;
                newListNode->next = newNode;

                if (oldNode->next)
                {
                    // There is another node, move on
                    oldNode = oldNode->next;
                }
                else
                {
                    // We reached the end of the list
                    if (oldNode->offset + oldNode->size == oldSize)
                    {
                        // The last node in the old list extends to the end of the list so we simply add the size
                        // difference to it
                        newNode->size += sizeDifference;
                    }
                    else
                    {
                        // The last node
                        Node* newNodeEnd   = GetNode();
                        newNodeEnd->offset = oldSize;
                        newNodeEnd->size   = sizeDifference;
                        newNodeEnd->next   = nullptr;
                        newNode->next      = newNodeEnd;
                    }

                    break;
                }
            }
        }

        return true;
    }

    bool FreeList::Clear()
    {
        std::memset(m_nodes, 0, m_nodesSize);
        m_head         = &m_nodes[0];
        m_head->size   = m_totalManagedSize;
        m_head->offset = 0;
        m_head->next   = nullptr;
        return true;
    }

    bool FreeList::AllocateBlock(const u64 size, u64* outOffset) const
    {
        // Keep track of our current and previous node
        Node* prev = nullptr;
        // We start at the head of our free_list
        Node* current = m_head;

        while (current)
        {
            if (current->size == size)
            {
                // We have an exact size match so our allocation fits nicely into this node
                *outOffset = current->offset;

                if (prev)
                {
                    // We have a previous so we can simply point it to the next node
                    prev->next = current->next;
                }
                else if (current->next)
                {
                    // There is no previous node so our next node needs to become the head
                    m_head = current->next;
                }

                // Invalidate this node since it is no longer needed
                current->Invalidate();
                return true;
            }

            if (current->size > size)
            {
                // We have more space than is required for our allocation
                *outOffset = current->offset;

                current->size -= size;
                current->offset += size;
                return true;
            }

            prev    = current;
            current = current->next;
        }

        ERROR_LOG("Failed to find a node with enough space for the allocation.");
        return false;
    }

    bool FreeList::FreeBlock(const u64 size, const u64 offset) const
    {
#ifdef _DEBUG
        if (size == 0)
        {
            ERROR_LOG("Called with size = 0.");
            return false;
        }
#endif

        Node* prev    = nullptr;
        Node* current = m_head;

        // Check the case where the entire freelist is allocated (which means we have no head node)
        if (!current)
        {
            // In this case a new node is needed at the head
            const auto newHead = GetNode();
            newHead->offset    = offset;
            newHead->size      = size;
            newHead->next      = nullptr;
            m_head             = newHead;
            return true;
        }

        while (current)
        {
            if (current->offset + current->size == offset)
            {
                // We can append the new free block to the right of the current block
                current->size += size;

                // Check if this then connects our current node to the next node
                if (current->next && AreExactlyAdjacent(current, current->next))
                {
                    const auto next = current->next;

                    // We add the size of the next node to our new node
                    current->size += next->size;
                    // And connect our new node to the next node's next node
                    current->next = next->next;
                    // Then we invalidate the next node
                    next->Invalidate();
                }

                return true;
            }
            if (current->offset > offset)
            {
                // Our current node's offset is further into the memory block than where we want to free
                const auto newNode = GetNode();
                newNode->next      = current;
                newNode->offset    = offset;
                newNode->size      = size;

                if (prev)
                {
                    // We have a previous node so we insert our new node in between prev and current
                    prev->next = newNode;
                }
                else
                {
                    // We have no previous node so our new node becomes the new head
                    m_head = newNode;
                }

                // If our new node is exactly adjacent to the next
                if (newNode->next && AreExactlyAdjacent(newNode, newNode->next))
                {
                    const auto next = newNode->next;

                    // We add the size of the next node to our new node
                    newNode->size += next->size;
                    // And connect our new node to the next node's next node
                    newNode->next = next->next;
                    // Then we invalidate the next node
                    next->Invalidate();
                }

                // If our previous node is exactly adjacent to our new node
                if (prev && AreExactlyAdjacent(prev, newNode))
                {
                    // We add the size of our new node to our prev node
                    prev->size += newNode->size;
                    // And connect our prev node to our new node's next node
                    prev->next = newNode->next;
                    // Then we invalidate our new node
                    newNode->Invalidate();
                }

                return true;
            }

            // If our current node's offset is smaller then the provided offset we simply move to the next node
            prev    = current;
            current = current->next;
        }

        FATAL_LOG("Failed. No node was found.");
        return false;
    }

    u64 FreeList::FreeSpace() const
    {
        u64 free            = 0;
        const Node* current = m_head;
        while (current)
        {
            free += current->size;
            current = current->next;
        }
        return free;
    }

    bool FreeList::AreExactlyAdjacent(const Node* first, const Node* second) { return first->offset + first->size == second->offset; }

    FreeList::Node* FreeList::GetNode() const
    {
        for (u64 i = 0; i < m_totalNodes; i++)
        {
            if (m_nodes[i].offset == INVALID_ID_U64)
            {
                return &m_nodes[i];
            }
        }
        FATAL_LOG("Unable to get a valid node.");
        return nullptr;
    }
}  // namespace C3D
