
#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <audio/audio_file.h>
#include <math/math_types.h>

#include <mutex>

namespace C3D
{
    class Source
    {
    public:
        Source() = default;

        bool Create(u32 chunkSize);
        void Destroy();

        bool SetDefaults(bool resetUse);

        bool SetGain(f32 gain);
        bool SetPitch(f32 pitch);
        bool SetPosition(const vec3& position);
        bool SetLoop(bool loop);

        bool Play(AudioFile& audio);

        void Play();
        void Pause();
        void Resume();
        void Stop();

        void RunWorkerThread();

        /** @brief Frees the Source's internal buffers if they are not in use.
         * Returns the amount of buffers freed and the corresponding buffer id's.
         **/
        void FreeIfNotInUse(ALuint* buffers, ALint* count);

        ALCuint GetId() const { return m_id; }
        f32 GetGain() const { return m_gain; }
        f32 GetPitch() const { return m_pitch; }
        vec3 GetPosition() const { return m_position; }
        bool GetLoop() const { return m_loop; }

    private:
        bool UpdateStream();
        bool StreamMusicData(ALuint bufferId);

        /** @brief Internal OpenAL source. */
        ALCuint m_id = INVALID_ID;
        /** @brief Audio chunk size. */
        u32 m_chunkSize = 0;
        /** @brief Volume */
        f32 m_gain = 1.0f;
        /** @brief Pitch for the source (generally left at 1.0f). */
        f32 m_pitch = 1.0f;
        /** @brief Position of the sound. */
        vec3 m_position = vec3(0.0f);
        /** @brief Indicates if the source is looping. */
        bool m_loop = false;
        /** @brief Indicates if the source is in use. */
        bool m_inUse = false;
        /** @brief Worker thread for this source. */
        std::thread m_thread;
        /** @brief Mutex for locking this source for data access. */
        std::mutex m_mutex;
        /** @brief Current piece of audio that this source is using. */
        AudioFile* m_current = nullptr;
        bool m_triggerPlay   = false;
        bool m_triggerExit   = false;
    };
}  // namespace C3D