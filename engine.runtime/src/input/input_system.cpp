
#include "input_system.h"

#include "events/event_system.h"
#include "logger/logger.h"
#include "platform/platform.h"
#include "system/system_manager.h"

namespace
{
    constexpr ivec2 ZERO_VEC = ivec2(0, 0);
}

namespace C3D
{
    bool InputSystem::OnInit()
    {
        for (auto& key : m_downKeys)
        {
            key = INVALID_ID_U8;
        }
        for (auto& button : m_downButtons)
        {
            button = INVALID_ID_U8;
        }

        // Bind the callbacks in the platform layer to our input system
        Platform::SetOnKeyCallback([this](Keys key, InputState state) { ProcessKey(key, state); });
        Platform::SetOnButtonCallback([this](Buttons button, InputState state) { ProcessButton(button, state); });
        Platform::SetOnMouseMoveCallback([this](i32 x, i32 y) { ProcessMouseMove(x, y); });
        Platform::SetOnMouseWheelCallback([this](i32 delta) { ProcessMouseWheel(delta); });

        // Check the current capslock state
        m_capslockActive = Platform::GetCurrentCapsLockState();

        m_initialized = true;

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void InputSystem::OnShutdown()
    {
        INFO_LOG("Shutting down.");
        m_initialized = false;
    }

    bool InputSystem::OnUpdate(const FrameData& frameData)
    {
        if (!m_initialized) return true;

        for (auto& index : m_downButtons)
        {
            if (index == INVALID_ID_U8) continue;

            auto& button = m_mouseCurrent.buttons[index];
            if (button.state == InputState::Down)
            {
                // If the button is still down we increment the downCount
                button.downCount++;
                if (button.downCount >= KEY_HELD_DELAY)
                {
                    // If the downCount passes the threshold we switch to held mode
                    button.state = InputState::Held;
                    // Reset the downCount for the next time
                    button.downCount = 0;
                    // Clear this button since we don't need to check it anymore
                    index = INVALID_ID_U8;

                    EventContext context = {};
                    context.data.u16[0]  = index;
                    context.data.i16[1]  = m_mouseCurrent.pos.x;
                    context.data.i16[2]  = m_mouseCurrent.pos.y;
                    Event.Fire(EventCodeButtonHeldStart, this, context);
                }
            }
        }
        for (auto& index : m_downKeys)
        {
            if (index == INVALID_ID_U8) continue;

            auto& key = m_keyboardCurrent.keys[index];
            if (key.state == InputState::Down)
            {
                // If the key is still down we increment the downCount
                key.downCount++;
                if (key.downCount >= KEY_HELD_DELAY)
                {
                    // If the downCount passes the threshold we switch to held mode
                    key.state = InputState::Held;
                    // Reset the downCount for the next time
                    key.downCount = 0;
                    // Clear this button since we don't need to check it anymore
                    index = INVALID_ID_U8;

                    EventContext context = {};
                    context.data.u16[0]  = index;
                    Event.Fire(EventCodeKeyHeldStart, this, context);
                }
            }
        }

        m_keyboardPrevious = m_keyboardCurrent;
        m_mousePrevious    = m_mouseCurrent;
        return true;
    }

    void InputSystem::ProcessKey(const Keys key, const InputState state)
    {
        if (key >= Keys::MaxKeys)
        {
            WARN_LOG("Key{} keycode was larger than expected '{}'.", state == InputState::Down ? "Down" : "Up", ToUnderlying(key));
            return;
        }

        auto& currentKey = m_keyboardCurrent.keys[key];

        EventContext context = {};
        context.data.u16[0]  = key;

        if (state == InputState::Down)
        {
            Event.Fire(EventCodeKeyDown, this, context);
            currentKey.state = InputState::Down;

            // Find an empty slot in our down keys array so we can keep track of how long this key has been down
            for (auto& index : m_downKeys)
            {
                if (index == INVALID_ID_U8)
                {
                    // We found an empty slot so let's set it and break
                    index = key;
                    break;
                }
            }
        }
        else
        {
            // If our capslock key goes up we toggle it's state
            if (key == KeyCapslock)
            {
                m_capslockActive ^= true;
            }

            Event.Fire(EventCodeKeyUp, this, context);
            currentKey.state = InputState::Up;
        }
    }

    void InputSystem::ProcessButton(const u8 button, const InputState state)
    {
        auto& currentButton = m_mouseCurrent.buttons[button];

        EventContext context = {};
        context.data.i16[0]  = button;
        context.data.i16[1]  = m_mouseCurrent.pos.x;
        context.data.i16[2]  = m_mouseCurrent.pos.y;

        if (state == InputState::Down)
        {
            currentButton.state = InputState::Down;
            Event.Fire(EventCodeButtonDown, this, context);

            // Find an empty slot in our down buttons array so we can keep track of how long this button has been down
            for (auto& index : m_downButtons)
            {
                if (index == INVALID_ID_U8)
                {
                    // We found an empty slot so let's set it and break
                    index = button;
                    break;
                }
            }
        }
        else
        {
            currentButton.state = InputState::Up;
            Event.Fire(EventCodeButtonUp, this, context);

            if (currentButton.inDrag)
            {
                Event.Fire(EventCodeMouseDraggedEnd, this, context);
                currentButton.inDrag = false;
            }
            else
            {
                Event.Fire(EventCodeButtonClicked, this, context);
            }
        }
    }

    void InputSystem::ProcessMouseMove(const i32 xPos, const i32 yPos)
    {
        const auto x = static_cast<i16>(xPos);
        const auto y = static_cast<i16>(yPos);

        if (m_mouseCurrent.pos.x != x || m_mouseCurrent.pos.y != y)
        {
            m_mouseCurrent.pos = { x, y };

            EventContext mouseMovedContext = {};
            mouseMovedContext.data.i16[0]  = x;
            mouseMovedContext.data.i16[1]  = y;

            Event.Fire(ToUnderlying(EventCodeMouseMoved), this, mouseMovedContext);

            for (i32 i = 0; i < MaxButtons; i++)
            {
                auto& currentButton = m_mouseCurrent.buttons[i];

                if (currentButton.state == InputState::Held)
                {
                    if (currentButton.inDrag)
                    {
                        // Already in drag
                        EventContext context = {};
                        context.data.i16[0]  = i;
                        context.data.i16[1]  = x;
                        context.data.i16[2]  = y;
                        Event.Fire(EventCodeMouseDragged, this, context);
                    }
                    else
                    {
                        /* Button is held but we have not started dragging yet */
                        EventContext context = {};
                        context.data.i16[0]  = i;
                        context.data.i16[1]  = x;
                        context.data.i16[2]  = y;
                        Event.Fire(EventCodeMouseDraggedStart, this, context);
                        currentButton.inDrag = true;
                    }
                }
            }
        }
    }

    void InputSystem::ProcessMouseWheel(const i32 delta) const
    {
        EventContext context{};
        context.data.i8[0] = static_cast<i8>(delta);
        Event.Fire(ToUnderlying(EventCodeMouseScrolled), nullptr, context);
    }

    void InputSystem::SetCapslockState(bool active) { m_capslockActive = active; }

    bool InputSystem::IsKeyDown(const u8 key) const
    {
        if (!m_initialized) return false;
        return m_keyboardCurrent.keys[key].state > InputState::Up; /* Down or Held */
    }

    bool InputSystem::IsKeyUp(const u8 key) const
    {
        if (!m_initialized) return true;
        return m_keyboardCurrent.keys[key].state == InputState::Up;
    }

    bool InputSystem::IsKeyPressed(const u8 key) const
    {
        if (!m_initialized) return false;
        return m_keyboardCurrent.keys[key].state == InputState::Down && m_keyboardPrevious.keys[key].state == InputState::Up;
    }

    bool InputSystem::WasKeyDown(const u8 key) const
    {
        if (!m_initialized) return false;
        return m_keyboardPrevious.keys[key].state > InputState::Up; /* Down or Held */
    }

    bool InputSystem::WasKeyUp(const u8 key) const
    {
        if (!m_initialized) return true;
        return m_keyboardPrevious.keys[key].state == InputState::Up;
    }

    bool InputSystem::IsButtonDown(const Buttons button) const
    {
        if (!m_initialized) return false;
        return m_mouseCurrent.buttons[button].state > InputState::Up; /* Down or Held */
    }

    bool InputSystem::IsButtonUp(const Buttons button) const
    {
        if (!m_initialized) return true;
        return m_mouseCurrent.buttons[button].state == InputState::Up;
    }

    bool InputSystem::IsButtonPressed(const Buttons button) const
    {
        if (!m_initialized) return false;
        return m_mouseCurrent.buttons[button].state == InputState::Down && m_mousePrevious.buttons[button].state == InputState::Up;
    }

    bool InputSystem::IsButtonDragging(Buttons button) const
    {
        if (!m_initialized) return false;
        return m_mouseCurrent.buttons[button].inDrag;
    }

    bool InputSystem::WasButtonDown(const Buttons button) const
    {
        if (!m_initialized) return false;
        return m_mousePrevious.buttons[button].state > InputState::Up; /* Down or Held */
    }

    bool InputSystem::WasButtonUp(const Buttons button) const
    {
        if (!m_initialized) return true;
        return m_mousePrevious.buttons[button].state == InputState::Up;
    }

    bool InputSystem::IsShiftDown() const
    {
        if (!m_initialized) return false;
        return m_keyboardCurrent.keys[KeyShift].state > InputState::Up || m_keyboardCurrent.keys[KeyLShift].state > InputState::Up ||
               m_keyboardCurrent.keys[KeyRShift].state > InputState::Up;
    }

    bool InputSystem::IsCtrlDown() const
    {
        if (!m_initialized) return false;
        return m_keyboardCurrent.keys[KeyControl].state > InputState::Up || m_keyboardCurrent.keys[KeyLControl].state > InputState::Up ||
               m_keyboardCurrent.keys[KeyRControl].state > InputState::Up;
    }
    bool InputSystem::IsAltDown() const
    {
        if (!m_initialized) return false;
        return m_keyboardCurrent.keys[KeyLAlt].state > InputState::Up || m_keyboardCurrent.keys[KeyRAlt].state > InputState::Up;
    }

    bool InputSystem::IsCapslockActive() const { return m_capslockActive; }

    const ivec2& InputSystem::GetMousePosition() const
    {
        if (!m_initialized)
        {
            return ZERO_VEC;
        }
        return m_mouseCurrent.pos;
    }

    const ivec2& InputSystem::GetPreviousMousePosition() const
    {
        if (!m_initialized)
        {
            return ZERO_VEC;
        }
        return m_mousePrevious.pos;
    }
}  // namespace C3D
