
#include "audio_file.h"

#define MINIMP3_IMPLEMENTATION
#include <minimp3/minimp3_ex.h>
#include <stb/stb_vorbis.h>

#include "logger/logger.h"
#include "math/c3d_math.h"
#include "memory/global_memory_system.h"
#include "string/string.h"

namespace C3D
{
    u64 AudioFile::LoadSamples(u32 chunkSize)
    {
        switch (m_fileType)
        {
            case VORBIS:
            {
                u8 channels = ToUnderlying(m_channelType);
                i32 samples = stb_vorbis_get_samples_short_interleaved(m_vorbis, channels, m_pcm, chunkSize);
                // The samples here do not include the channels, so let's factor them in
                return samples * channels;
            }
            case MP3:
            {
                // The samples here include channels.
                return Min(m_totalSamplesLeft, chunkSize);
            }
            default:
                ERROR_LOG("Error loading samples: 'Unknown file type'.");
                return 0;
        }
    }

    void* AudioFile::StreamBufferData()
    {
        switch (m_fileType)
        {
            case VORBIS:
                return m_pcm;
            case MP3:
                return m_mp3Info.buffer + (m_mp3Info.samples - m_totalSamplesLeft);
            default:
                ERROR_LOG("Error streaming buffer data: 'Unknown file type'.");
                return 0;
        }
    }

    void AudioFile::Rewind()
    {
        switch (m_fileType)
        {
            case VORBIS:
            {
                u8 channels = ToUnderlying(m_channelType);
                stb_vorbis_seek_start(m_vorbis);
                // Reset sample counter
                m_totalSamplesLeft = stb_vorbis_stream_length_in_samples(m_vorbis) * channels;
                break;
            }
            case MP3:
            {
                // Reset sample counter
                m_totalSamplesLeft = m_mp3Info.samples;
                break;
            }
            default:
                ERROR_LOG("Error rewinding: 'Unknown file type'.");
                break;
        }
    }

    bool AudioFile::LoadVorbis(AudioType type, u32 chunkSize, const String& path)
    {
        m_type     = type;
        m_fileType = AudioFileType::VORBIS;

        i32 error = 0;

        m_vorbis = stb_vorbis_open_filename(path.Data(), &error, nullptr);
        if (!m_vorbis)
        {
            switch (error)
            {
                case VORBIS_file_open_failure:  // fopen() failed
                    ERROR_LOG("Error while trying to open file: '{}'.", path);
                    break;
                case VORBIS_unexpected_eof:  // file is truncated?
                    ERROR_LOG("Unexpected end of file in: '{}'.", path);
                    break;
                default:
                    ERROR_LOG("Unknown error occured while trying to load.");
                    break;
            }
            return false;
        }

        stb_vorbis_info info = stb_vorbis_get_info(m_vorbis);
        m_channelType        = static_cast<ChannelType>(info.channels);
        m_sampleRate         = info.sample_rate;
        m_totalSamplesLeft   = stb_vorbis_stream_length_in_samples(m_vorbis) * info.channels;

        if (m_type == AudioType::MusicStream)
        {
            // Allocate space for a buffer to read from
            m_pcm     = Memory.Allocate<i16>(MemoryType::AudioType, chunkSize);
            m_pcmSize = chunkSize * sizeof(i16);
            return true;
        }
        else if (m_type == AudioType::SoundEffect)
        {
            // Allocate space for a buffer for the entire size to read from
            m_pcm     = Memory.Allocate<i16>(MemoryType::AudioType, m_totalSamplesLeft);
            m_pcmSize = sizeof(i16) * m_totalSamplesLeft;

            i32 readSamples = stb_vorbis_get_samples_short_interleaved(m_vorbis, info.channels, m_pcm, m_totalSamplesLeft);
            if (readSamples != m_totalSamplesLeft)
            {
                WARN_LOG("Read Bytes: {} does not match TotalSamplesLeft: {}. This might cause playback issues.");
            }

            // Ensure our total samples left is always a multiple of 4 to avoid issues
            m_totalSamplesLeft += (m_totalSamplesLeft % 4);
            return true;
        }

        ERROR_LOG("AudioType is unknown.");
        return false;
    }

    bool AudioFile::LoadMp3(AudioType type, u32 chunkSize, const String& path, mp3dec_t* decoder)
    {
        m_type     = type;
        m_fileType = AudioFileType::MP3;

        i32 status = mp3dec_load(decoder, path.Data(), &m_mp3Info, nullptr, nullptr);
        if (status == 0)
        {
            DEBUG_LOG("mp3 freq: {}Hz, avg kbit/s rate: {}.", m_mp3Info.hz, m_mp3Info.avg_bitrate_kbps);

            m_channelType      = static_cast<ChannelType>(m_mp3Info.channels);
            m_sampleRate       = m_mp3Info.hz;
            m_totalSamplesLeft = m_mp3Info.samples;

            return true;
        }

        ERROR_LOG("Failed to load mp3 file: '{}' with error code: {}.", path, status);

        return false;
    }

    void AudioFile::Unload()
    {
        if (m_vorbis)
        {
            stb_vorbis_close(m_vorbis);
        }

        if (m_mp3Info.buffer)
        {
            // TODO: Is this correct??
            free(m_mp3Info.buffer);
        }

        if (m_pcm)
        {
            Memory.Free(m_pcm);
            m_pcm     = nullptr;
            m_pcmSize = 0;
        }
    }

}  // namespace C3D