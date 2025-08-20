
#pragma once
#include <defines.h>
#include <string/string.h>
#include <volk.h>

namespace C3D
{
    struct VulkanContext;

    enum ShaderModuleFlag : u8
    {

    };
    using ShaderModuleFlags = u8;

    class VulkanShaderModule
    {
    public:
        bool Create(VulkanContext* context, const char* name);

        void Destroy();

        VkShaderModule GetHandle() const { return m_handle; }

    private:
        /** @brief Determines the shader stage based on the name of the input glsl file. */
        void DetermineShaderStage();

        /** @brief Compiles text (GLSL) input into SPIR-V. */
        bool CompileIntoSPIRV(const char* source, u64 sourceSize, u32** code, u64& byteCount);

        String m_name;

        VkShaderStageFlags m_shaderStage;

        VkShaderModule m_handle  = nullptr;
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D