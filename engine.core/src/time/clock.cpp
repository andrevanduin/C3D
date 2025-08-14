
#include "clock.h"

#include "platform/platform.h"

namespace C3D
{
    void Clock::Begin() { m_startTime = Platform::GetAbsoluteTime(); }

    void Clock::End()
    {
        m_elapsedTime = Platform::GetAbsoluteTime() - m_startTime;
        m_totalElapsedTime += m_elapsedTime;
    }

    void Clock::Reset()
    {
        m_startTime        = 0;
        m_elapsedTime      = 0;
        m_totalElapsedTime = 0;
    }

    void Clock::ResetTotal() { m_totalElapsedTime = 0; }

    f64 Clock::GetElapsed() const { return m_elapsedTime; }

    f64 Clock::GetElapsedMs() const { return m_elapsedTime * 1000; }

    f64 Clock::GetElapsedUs() const { return m_elapsedTime * 1000000; }

    f64 Clock::GetTotalElapsed() const { return m_totalElapsedTime; }

    f64 Clock::GetTotalElapsedMs() const { return m_totalElapsedTime * 1000; }

    f64 Clock::GetTotalElapsedUs() const { return m_totalElapsedTime * 1000000; }
}  // namespace C3D
