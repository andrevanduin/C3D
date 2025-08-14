
#pragma once
#include "defines.h"
#include "functions/function.h"

namespace C3D
{
    struct EventContext
    {
        union {
            i64 i64[2];
            u64 u64[2];
            f64 f64[2];

            i32 i32[4];
            u32 u32[4];
            f32 f32[4];

            i16 i16[8];
            u16 u16[8];

            i8 i8[16];
            u8 u8[16];

            char c[16];
        } data;
    };

    enum SystemEventCode : u16
    {
        EventCodeApplicationQuit = 0x01,
        /** @brief An event that gets triggered when a key goes down. */
        EventCodeKeyDown,
        /** @brief An event that gets triggered when a key goes up. */
        EventCodeKeyUp,
        /** @brief An event that gets triggered when a key starts being held. */
        EventCodeKeyHeldStart,
        /** @brief An event that gets triggered when a mouse button goes down. */
        EventCodeButtonDown,
        /** @brief An event that gets triggered when a mouse button goes up. */
        EventCodeButtonUp,
        /** @brief An event that gets triggered when a mouse button goes up after being down (but not held). */
        EventCodeButtonClicked,
        /** @brief An event that gets triggered when a mouse button starts being held. */
        EventCodeButtonHeldStart,
        /** @brief An event that gets triggered when the mouse is moved. */
        EventCodeMouseMoved,
        /** @brief An event that gets triggered when the mouse is being dragged (held and moved). */
        EventCodeMouseDragged,
        /** @brief An event that gets triggered when a mouse button starts being dragged. */
        EventCodeMouseDraggedStart,
        /** @brief An event that gets triggered when a mouse button stops being dragged. */
        EventCodeMouseDraggedEnd,
        /** @brief An event that gets triggered when the mouse wheel gets scrolled. */
        EventCodeMouseScrolled,
        EventCodeResized,
        EventCodeMinimized,
        EventCodeFocusGained,
        EventCodeSetRenderMode,

        EventCodeDebug0,
        EventCodeDebug1,
        EventCodeDebug2,
        EventCodeDebug3,
        EventCodeDebug4,

        EventCodeObjectHoverIdChanged,
        EventCodeDefaultRenderTargetRefreshRequired,
        EventCodeWatchedFileChanged,
        EventCodeWatchedFileRemoved,

        EventCodeMaxCode = 0xFF
    };

    // TODO: Replace with fancy StackFunction<bool(u16, void*, const EventContext&), 16> after it's properly implemented
    using EventCallbackFunc = std::function<bool(u16, void*, const EventContext&)>;
    using EventCallbackId   = u16;

#define INVALID_CALLBACK std::numeric_limits<u16>::max()

    struct RegisteredEventCallback
    {
        u16 code           = INVALID_ID_U16;
        EventCallbackId id = INVALID_ID_U16;
    };
}  // namespace C3D