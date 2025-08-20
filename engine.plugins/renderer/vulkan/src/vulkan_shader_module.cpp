
#include "vulkan_shader_module.h"

#include <assets/managers/shader_manager.h>
#include <shaderc/shaderc.h>
#include <time/scoped_timer.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanShaderModule::Create(VulkanContext* context, const char* name)
    {
        m_name    = name;
        m_context = context;

        ShaderManager ShaderManager;
        ShaderAsset shader;

        INFO_LOG("Loading GLSL file: '{}'.", name);

        if (!ShaderManager.Read(name, shader))
        {
            ERROR_LOG("Failed to read the source for: '{}'.", name);
            return false;
        }

        DetermineShaderStage();

        u32* code     = nullptr;
        u64 byteCount = 0;
        if (!CompileIntoSPIRV(shader.source, shader.size, &code, byteCount))
        {
            ERROR_LOG("Failed to compile GLSL into SPIRV for: '{}'.", name);
            return false;
        }

        ShaderManager::Cleanup(shader);

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize                 = byteCount;
        createInfo.pCode                    = code;

        auto result = vkCreateShaderModule(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_handle);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create shader module: '{}' with error: '{}'.", name, VulkanUtils::ResultString(result));
            if (code)
            {
                Memory.Free(code);
            }
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_SHADER_MODULE, m_handle, String::FromFormat("SHADER_MODULE_{}", name));

        if (code)
        {
            Memory.Free(code);
        }

        INFO_LOG("ShaderModule: '{}' created successfully.", name);
        return true;
    }

    void VulkanShaderModule::Destroy()
    {
        if (m_handle)
        {
            INFO_LOG("Destroying ShaderModule: '{}'.", m_name);
            m_name.Destroy();

            vkDestroyShaderModule(m_context->device.GetLogical(), m_handle, m_context->allocator);
        }
    }

    void VulkanShaderModule::DetermineShaderStage()
    {
        if (m_name.Contains(".vert"))
        {
            m_shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
        }
        else if (m_name.Contains(".frag"))
        {
            m_shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        else if (m_name.Contains(".mesh"))
        {
            m_shaderStage = VK_SHADER_STAGE_MESH_BIT_EXT;
        }
        else if (m_name.Contains(".comp"))
        {
            m_shaderStage = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        else
        {
            C3D_ASSERT_MSG(false, "Could not determine shader stage.");
        }
    }

    static shaderc_shader_kind ShaderStageToShaderKind(VkShaderStageFlags stage)
    {
        switch (stage)
        {
            case VK_SHADER_STAGE_VERTEX_BIT:
                return shaderc_glsl_default_vertex_shader;
            case VK_SHADER_STAGE_FRAGMENT_BIT:
                return shaderc_glsl_default_fragment_shader;
            case VK_SHADER_STAGE_MESH_BIT_EXT:
                return shaderc_glsl_default_mesh_shader;
            case VK_SHADER_STAGE_COMPUTE_BIT:
                return shaderc_glsl_default_compute_shader;
            default:
                C3D_ASSERT_MSG(false, "Could not determine shader kind");
                return shaderc_glsl_vertex_shader;
        }
    }

    bool VulkanShaderModule::CompileIntoSPIRV(const char* source, u64 sourceSize, u32** code, u64& byteCount)
    {
        ScopedTimer timer("Compilation");

        INFO_LOG("Compiling: '{}' into SPIR-V for ShaderModule.", m_name);

        // Set target SPIR-V version
        shaderc_compile_options_t options = shaderc_compile_options_initialize();
        shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_4);

        // Compile the GLSL into SPIR-V
        shaderc_compilation_result_t compilationResult =
            shaderc_compile_into_spv(m_context->shaderCompiler, source, sourceSize, ShaderStageToShaderKind(m_shaderStage), m_name.Data(), "main", options);

        if (!compilationResult)
        {
            ERROR_LOG("Unknown error while trying to compile.");
            return false;
        }

        shaderc_compilation_status status = shaderc_result_get_compilation_status(compilationResult);

        // Handle errors if there are any
        if (status != shaderc_compilation_status_success)
        {
            const char* errorMessage = shaderc_result_get_error_message(compilationResult);
            u64 errorCount           = shaderc_result_get_num_errors(compilationResult);

            ERROR_LOG("Compilation failed with {} error(s).", errorCount);

            // const char* s = source;

            /*
            String line, errorSource;
            u32 index = 0, lineNumber = 1;
            line.Reserve(256);
            errorSource.Reserve(4096);

            while (s != '\0')
            {
                const void* eol = std::memchr(s, '\n', sourceSize);
                if (eol == nullptr)
                {
                    // We could not find another \n
                    break;
                }


            }*/

            ERROR_LOG("Source:\n{}", source);
            ERROR_LOG("Errors:\n{}", errorMessage);

            shaderc_result_release(compilationResult);
            return false;
        }

        // Output warnings if there are any.
        u64 warningCount = shaderc_result_get_num_warnings(compilationResult);
        if (warningCount > 0)
        {
            // NOTE: Not sure if this is the correct way to obtain warnings.
            const char* warnings = shaderc_result_get_error_message(compilationResult);
            WARN_LOG("Compilation found: {} warnings:\n{}", warningCount, warnings);
        }

        // Extract the data from the result.
        const char* bytes = shaderc_result_get_bytes(compilationResult);
        byteCount         = shaderc_result_get_length(compilationResult);

        // Take a copy of the result data and cast it to a u32* as is required by Vulkan.
        *code = Memory.Allocate<u32>(C3D::MemoryType::RenderSystem, byteCount);
        std::memcpy(*code, bytes, byteCount);

        // Release the compilation result.
        shaderc_result_release(compilationResult);

        return true;
    }
}  // namespace C3D