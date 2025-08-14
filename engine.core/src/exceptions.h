
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    class Exception final : public std::exception
    {
    public:
        explicit Exception(const char* reason) : m_what(reason) {}
        explicit Exception(const String& reason) : m_what(reason) {}
        explicit Exception(String&& reason) : m_what(reason) {}

        template <typename... Args>
        Exception(const char* format, Args&&... args)
        {
            fmt::vformat_to(std::back_inserter(m_what), format, fmt::make_format_args(args...));
        }

        const char* what() const noexcept override { return m_what.Data(); }

    private:
        String m_what;
    };
}  // namespace C3D