
#pragma once
#include "defines.h"

namespace C3D
{
    struct TimeData
    {
        /** @brief The time in seconds since the last frame. */
        f64 delta = 0.0;
        /** @brief The total amount of time in milliseconds that the application has been running. */
        f64 total = 0.0;
        /** @brief The average time it takes to run the engine Run() method in milliseconds. */
        f64 avgRunTimeMs = 0.0;
        /** @brief The average time it takes to run OnUpdate() in milliseconds. */
        f64 avgUpdateTimeMs = 0.0;
        /** @brief The average time it takes to run PrepareFrame() in milliseconds. */
        f64 avgPrepareFrameTimeMs = 0.0;
        /** @brief The average time it takes to run OnRender() in milliseconds. */
        f64 avgRenderTimeMs = 0.0;
        /** @brief The average time it takes to run PresentFrame() in milliseconds. */
        f64 avgPresentTimeMs = 0.0;
    };

    struct FrameData
    {
        /** @brief A struct containing all timing related data. */
        TimeData timeData;
        /** @brief The number of meshes drawn in the last frame. */
        u32 drawnMeshCount = 0;
        /** @brief The number of terrain meshes drawn in the last frame. */
        u32 drawnTerrainCount = 0;
        /** @brief The number of shadow meshes drawn in the last frame. */
        u32 drawnShadowMeshCount = 0;
        /** @brief The number of debug meshes drawn in the last frame. */
        u32 drawnDebugCount = 0;
        /** @brief The current frame number, typically used for data synchronization. */
        u64 frameNumber = INVALID_ID_U64;
        /** @brief The current draw index for this frame. Used to track queue submissions for this frame. */
        u8 drawIndex = INVALID_ID_U8;
        /** @brief The current render target index for renderers that use multiple render targets at once. */
        u64 renderTargetIndex = INVALID_ID_U64;
    };
}  // namespace C3D