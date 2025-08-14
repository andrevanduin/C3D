
#pragma once
#include "defines.h"

namespace C3D
{
    class C3D_API UUID
    {
    public:
        /** @brief Default UUID which starts off as invalid. */
        UUID();
        /** @brief Create an UUID from an already existing 64-bit number. */
        UUID(u64 uuid);
        UUID(const UUID&);

        void Generate();

        void Invalidate();

        bool IsValid() const { return m_uuid != INVALID_ID_U64; }

        operator u64() const { return m_uuid; }
        operator bool() const { return m_uuid != INVALID_ID_U64; }

        bool operator==(const UUID& other) const { return m_uuid == other.m_uuid; }
        bool operator!=(const UUID& other) const { return m_uuid != other.m_uuid; }

        bool operator!() const { return m_uuid == INVALID_ID_U64; }

        static UUID Create();
        static UUID Invalid();

    private:
        u64 m_uuid = INVALID_ID_U64;
    };
}  // namespace C3D

template <>
struct fmt::formatter<C3D::UUID>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const C3D::UUID& uuid, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "{}", (u64)uuid);
    }
};

namespace std
{
    template <>
    struct hash<C3D::UUID>
    {
        std::size_t operator()(const C3D::UUID& uuid) const { return hash<u64>()((u64)uuid); }
    };
}  // namespace std
