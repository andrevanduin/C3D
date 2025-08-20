
#include "event_system.h"

#include <algorithm>

#include "logger/logger.h"
#include "platform/platform.h"

namespace C3D
{
    bool EventSystem::OnInit()
    {
        Platform::SetOnWatchedFileChangedCallback([this](u32 watchId) {
            EventContext context;
            context.data.u32[0] = watchId;
            Fire(EventCodeWatchedFileChanged, nullptr, context);
        });

        Platform::SetOnWatchedFileDeletedCallback([this](u32 watchId) {
            EventContext context;
            context.data.u32[0] = watchId;
            Fire(EventCodeWatchedFileRemoved, nullptr, context);
        });

        m_initialized = true;

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void EventSystem::OnShutdown()
    {
        INFO_LOG("Unregistering and clearing all events.");
        for (auto& [events] : m_registered)
        {
            if (!events.Empty()) events.Clear();
        }
    }

    RegisteredEventCallback EventSystem::Register(const u16 code, const EventCallbackFunc& callback)
    {
        static EventCallbackId UNIQUE_ID = 0;

        auto& events = m_registered[code].events;
        events.EmplaceBack(UNIQUE_ID, callback);

        RegisteredEventCallback cb = { code, UNIQUE_ID++ };
        return cb;
    }

    bool EventSystem::Unregister(const u16 code, EventCallbackId& callbackId)
    {
        if (code > MAX_MESSAGE_CODES)
        {
            WARN_LOG("Tried to Unregister Event for invalid code: '{}'.", code);
            return false;
        }

        auto& events = m_registered[code].events;
        if (events.Empty())
        {
            WARN_LOG("Tried to Unregister Event for code: '{}' that has no events.", code);
            return false;
        }

        const auto it = std::find_if(events.begin(), events.end(), [&](const EventCallback& e) { return e.id == callbackId; });
        if (it != events.end())
        {
            events.Erase(it);
            callbackId = INVALID_ID_U16;
            return true;
        }

        WARN_LOG("Tried to Unregister Event that did not exist.");
        return false;
    }

    bool EventSystem::Unregister(RegisteredEventCallback callback) { return Unregister(callback.code, callback.id); }

    bool EventSystem::UnregisterAll(u16 code)
    {
        auto& events = m_registered[code].events;
        if (events.Empty())
        {
            WARN_LOG("Tried to UnRegister all Events for code: '{}' that has no events.", code);
            return false;
        }

        events.Clear();
        return true;
    }

    bool EventSystem::Fire(const u16 code, void* sender, const EventContext& context) const
    {
        if (!m_initialized) return true;

        const auto& events = m_registered[code].events;
        if (events.Empty())
        {
            return false;
        }

        return std::ranges::any_of(events, [&](const EventCallback& e) { return e.func(code, sender, context); });
    }
}  // namespace C3D
