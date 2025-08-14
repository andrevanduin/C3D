
#pragma once
#include "audio/audio_file.h"
#include "audio/audio_types.h"
#include "defines.h"
#include "math/math_types.h"

namespace C3D
{
    struct FrameData;

    struct AudioPluginConfig
    {
        u32 maxSources;
        u32 maxBuffers;
        u32 chunkSize;
        u32 frequency;
        u8 channelCount;
    };

    struct ListenerOrientation
    {
        vec3 forward;
        vec3 up;
    };

    class AudioPlugin
    {
    public:
        virtual bool Init(const AudioPluginConfig& config) = 0;
        virtual void Shutdown()                            = 0;
        virtual bool OnUpdate(const FrameData& frameData)  = 0;

        virtual bool LoadChunk(AudioFile& audio)  = 0;
        virtual bool LoadStream(AudioFile& audio) = 0;

        virtual vec3 GetListenerPosition() const               = 0;
        virtual bool SetListenerPosition(const vec3& position) = 0;

        virtual ListenerOrientation GetListenerOrientation() const               = 0;
        virtual bool SetListenerOrientation(const vec3& forward, const vec3& up) = 0;

        virtual vec3 GetSourcePosition(u8 channelIndex) const                 = 0;
        virtual void SetSourcePosition(u8 channelIndex, const vec3& position) = 0;

        virtual bool GetSourceLoop(u8 channelIndex) const      = 0;
        virtual void SetSourceLoop(u8 channelIndex, bool loop) = 0;

        virtual f32 GetSourceGain(u8 channelIndex) const      = 0;
        virtual void SetSourceGain(u8 channelIndex, f32 gain) = 0;

        virtual bool SourcePlay(u8 channelIndex, AudioFile& file) = 0;

        virtual void SourcePlay(u8 channelIndex)   = 0;
        virtual void SourcePause(u8 channelIndex)  = 0;
        virtual void SourceResume(u8 channelIndex) = 0;
        virtual void SourceStop(u8 channelIndex)   = 0;

        virtual void Unload(AudioFile& file) = 0;

    protected:
        AudioPluginConfig m_config;
    };
}  // namespace C3D