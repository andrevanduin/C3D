
#include "source.h"

#include <AL/al.h>
#include <platform/platform.h>

#include "audio_data.h"
#include "open_al_utils.h"

namespace C3D
{
    bool Source::Create(u32 chunkSize)
    {
        m_chunkSize = chunkSize;

        alGenSources(1, &m_id);
        if (!OpenAL::CheckError())
        {
            ERROR_LOG("Failed to generate source.");
            return false;
        }

        // Dispatch our worker thread
        m_thread = std::thread(&Source::RunWorkerThread, this);
        return true;
    }

    void Source::Destroy()
    {
        m_triggerExit = true;
        m_thread.join();

        if (m_id != INVALID_ID)
        {
            alDeleteSources(1, &m_id);
            m_id = INVALID_ID;
        }
    }

    bool Source::SetDefaults(bool resetUse)
    {
        if (resetUse)
        {
            m_inUse = false;
        }

        if (!SetGain(1.0f))
        {
            ERROR_LOG("Failed to set Gain to default.");
            return false;
        }

        if (!SetPitch(1.0f))
        {
            ERROR_LOG("Failed to set Pitch to default.");
            return false;
        }

        if (!SetPosition(vec3(0.0f)))
        {
            ERROR_LOG("Failed to set Position to default.");
            return false;
        }

        if (!SetLoop(false))
        {
            ERROR_LOG("Failed to set Loop to default.");
            return false;
        }

        return true;
    }

    bool Source::SetGain(f32 gain)
    {
        m_gain = gain;
        alSourcef(m_id, AL_GAIN, gain);
        return OpenAL::CheckError();
    }

    bool Source::SetPitch(f32 pitch)
    {
        m_pitch = pitch;
        alSourcef(m_id, AL_PITCH, pitch);
        return OpenAL::CheckError();
    }

    bool Source::SetPosition(const vec3& position)
    {
        m_position = position;
        alSource3f(m_id, AL_POSITION, position.x, position.y, position.z);
        return OpenAL::CheckError();
    }

    bool Source::SetLoop(bool loop)
    {
        m_loop = loop;
        alSourcei(m_id, AL_LOOPING, loop);
        return OpenAL::CheckError();
    }

    bool Source::Play(AudioFile& audio)
    {
        std::lock_guard lock(m_mutex);

        auto internalData = static_cast<AudioData*>(audio.GetPluginData());
        m_current         = &audio;

        if (audio.GetType() == AudioType::SoundEffect)
        {
            alSourceQueueBuffers(m_id, 1, &internalData->buffer);
            if (!OpenAL::CheckError())
            {
                ERROR_LOG("Failed to queue up buffer: {} for sound effect.", internalData->buffer);
                return false;
            }
        }
        else
        {
            // Initially load into all buffers
            for (auto buffer : internalData->buffers)
            {
                if (!StreamMusicData(buffer))
                {
                    ERROR_LOG("Failed to stream data to buffer: {} in music file.", buffer);
                    return false;
                }
            }

            // Queue up our loaded (streamed) buffers
            alSourceQueueBuffers(m_id, OPEN_AL_PLUGIN_MUSIC_BUFFER_COUNT, internalData->buffers);
            if (!OpenAL::CheckError())
            {
                ERROR_LOG("Failed to queue up data buffers: for streaming.");
                return false;
            }
        }

        m_inUse = true;
        alSourcePlay(m_id);

        return true;
    }

    void Source::Play()
    {
        std::lock_guard lock(m_mutex);
        if (m_current)
        {
            m_triggerPlay = true;
            m_inUse       = true;
        }
        else
        {
            WARN_LOG("Tried to play but this source currently does not have a AudioSource to play.");
        }
    }

    void Source::Pause()
    {
        ALint state;
        alGetSourcei(m_id, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING)
        {
            alSourcePause(m_id);
        }
    }

    void Source::Resume()
    {
        ALint state;
        alGetSourcei(m_id, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED)
        {
            alSourcePlay(m_id);
        }
    }

    void Source::Stop()
    {
        alSourceStop(m_id);

        // Detach all buffers
        alSourcei(m_id, AL_BUFFER, 0);
        OpenAL::CheckError();

        // Rewind
        alSourceRewind(m_id);

        m_inUse = false;
    }

    void Source::RunWorkerThread()
    {
        INFO_LOG("Starting Audio Source thread.");

        bool stop = false;
        while (!stop)
        {
            {
                std::lock_guard threadGuard(m_mutex);

                if (m_triggerExit)
                {
                    break;
                }
                if (m_triggerPlay)
                {
                    alSourcePlay(m_id);
                    m_triggerPlay = false;
                }
            }

            if (m_current && m_current->GetType() == AudioType::MusicStream)
            {
                UpdateStream();
            }

            Platform::SleepMs(2);
        }

        INFO_LOG("Audio Source thread shutting down.")
    }

    void Source::FreeIfNotInUse(ALuint* buffers, ALint* count)
    {
        if (!m_inUse)
        {
            alGetSourcei(m_id, AL_BUFFERS_PROCESSED, count);
            if (OpenAL::CheckError() && *count > 0)
            {
                alSourceUnqueueBuffers(m_id, *count, buffers);
                OpenAL::CheckError();
                return;
            }
        }

        *count = 0;
    }

    bool Source::UpdateStream()
    {
        // Get current state of the source
        ALint sourceState;
        alGetSourcei(m_id, AL_SOURCE_STATE, &sourceState);

        // Sometimes it can be that a source is not currently playing, even with buffers already queued up
        if (sourceState != AL_PLAYING)
        {
            // Handle this case by starting to play
            TRACE("Stream update, play needed for source id: {}", source->id);
            alSourcePlay(m_id);
        }

        // Check for processed buffers that can unqueue
        ALint processedBufferCount = 0;
        alGetSourcei(m_id, AL_BUFFERS_PROCESSED, &processedBufferCount);

        while (processedBufferCount--)
        {
            ALuint bufferId = 0;
            alSourceUnqueueBuffers(m_id, 1, &bufferId);

            // If this returns false, there was nothing further to read (i.e we are at the end of the file)
            if (!StreamMusicData(bufferId))
            {
                auto data = static_cast<AudioData*>(m_current->GetPluginData());
                if (!data->loop)
                {
                    return false;
                }

                m_current->Rewind();

                if (!StreamMusicData(bufferId))
                {
                    return false;
                }
            }

            // Queue up the next buffer
            alSourceQueueBuffers(m_id, 1, &bufferId);
        }

        return true;
    }

    bool Source::StreamMusicData(ALuint bufferId)
    {
        u64 size = m_current->LoadSamples(m_chunkSize);
        if (size == INVALID_ID_U64)
        {
            ERROR_LOG("Failed to stream data.");
            return false;
        }

        // 0 Means that we have reached the end of the file, either the stream will stop or we should restart (when looping)
        if (size == 0)
        {
            return false;
        }

        OpenAL::CheckError();

        void* streamedData = m_current->StreamBufferData();
        if (streamedData)
        {
            alBufferData(bufferId, m_current->GetFormat(), streamedData, size * sizeof(ALshort), m_current->GetSampleRate());
            OpenAL::CheckError();
        }
        else
        {
            ERROR_LOG("Error streaming data.");
            return false;
        }

        // Update the remaining samples (total left - size)
        m_current->SubtractSamples(size);

        return true;
    }
}  // namespace C3D