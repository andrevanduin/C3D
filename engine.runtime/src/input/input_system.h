
#pragma once
#include "defines.h"
#include "input/buttons.h"
#include "input/input_state.h"
#include "input/keys.h"
#include "logger/logger.h"
#include "math/math_types.h"
#include "system/system.h"

namespace C3D
{
    /** @brief How many update ticks we expect before switching to HELD state for keys. **/
    constexpr u8 KEY_HELD_DELAY = 10;
    /** @brief How many update ticks we expect before switching to HELD state for buttons. **/
    constexpr u8 BUTTON_HELD_DELAY = 10;
    /** @brief Maximum amount of keys that can be held. */
    constexpr u8 MAX_HELD_KEYS = 8;
    /** @brief Maximum amount of buttnos that can be held. */
    constexpr u8 MAX_HELD_BUTTONS = 3;

    struct KeyState
    {
        InputState state = InputState::Up;
        u8 downCount     = 0;
    };

    struct ButtonState
    {
        InputState state = InputState::Up;
        u8 downCount     = 0;
        bool inDrag      = false;
    };

    struct KeyBoardState
    {
        KeyState keys[ToUnderlying(Keys::MaxKeys)];
    };

    struct MouseState
    {
        ivec2 pos;
        ButtonState buttons[ToUnderlying(Buttons::MaxButtons)];
    };

    class InputSystem final : public BaseSystem
    {
    public:
        bool OnInit() override;
        void OnShutdown() override;

        bool OnUpdate(const FrameData& frameData) override;

        void ProcessKey(Keys key, InputState state);
        void ProcessButton(u8 button, InputState state);
        void ProcessMouseMove(i32 xPos, i32 yPos);
        void ProcessMouseWheel(i32 delta) const;

        void SetCapslockState(bool active);

        [[nodiscard]] C3D_API bool IsKeyDown(u8 key) const;
        [[nodiscard]] C3D_API bool IsKeyUp(u8 key) const;
        [[nodiscard]] C3D_API bool IsKeyPressed(u8 key) const;

        [[nodiscard]] C3D_API bool WasKeyDown(u8 key) const;
        [[nodiscard]] C3D_API bool WasKeyUp(u8 key) const;

        [[nodiscard]] C3D_API bool IsButtonDown(Buttons button) const;
        [[nodiscard]] C3D_API bool IsButtonUp(Buttons button) const;
        [[nodiscard]] C3D_API bool IsButtonPressed(Buttons button) const;
        [[nodiscard]] C3D_API bool IsButtonDragging(Buttons button) const;

        [[nodiscard]] C3D_API bool WasButtonDown(Buttons button) const;
        [[nodiscard]] C3D_API bool WasButtonUp(Buttons button) const;

        [[nodiscard]] C3D_API bool IsShiftDown() const;
        [[nodiscard]] C3D_API bool IsCtrlDown() const;
        [[nodiscard]] C3D_API bool IsAltDown() const;
        [[nodiscard]] C3D_API bool IsCapslockActive() const;

        [[nodiscard]] C3D_API const ivec2& GetMousePosition() const;
        [[nodiscard]] C3D_API const ivec2& GetPreviousMousePosition() const;

    private:
        KeyBoardState m_keyboardCurrent  = {};
        KeyBoardState m_keyboardPrevious = {};

        MouseState m_mouseCurrent  = {};
        MouseState m_mousePrevious = {};

        u8 m_downKeys[MAX_HELD_KEYS]       = {};
        u8 m_downButtons[MAX_HELD_BUTTONS] = {};

        bool m_capslockActive = false;
    };
}  // namespace C3D
