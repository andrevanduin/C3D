
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    /** @brief The Renderer Plugin type. */
    enum class RendererPluginType
    {
        Unknown,
        Vulkan,
        OpenGl,
        DirectX,
    };

    enum RendererConfigFlag : u8
    {
        /** @brief Sync frame rate to monitor refresh rate. */
        FlagVSync = 0x1,
        /** @brief Configure renderer to try to save power wherever possible (useful when on battery power for example). */
        FlagPowerSaving = 0x2,
        /** @brief Configure the renderer to use validation layers (if supported by the backend). */
        FlagValidationLayers = 0x4,
        /** @brief Use PCF filtering. */
        FlagPCF = 0x8,
    };

    enum RendererSupportFlag : u8
    {
        RENDERER_SUPPORT_FLAG_NONE         = 0x0,
        RENDERER_SUPPORT_FLAG_MESH_SHADING = 0x1,
    };

    using RendererConfigFlags = u8;

    struct RendererPluginConfig
    {
        const char* applicationName = nullptr;
        u32 applicationVersion;

        RendererConfigFlags flags;
    };

    struct RenderSystemConfig
    {
        String rendererPlugin;
        RendererConfigFlags flags;
    };

    struct WindowRendererBackendState;

    struct WindowRendererState
    {
        /** @brief A pointer to the renderer backend state. */
        WindowRendererBackendState* backendState = nullptr;
    };

    /** @brief The globals structure that is uploaded to the GPU once per frame.  */
    struct alignas(16) Globals
    {
        mat4 projection;
    };
}  // namespace C3D