
#include "open_al_plugin.h"

#include <AL/al.h>
#include <math/c3d_math.h>
#include <math/math_types.h>
#include <memory/global_memory_system.h>

#include "open_al_utils.h"

namespace C3D
{
    bool OpenALPlugin::Init(const AudioPluginConfig& config)
    {
        INFO_LOG("Initializing OpenAL Plugin.");

        m_config = config;

        if (m_config.maxSources == 0)
        {
            WARN_LOG("MaxSources was configured as 0, which is not possible. Defaulting to 8 sources.");
            m_config.maxSources = 8;
        }

        if (m_config.maxBuffers < 20)
        {
            WARN_LOG("MaxBuffers was configured to be < 20, the minimum recommended amount of buffers is 20. Defaulting to 256.");
            m_config.maxBuffers = 256;
        }

        m_buffers.Resize(m_config.maxBuffers);
        m_freeBuffers = Queue<u32>(m_config.maxBuffers);

        // TODO: We now just default to the first device. We should somehow iterate over the devices to pick the best one
        m_device = alcOpenDevice(nullptr);
        if (!m_device)
        {
            ERROR_LOG("Failed to create ALC Device.");
            return false;
        }

        INFO_LOG("OpenAL Device acquired.");

        m_context = alcCreateContext(m_device, nullptr);
        if (!m_context)
        {
            ERROR_LOG("Failed to create ALC Context.");
            return false;
        }

        if (!alcMakeContextCurrent(m_context))
        {
            OpenAL::CheckError();
            ERROR_LOG("Failed to make ALC Context current.");
            return false;
        }

        // Configure our listener with some default values
        SetListenerPosition(vec3(0, 0, 0));
        SetListenerOrientation(VEC3_FORWARD, VEC3_UP);

        // NOTE: Zero out velocity since we will probably never use it
        alListener3f(AL_VELOCITY, 0, 0, 0);

        OpenAL::CheckError();

        // Allocate enough space for our sources
        m_sources = Memory.NewArray<Source>(MemoryType::AudioType, m_config.maxSources);
        for (u32 i = 0; i < m_config.maxSources; i++)
        {
            if (!m_sources[i].Create(m_config.chunkSize))
            {
                ERROR_LOG("Failed to create audio source in OpenAL plugin.");
                return false;
            }
        }

        // Buffers
        alGenBuffers(m_buffers.Size(), m_buffers.GetData());
        OpenAL::CheckError();

        // Mark all buffers as free
        for (auto buffer : m_buffers)
        {
            m_freeBuffers.Enqueue(buffer);
        }

        INFO_LOG("Successfully initialized.");
        return true;
    }

    bool OpenALPlugin::LoadChunk(AudioFile& audio)
    {
        auto data = Memory.New<AudioData>(MemoryType::AudioType);

        data->buffer = FindFreeBuffer();
        if (data->buffer == INVALID_ID || !OpenAL::CheckError())
        {
            Memory.Delete(data);
            ERROR_LOG("Unable to open Audio File since there are no OpenAL buffers free.");
            return false;
        }

        auto format = audio.GetNumChannels() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        audio.SetFormat(format);

        if (audio.HasSamplesLeft())
        {
            audio.SetInternalPluginData(data);

            // We load the entire sound into a buffer
            void* pcm = audio.StreamBufferData();
            OpenAL::CheckError();
            alBufferData(data->buffer, format, static_cast<i16*>(pcm), audio.GetTotalSamplesLeft(), audio.GetSampleRate());
            OpenAL::CheckError();
            return true;
        }

        Memory.Delete(data);
        return false;
    }

    bool OpenALPlugin::LoadStream(AudioFile& audio)
    {
        auto data = Memory.New<AudioData>(MemoryType::AudioType);

        for (u32 i = 0; i < OPEN_AL_PLUGIN_MUSIC_BUFFER_COUNT; i++)
        {
            data->buffers[i] = FindFreeBuffer();
            if (data->buffers[i] == INVALID_ID)
            {
                Memory.Delete(data);
                ERROR_LOG("Unable to open Audio File since there are no OpenAL buffers free.");
                return false;
            }
        }

        OpenAL::CheckError();

        auto format = audio.GetNumChannels() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        audio.SetFormat(format);

        data->loop = true;

        audio.SetInternalPluginData(data);
        return true;
    }

