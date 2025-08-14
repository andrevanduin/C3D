
#include "open_al_utils.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <logger/logger.h>

namespace C3D::OpenAL
{
    const char* GetErrorStr(ALCenum error)
    {
        switch (error)
        {
            case AL_INVALID_VALUE:
                return "AL_INVALID_VALUE";
            case AL_INVALID_NAME:
                return "AL_INVALID_NAME or ALC_INVALID_DEVICE";
            case AL_INVALID_OPERATION:
                return "AL_INVALID_OPERATION";
            case AL_NO_ERROR:
                return "AL_NO_ERROR";
            case AL_OUT_OF_MEMORY:
                return "AL_OUT_OF_MEMORY";
            default:
                return "Unknown/Unhandled error";
        }
    }

    bool CheckError()
    {
        ALCenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            ERROR_LOG("OpenAL error {}: '{}'", error, GetErrorStr(error));
            return false;
        }
        return true;
    }
}  // namespace C3D::OpenAL