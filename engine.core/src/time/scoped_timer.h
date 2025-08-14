
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    /** @brief A scoped timer that starts on creation and prints the elapsed time on destruction.
     * Requires Platform to be initialized fully in order to work properly.
     */
    class C3D_API ScopedTimer
    {
    public:
        ScopedTimer(const String& scopeName);
        ~ScopedTimer();

    private:
        String m_name;
        f64 m_startTime;
    };
}  // namespace C3D