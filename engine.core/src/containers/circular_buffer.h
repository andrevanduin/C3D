
#pragma once
#include "defines.h"

namespace C3D
{
    template <typename Type, u64 ElementCount>
    class CircularBuffer
    {
    public:
        CircularBuffer() = default;

        Type& operator[](const u64 index) { return m_elements[index % ElementCount]; }

        const Type& operator[](const u64 index) const { return m_elements[index % ElementCount]; }

        [[nodiscard]] static constexpr u64 Size() { return ElementCount; }

        [[nodiscard]] constexpr Type* Data() { return static_cast<Type*>(m_elements); }

        [[nodiscard]] constexpr const Type* Data() const { return const_cast<Type*>(m_elements); }

        constexpr Type* begin() noexcept { return Data(); }

        [[nodiscard]] constexpr const Type* begin() const noexcept { return Data(); }

        constexpr Type* end() noexcept { return &m_elements[ElementCount]; }

        [[nodiscard]] constexpr const Type* end() const noexcept { return const_cast<Type*>(&m_elements[ElementCount]); }

    private:
        Type m_elements[ElementCount];
    };
}  // namespace C3D