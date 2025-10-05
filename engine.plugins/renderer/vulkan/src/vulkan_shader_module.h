
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
        VkShaderStageFlagBits GetShaderStage() const { return m_shaderStage; }

        u32 GetResourceMask() const { return m_resourceMask; }
        const VkDescriptorType* GetResourceTypes() const { return m_resourceTypes; }

        bool UsesPushConstants() const { return m_usePushConstants; }

        u32 GetLocalSizeX() const { return m_localSizeX; }
        u32 GetLocalSizeY() const { return m_localSizeY; }
        u32 GetLocalSizeZ() const { return m_localSizeZ; }

    private:
        /** @brief Determines the shader stage based on the name of the input glsl file. */
        void DetermineShaderStage();

        /** @brief Compiles text (GLSL) input into SPIR-V. */
        bool CompileIntoSPIRV(const char* source, u64 sourceSize, u32** code, u64& codeSize);

        /** @brief Reflect SPIR-V to collect data about the shader. */
        bool ReflectSPIRV(u32* code, u64 numBytes);

        /** @brief The name of the shader module. */
        String m_name;
        /** @brief The type of shader stage. */
        VkShaderStageFlagBits m_shaderStage;
        /** @brief A collection of all the types of descriptors in the module. */
        VkDescriptorType m_resourceTypes[32];
        u32 m_resourceMask = 0;
        /** @brief A boolean indicating if this shader will use push constants. */
        bool m_usePushConstants = false;
        /** @brief The shader's local size. */
        u32 m_localSizeX = 0, m_localSizeY = 0, m_localSizeZ = 0;
        /** @brief A handle to the underlying Vulkan Shader Module. */
        VkShaderModule m_handle = nullptr;
        /** @brief A pointer to our Vulkan context. */
        VulkanContext* m_context = nullptr;
    };
}  // namespace C3D