    void OpenALPlugin::Shutdown()
    {
        INFO_LOG("Shutting down.");

        INFO_LOG("Destroying sources.")
        for (u32 i = 0; i < m_config.maxSources; i++)
        {
            m_sources[i].Destroy();
        }
        Memory.DeleteArray(m_sources, m_config.maxSources);
        m_sources = nullptr;

        INFO_LOG("Deleting buffers.");
        alDeleteBuffers(m_buffers.Size(), m_buffers.GetData());

        INFO_LOG("Destroying ALC Context.");
        if (m_context)
        {
            alcDestroyContext(m_context);
            m_context = nullptr;
        }

        INFO_LOG("Closing ALC Device.");
        if (m_device)
        {
            alcCloseDevice(m_device);
            m_device = nullptr;
        }

        INFO_LOG("Destroying buffers.")
        m_buffers.Destroy();
        m_freeBuffers.Destroy();
    }

    bool OpenALPlugin::OnUpdate(const FrameData& frameData) { return true; }

    vec3 OpenALPlugin::GetListenerPosition() const { return m_listenerPosition; }

    bool OpenALPlugin::SetListenerPosition(const vec3& position)
    {
        m_listenerPosition = position;
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        return OpenAL::CheckError();
    }

    ListenerOrientation OpenALPlugin::GetListenerOrientation() const { return { m_listenerForward, m_listenerUp }; }

    bool OpenALPlugin::SetListenerOrientation(const vec3& forward, const vec3& up)
    {
        m_listenerForward     = forward;
        m_listenerUp          = up;
        ALfloat orientation[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
        alListenerfv(AL_ORIENTATION, orientation);
        return OpenAL::CheckError();
    }

    vec3 OpenALPlugin::GetSourcePosition(u8 channelIndex) const { return m_sources[channelIndex].GetPosition(); }

    void OpenALPlugin::SetSourcePosition(u8 channelIndex, const vec3& position) { m_sources[channelIndex].SetPosition(position); }

    bool OpenALPlugin::GetSourceLoop(u8 channelIndex) const { return m_sources[channelIndex].GetLoop(); }

    void OpenALPlugin::SetSourceLoop(u8 channelIndex, bool loop) { m_sources[channelIndex].SetLoop(loop); }

    f32 OpenALPlugin::GetSourceGain(u8 channelIndex) const { return m_sources[channelIndex].GetGain(); }

    void OpenALPlugin::SetSourceGain(u8 channelIndex, f32 gain) { m_sources[channelIndex].SetGain(gain); }

    bool OpenALPlugin::SourcePlay(u8 channelIndex, AudioFile& audio) { return m_sources[channelIndex].Play(audio); }

    void OpenALPlugin::SourcePlay(u8 channelIndex) { m_sources[channelIndex].Play(); }
    void OpenALPlugin::SourcePause(u8 channelIndex) { m_sources[channelIndex].Pause(); }
    void OpenALPlugin::SourceResume(u8 channelIndex) { m_sources[channelIndex].Resume(); }
    void OpenALPlugin::SourceStop(u8 channelIndex) { m_sources[channelIndex].Stop(); }

    void OpenALPlugin::Unload(AudioFile& audio)
    {
        auto data = static_cast<AudioData*>(audio.GetPluginData());

        // Clear our chunk buffer (if it exists)
        if (data->buffer != INVALID_ID)
        {
            m_freeBuffers.Enqueue(data->buffer);
        }
        // Clear our streaming buffers (if they exist)
        for (auto buffer : data->buffers)
        {
            if (buffer != INVALID_ID)
            {
                m_freeBuffers.Enqueue(buffer);
            }
        }

        Memory.Delete(data);
    }

    u32 OpenALPlugin::FindFreeBuffer()
    {
        if (m_freeBuffers.Empty())
        {
            // We have no free buffers, let's try to free one first
            INFO_LOG("No free buffers found. Attempting to free an existing one.");
            if (!OpenAL::CheckError())
            {
                return INVALID_ID;
            }

            for (u32 i = 0; i < m_config.maxSources; i++)
            {
                auto& source = m_sources[i];

                ALuint buffers[16];
                ALint freedCount = 0;
                source.FreeIfNotInUse(buffers, &freedCount);

                for (u32 i = 0; i < freedCount; i++)
                {
                    m_freeBuffers.Enqueue(buffers[i]);
                }
            }
        }

        // Check free count again
        if (m_freeBuffers.Empty())
        {
            // If it's still == 0 we can't proceed and must report an error
            ERROR_LOG("Could not find any free buffers, even after trying to free some in use buffers.");
            return INVALID_ID;
        }

        u32 freeBufferId = m_freeBuffers.Pop();
        DEBUG_LOG("Found a free buffer with id: {}. Now there are {} free buffers left.", freeBufferId, m_freeBuffers.Count());

        return freeBufferId;
    }

    AudioPlugin* CreatePlugin() { return Memory.New<OpenALPlugin>(MemoryType::AudioType); }

    void DeletePlugin(AudioPlugin* plugin) { Memory.Delete(plugin); }
}  // namespace C3D