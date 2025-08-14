
#pragma once
#include <AL/al.h>
#include <AL/alc.h>

namespace C3D
{
    constexpr auto OPEN_AL_PLUGIN_MUSIC_BUFFER_COUNT = 2;

    struct AudioData
    {
        /** @brief The current buffer being used to play sound effect types. */
        ALuint buffer = INVALID_ID;
        /** @brief The internal buffers used for streaming music file data. */
        ALuint buffers[OPEN_AL_PLUGIN_MUSIC_BUFFER_COUNT] = { INVALID_ID, INVALID_ID };
        /** @brief Inidicates if the music file should loop. */
        bool loop;
    };
}  // namespace C3D