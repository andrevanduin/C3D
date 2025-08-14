
#pragma once
#include <audio/audio_plugin.h>
#include <containers/dynamic_array.h>
#include <containers/queue.h>
#include <defines.h>

#include "audio_data.h"
#include "source.h"

namespace C3D
{
    extern "C" {
    C3D_API AudioPlugin* CreatePlugin();
    C3D_API void DeletePlugin(AudioPlugin* plugin);
    }

    class OpenALPlugin final : public AudioPlugin
    {
    public:
        bool Init(const AudioPluginConfig& config) override;
        void Shutdown() override;
        bool OnUpdate(const FrameData& frameData) override;

        bool LoadChunk(AudioFile& audio) override;
        bool LoadStream(AudioFile& audio) override;

        vec3 GetListenerPosition() const override;
        bool SetListenerPosition(const vec3& position) override;

        ListenerOrientation GetListenerOrientation() const override;
        bool SetListenerOrientation(const vec3& forward, const vec3& up) override;

        vec3 GetSourcePosition(u8 channelIndex) const override;
        void SetSourcePosition(u8 channelIndex, const vec3& position) override;

        bool GetSourceLoop(u8 channelIndex) const override;
        void SetSourceLoop(u8 channelIndex, bool loop) override;

        f32 GetSourceGain(u8 channelIndex) const override;
        void SetSourceGain(u8 channelIndex, f32 gain) override;

        bool SourcePlay(u8 channelIndex, AudioFile& audio) override;

        void SourcePlay(u8 channelIndex) override;
        void SourcePause(u8 channelIndex) override;
        void SourceResume(u8 channelIndex) override;
        void SourceStop(u8 channelIndex) override;

        void Unload(AudioFile& audio) override;

    private:
        u32 FindFreeBuffer();

        /** @brief The currently selected device to play audio on.*/
        ALCdevice* m_device = nullptr;
        /** @brief The current audio context. */
        ALCcontext* m_context = nullptr;
        /** @brief A pool of buffers to be used for all kinds of audio/music playback. */
        DynamicArray<ALuint> m_buffers;

        /** @brief The current listener's position in the world. */
        vec3 m_listenerPosition;
        vec3 m_listenerForward;
        vec3 m_listenerUp;

        /** @brief A collection of available sources. (size == config.maxSources) */
        Source* m_sources;
        /** @brief A collection of currently free/available buffer ids. */
        Queue<u32> m_freeBuffers;
    };
}  // namespace C3D