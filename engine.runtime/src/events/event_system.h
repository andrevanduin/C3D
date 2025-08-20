
#pragma once
#include "containers/dynamic_array.h"
#include "defines.h"
#include "event_context.h"
#include "logger/logger.h"
#include "system/system.h"
#include "types.h"

namespace C3D
{
    constexpr auto MAX_MESSAGE_CODES = 4096;

    class C3D_API EventSystem final : public BaseSystem
    {
        struct EventCallback
        {
            EventCallback(const EventCallbackId id, EventCallbackFunc func) : id(id), func(std::move(func)) {}

            EventCallbackId id;
            EventCallbackFunc func;
        };

        struct EventCodeEntry
        {
            DynamicArray<EventCallback> events;
        };

    public:
        bool OnInit() override;
        void OnShutdown() override;

        RegisteredEventCallback Register(u16 code, const EventCallbackFunc& callback);
        bool Unregister(u16 code, EventCallbackId& callbackId);
        bool Unregister(RegisteredEventCallback callback);

        bool UnregisterAll(u16 code);

        bool Fire(u16 code, void* sender, const EventContext& context) const;

    private:
        EventCodeEntry m_registered[MAX_MESSAGE_CODES] = {};
    };
}  // namespace C3D
