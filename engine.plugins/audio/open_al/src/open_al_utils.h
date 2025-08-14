
#pragma once
#include <AL/alc.h>

namespace C3D::OpenAL
{
    const char* GetErrorStr(ALCenum error);
    bool CheckError();
}  // namespace C3D::OpenAL