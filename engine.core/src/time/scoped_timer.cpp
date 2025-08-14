
#include "scoped_timer.h"

#include "platform/platform.h"

namespace C3D
{
    ScopedTimer::ScopedTimer(const String& scopeName) : m_name(scopeName) { m_startTime = Platform::GetAbsoluteTime(); }

    ScopedTimer::~ScopedTimer()
    {
        auto elapsedTime = Platform::GetAbsoluteTime() - m_startTime;
        Logger::Info("[SCOPED_TIMER] {} took {:.6}ms", m_name, elapsedTime * 1000);
    }
}  // namespace C3D