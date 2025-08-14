
#pragma once
#include "defines.h"

namespace C3D
{
    class C3D_API Clock
    {
    public:
        /** @brief Begin the measured time frame. */
        void Begin();

        /** @brief End the measured time frame. */
        void End();

        /** @brief Reset everything back to 0. */
        void Reset();
        /** @brief Reset total back to 0. */
        void ResetTotal();

        /** @brief Gets the elapsed time in seconds between Start() and End(). */
        [[nodiscard]] f64 GetElapsed() const;
        /** @brief Gets the total elapsed time in seconds between Start() and End(). */
        [[nodiscard]] f64 GetTotalElapsed() const;

        /** @brief Gets the elapsed time in milliseconds between Start() and End(). */
        [[nodiscard]] f64 GetElapsedMs() const;
        /** @brief Gets the total elapsed time in milliseconds between Start() and End(). */
        [[nodiscard]] f64 GetTotalElapsedMs() const;

        /** @brief Gets the elapsed time in microseconds between Start() and End(). */
        [[nodiscard]] f64 GetElapsedUs() const;
        /** @brief Gets the total elapsed time in microseconds between Start() and End(). */
        [[nodiscard]] f64 GetTotalElapsedUs() const;

    private:
        f64 m_elapsedTime      = 0;
        f64 m_totalElapsedTime = 0;

        f64 m_startTime = 0;
    };

    /** @brief A struct containing all clocks that are used by the engine. */
    struct Clocks
    {
        Clock prepareFrame;
        Clock onUpdate;
        Clock prepareRender;
        Clock onRender;
        Clock present;
        Clock total;
    };
}  // namespace C3D
