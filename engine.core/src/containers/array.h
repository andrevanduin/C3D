
#pragma once
#include "asserts/asserts.h"
#include "defines.h"

namespace C3D
{
    template <typename Type, u64 ElementCount>
    class C3D_API Array
    {
    public:
        Array() = default;

        Array(std::initializer_list<Type> values)
        {
            C3D_ASSERT(values.size() <= ElementCount);
            std::copy(values.begin(), values.end(), m_elements);
        }

        Type& operator[](u64 index) { return m_elements[index]; }

        const Type& operator[](u64 index) const { return m_elements[index]; }

        Type& At(u64 index)
        {
            C3D_ASSERT(index > 0 && index < ElementCount);
            return m_elements[index];
        }

        [[nodiscard]] const Type& At(u64 index) const
        {
            C3D_ASSERT(index > 0 && index < ElementCount);
            return m_elements[index];
        }

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
