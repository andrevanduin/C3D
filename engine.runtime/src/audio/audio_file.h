
#pragma once
#include "audio_types.h"
#include "defines.h"
#include "minimp3/minimp3_ex.h"
#include "resources/types.h"

struct stb_vorbis;

namespace C3D
{
    enum AudioFileType
    {
        VORBIS,
        MP3
    };

    class C3D_API AudioFile : public IResource
    {
    public:
        AudioFile() : IResource(ResourceType::AudioFile) {}

        u64 LoadSamples(u32 chunkSize);
        void* StreamBufferData();
        void Rewind();

        void SubtractSamples(u64 size) { m_totalSamplesLeft -= size; }

        bool LoadVorbis(AudioType type, u32 chunkSize, const String& path);
        bool LoadMp3(AudioType type, u32 chunkSize, const String& path, mp3dec_t* decoder);

        void Unload();

        void SetFormat(u32 format) { m_format = format; }
        void SetInternalPluginData(void* data) { m_pluginData = data; }

        bool HasSamplesLeft() const { return m_totalSamplesLeft > 0; }

        AudioType GetType() const { return m_type; }
        u32 GetFormat() const { return m_format; }
        u32 GetSampleRate() const { return m_sampleRate; }
        u32 GetTotalSamplesLeft() const { return m_totalSamplesLeft; }

        u8 GetNumChannels() const { return static_cast<u8>(m_channelType); }

        void* GetPluginData() const { return m_pluginData; }

    private:
        AudioType m_type;
        AudioFileType m_fileType;

        u32 m_format;
        u32 m_sampleRate;
        u32 m_totalSamplesLeft;

        stb_vorbis* m_vorbis = nullptr;
        mp3dec_file_info_t m_mp3Info;

        i16* m_pcm    = nullptr;
        u64 m_pcmSize = 0;

        /** @brief A pointer to the internal data used by the specific Audio plugin. */
        void* m_pluginData = nullptr;

        ChannelType m_channelType;
    };
}  // namespace C3D