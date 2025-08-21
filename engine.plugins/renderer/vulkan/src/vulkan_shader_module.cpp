
#include "vulkan_shader_module.h"

#include <assets/managers/shader_manager.h>
#include <shaderc/shaderc.h>
#include <spirv-headers/spirv.h>
#include <time/scoped_timer.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    struct ID
    {
        enum Kind
        {
            Unknown,
            Variable
        };

        Kind kind = Unknown;
        u32 type;
        u32 storageClass;
        u32 binding;
        u32 set;
    };

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

        u32* code    = nullptr;
        u64 numBytes = 0;
        if (!CompileIntoSPIRV(shader.source, shader.size, &code, numBytes))
        {
            ERROR_LOG("Failed to compile GLSL into SPIRV for: '{}'.", name);
            return false;
        }

        ShaderManager::Cleanup(shader);

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize                 = numBytes;
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

        // Code size defines the number of u32's (so bytes / 4)
        u64 codeSize = numBytes / 4;
        if (!ReflectSPIRV(code, codeSize))
        {
            ERROR_LOG("Failed to reflect the SPIRV for: '{}'.", name);
            return false;
        }

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

    bool VulkanShaderModule::CompileIntoSPIRV(const char* source, u64 sourceSize, u32** code, u64& numBytes)
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
        numBytes          = shaderc_result_get_length(compilationResult);

        // Take a copy of the result data and cast it to a u32* as is required by Vulkan.
        *code = Memory.Allocate<u32>(C3D::MemoryType::RenderSystem, numBytes / 4);
        std::memcpy(*code, bytes, numBytes);

        // Release the compilation result.
        shaderc_result_release(compilationResult);
        shaderc_compile_options_release(options);

        return true;
    }

    static VkShaderStageFlagBits ExecutionModelToShaderStage(SpvExecutionModel model)
    {
        switch (model)
        {
            case SpvExecutionModelVertex:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case SpvExecutionModelFragment:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            case SpvExecutionModelTaskEXT:
                return VK_SHADER_STAGE_TASK_BIT_EXT;
            case SpvExecutionModelMeshEXT:
                return VK_SHADER_STAGE_MESH_BIT_EXT;
            default:
                C3D_ASSERT_MSG(false, "Unsupported exection model!");
                return VkShaderStageFlagBits(0);
        }
    }

    /** @brief See: https://github.com/KhronosGroup/SPIRV-Guide/blob/main/chapters/parsing_instructions.md for more info */
    bool VulkanShaderModule::ReflectSPIRV(u32* code, u64 codeSize)
    {
        C3D_ASSERT_MSG(code[0] == SpvMagicNumber, "Missing SPV Magic number.");

        u32 idBound = code[3];

        DynamicArray<ID> ids(idBound);

        // First instruction starts at index 5
        u32 offset = 5;
        while (offset < codeSize)
        {
            u32* instruction = code + offset;
            u32 opcode       = instruction[0] & 0xFFFF;
            u32 wordCount    = instruction[0] >> 16;

            C3D_ASSERT(wordCount > 0);

            offset += wordCount;

            switch (opcode)
            {
                case SpvOpEntryPoint:
                {
                    C3D_ASSERT(wordCount >= 2);
                    m_shaderStage = ExecutionModelToShaderStage(SpvExecutionModel(instruction[1]));
                    break;
                }
                case SpvOpDecorate:
                {
                    C3D_ASSERT(wordCount >= 3);

                    u32 id = instruction[1];
                    C3D_ASSERT(id < idBound);

                    switch (instruction[2])
                    {
                        case SpvDecorationDescriptorSet:
                        {
                            C3D_ASSERT(wordCount == 4);
                            ids[id].set = instruction[3];
                            break;
                        }
                        case SpvDecorationBinding:
                        {
                            C3D_ASSERT(wordCount == 4);
                            ids[id].binding = instruction[3];
                            break;
                        }
                    }
                    break;
                }
                case SpvOpVariable:
                {
                    C3D_ASSERT(wordCount >= 4);

                    u32 id = instruction[2];
                    C3D_ASSERT(id < idBound);

                    C3D_ASSERT(ids[id].kind == ID::Unknown);
                    ids[id].kind         = ID::Variable;
                    ids[id].type         = instruction[1];
                    ids[id].storageClass = instruction[3];

                    INFO_LOG("Found id = {} storageClass = {}.", id, ids[id].storageClass);

                    break;
                }
            }
        }

        for (auto& id : ids)
        {
            if (id.kind == ID::Variable && id.storageClass == SpvStorageClassStorageBuffer)
            {
                // Assume that id.type refers to a pointer to a storage buffer
                C3D_ASSERT(id.set == 0);
                C3D_ASSERT(id.binding < 32);
                C3D_ASSERT((m_storageBufferMask & (1 << id.binding)) == 0);

                m_storageBufferMask |= 1 << id.binding;
            }
        }

        return true;
    }
}  // namespace C3D