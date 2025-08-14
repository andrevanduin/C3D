
#pragma once
#include "identifiers/uuid.h"

namespace C3D
{
    enum class AudioPluginType
    {
        None,
        OpenAL
    };

    enum class ChannelType : u8
    {
        Mono   = 1,
        Stereo = 2
    };

    enum class AudioType
    {
        Invalid,
        SoundEffect,
        MusicStream
    };

    struct AudioHandle
    {
        AudioHandle() : type(AudioType::Invalid) {}
        AudioHandle(AudioType type) : type(type) { uuid.Generate(); }

        AudioType type;
        UUID uuid;

        bool operator!() const { return !uuid; }
        operator bool() const { return uuid.operator bool(); }

        static AudioHandle Invalid() { return AudioHandle(); }
    };

    using EmitterHandle = UUID;
}  // namespace C3D