
#pragma once
#include "defines.h"
#include "string/cstring.h"
#include "string/string.h"

namespace C3D
{
    using DynamicLibraryPrefix    = CString<8>;
    using DynamicLibraryExtension = CString<8>;
    using FileWatchId             = u32;

    enum class CopyFileStatus : u8
    {
        Success,
        NotFound,
        Locked,
        NoPermissions,
        Failed,
        Unknown,
    };

    enum WindowFlag : u8
    {
        /** @brief No flags set for the window. */
        WindowFlagNone = 0x0,
        /** @brief Center the window horizontally. When this flag is set the x property is ignored. */
        WindowFlagCenterHorizontal = 0x1,
        /** @brief Center the window vertically. When this flag is set the y property is ignored. */
        WindowFlagCenterVertical = 0x2,
        /** @brief Center the window horizontally and vertically. When this flag is set the x and y property are ignored. */
        WindowFlagCenter = 0x4,
        /** @brief Make the window automatically size to the entire screen during startup.
         * When this flag is set width and height are ignored. */
        WindowFlagFullScreen = 0x8,
    };

    using WindowFlagBits = u8;

    struct WindowConfig
    {
        /** @brief The name of the window. */
        String name;
        /** @brief The title of the window. */
        String title;
        /** @brief The horizontal position of the window (can be negative for multi monitor setups). */
        i32 x = 0;
        /** @brief The vertical position of the window (can be negative for multi monitor setups). */
        i32 y = 0;
        /** @brief The width of the window. */
        u16 width = 0;
        /** @brief The height of the window. */
        u16 height = 0;
        /** @brief The flags that you want to set for this window. */
        WindowFlagBits flags = WindowFlagNone;
        /** @brief A boolean indicating if the windows should be shown immediatly after creation. */
        bool showImmediatly = true;
    };

    struct WindowPlatformState;
    struct WindowRendererState;

    struct Window
    {
        /** @brief The name of the window. */
        String name;
        /** @brief The title of the window. */
        String title;

        /** @brief The index of this window. */
        u32 index = INVALID_ID;

        /** @brief The width and height of this window. */
        u16 width  = 0;
        u16 height = 0;

        /** @brief The pixel density of this window. This is readonly since the platform is responsible for writing to this. */
        f32 devicePixelRatio = 0.0f;

        /** @brief A boolean indicating if this window is currently resizing. */
        bool resizing = false;
        /** @brief The number of frames that have passed since the last resize event. */
        u16 framesSinceResize = 0;

        /** @brief A pointer to the platform specific state. */
        WindowPlatformState* platformState = nullptr;
        /** @brief A pointer to render specific state. */
        WindowRendererState* rendererState = nullptr;
    };
}  // namespace C3D
