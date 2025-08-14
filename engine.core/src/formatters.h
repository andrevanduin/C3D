
#pragma once
#include <fmt/format.h>

template <>
struct fmt::formatter<std::thread::id>
{
    template <typename ParseContext>
    static auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::thread::id& id, FormatContext& ctx)
    {
        const auto hash = std::hash<std::thread::id>{}(id);
        return fmt::format_to(ctx.out(), "{}", hash);
    }
};