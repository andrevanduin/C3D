
#pragma once
#include "uuid.h"

namespace C3D
{
    /** @brief A handle to a resource of Type. */
    template <typename Type>
    struct C3D_API Handle
    {
        Handle() = default;
        Handle(u32 index, UUID uuid) : index(index), uuid(uuid) {}

        void Invalidate()
        {
            index = INVALID_ID;
            uuid.Invalidate();
        }

        static Handle<Type> Invalid() { return Handle(); }

        bool IsValid() const { return index != INVALID_ID && uuid.IsValid(); }

        bool operator==(const Handle& other) const { return uuid == other.uuid; }
        bool operator!=(const Handle& other) const { return uuid != other.uuid; }

        operator bool() const { return index != INVALID_ID && uuid.IsValid(); }

        /** @brief An index into the array of items. */
        u32 index = INVALID_ID;
        /** @brief A globally unique identifier. */
        UUID uuid;
    };
}  // namespace C3D

template <typename Type>
struct fmt::formatter<C3D::Handle<Type>>
{
    template <typename ParseContext>
    static auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const C3D::Handle<Type> handle, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "(index: {}, uuid: {})", handle.index, handle.uuid);
    }
};
