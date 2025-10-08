
#include "vulkan_shader.h"

#include "vulkan_context.h"
#include "vulkan_shader_module.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanShader::Create(const VulkanShaderCreateInfo& createInfo)
    {
        INFO_LOG("Creating: '{}'.", createInfo.name);

        m_context   = createInfo.context;
        m_name      = createInfo.name;
        m_bindPoint = createInfo.bindPoint;

        // Ensure the user provided at least 1 module
        if (createInfo.modules.size() == 0)
        {
            ERROR_LOG("A VulkanShader needs at least one module.");
            return false;
        }

        // Take over the modules provided by the user
        m_shaderModules.Reserve(createInfo.modules.size());
        for (auto module : createInfo.modules)
        {
            m_shaderModules.PushBack(module);
        }

        // Parse the provided Shader Modules
        for (auto shader : m_shaderModules)
        {
            // Keep track of all modules that use push constants
            if (shader->UsesPushConstants())
            {
                m_pushConstantStages |= shader->GetShaderStage();
            }
        }

        if (!CreateSetLayout())
        {
            ERROR_LOG("Failed to create SetLayout.");
            return false;
        }

        if (!CreatePipelineLayout(createInfo.pushConstantsSize))
        {
            ERROR_LOG("Failed to create PipelineLayout.");
            return false;
        }

        switch (m_bindPoint)
        {
            case VK_PIPELINE_BIND_POINT_GRAPHICS:
                if (!CreateGraphicsPipeline(createInfo.cache, createInfo.constants))
                {
                    ERROR_LOG("Failed to create Graphics Pipeline.");
                    return false;
                }
                break;
            case VK_PIPELINE_BIND_POINT_COMPUTE:
                if (!CreateComputePipeline(createInfo.cache, createInfo.constants))
                {
                    ERROR_LOG("Failed to create Compute Pipeline.");
                    return false;
                }
                break;
            default:
                ERROR_LOG("Unknown bindpoint specified.");
                return false;
        }

        if (!CreateDescriptorUpdateTemplate())
        {
            ERROR_LOG("Failed to create Descriptor Update Template.");
            return false;
        }

        return true;
    }

    void VulkanShader::Bind(VkCommandBuffer commandBuffer) const { vkCmdBindPipeline(commandBuffer, m_bindPoint, m_pipeline); }

    void VulkanShader::Dispatch(VkCommandBuffer commandBuffer, u32 countX, u32 countY, u32 countZ) const
    {
        const auto dispatchX = (countX + m_localSizeX - 1) / m_localSizeX;
        const auto dispatchY = (countY + m_localSizeY - 1) / m_localSizeY;
        const auto dispatchZ = (countZ + m_localSizeZ - 1) / m_localSizeZ;
        vkCmdDispatch(commandBuffer, dispatchX, dispatchY, dispatchZ);
    }

    void VulkanShader::PushDescriptorSet(VkCommandBuffer commandBuffer, DescriptorInfo* descriptors) const
    {
        vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, m_updateTemplate, m_layout, 0, descriptors);
    }

    void VulkanShader::PushConstants(VkCommandBuffer commandBuffer, const void* data, u64 size) const
    {
        vkCmdPushConstants(commandBuffer, m_layout, m_pushConstantStages, 0, size, data);
    }

    void VulkanShader::Destroy()
    {
        if (m_context)
        {
            INFO_LOG("Destroying: '{}'.", m_name);

            auto device = m_context->device.GetLogical();

            vkDestroyDescriptorUpdateTemplate(device, m_updateTemplate, m_context->allocator);
            vkDestroyDescriptorSetLayout(device, m_setLayout, m_context->allocator);
            vkDestroyPipelineLayout(device, m_layout, m_context->allocator);
            vkDestroyPipeline(device, m_pipeline, m_context->allocator);

            m_shaderModules.Destroy();

            m_name.Destroy();
        }
    }

    u32 VulkanShader::GatherResources(VkDescriptorType (&resourceTypes)[32])
    {
        u32 resourceMask = 0;

        for (auto shader : m_shaderModules)
        {
            auto mask  = shader->GetResourceMask();
            auto types = shader->GetResourceTypes();

            for (u32 i = 0; i < 32; ++i)
            {
                if (mask & (1 << i))
                {
                    if (resourceMask & (1 << i))
                    {
                        C3D_ASSERT(resourceTypes[i] == types[i]);
                    }
                    else
                    {
                        resourceTypes[i] = types[i];
                        resourceMask |= 1 << i;
                    }
                }
            }
        }

        return resourceMask;
    }

    bool VulkanShader::CreateSetLayout()
    {
        DynamicArray<VkDescriptorSetLayoutBinding> setBindings;

        VkDescriptorType resourceTypes[32];
        u32 resourceMask = GatherResources(resourceTypes);

        for (u32 i = 0; i < ARRAY_SIZE(resourceTypes); ++i)
        {
            if (resourceMask & (1 << i))
            {
                VkDescriptorSetLayoutBinding binding = {};

                binding.binding         = i;
                binding.descriptorType  = resourceTypes[i];
                binding.descriptorCount = 1;

                binding.stageFlags = 0;
                for (auto shader : m_shaderModules)
                {
                    if (shader->GetResourceMask() & (1 << i))
                    {
                        binding.stageFlags |= shader->GetShaderStage();
                    }
                }

                setBindings.PushBack(binding);
            }
        }

        VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };

        setCreateInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        setCreateInfo.bindingCount = setBindings.Size();
        setCreateInfo.pBindings    = setBindings.GetData();

        auto result = vkCreateDescriptorSetLayout(m_context->device.GetLogical(), &setCreateInfo, m_context->allocator, &m_setLayout);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create DescriptorSetLayout with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_setLayout, String::FromFormat("DESCRIPTOR_SET_LAYOUT_{}", m_name));

        return true;
    }

    bool VulkanShader::CreatePipelineLayout(u64 pushConstantsSize)
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount             = 1;
        createInfo.pSetLayouts                = &m_setLayout;

        VkPushConstantRange pushConstantRange = {};

        if (pushConstantsSize)
        {
            C3D_ASSERT_MSG(m_pushConstantStages != 0, "PushConstantsSize > 0 but no stages that use push constants were provided to the Shader.");

            pushConstantRange.size       = pushConstantsSize;
            pushConstantRange.stageFlags = m_pushConstantStages;

            createInfo.pushConstantRangeCount = 1;
            createInfo.pPushConstantRanges    = &pushConstantRange;
        }

        auto result = vkCreatePipelineLayout(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_layout);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create PipelineLayout with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_layout, String::FromFormat("PIPELINE_LAYOUT_{}", m_name));

        return true;
    }

    static VkSpecializationInfo FillSpecializationInfo(DynamicArray<VkSpecializationMapEntry>& entries, const std::initializer_list<i32>& constants)
    {
        for (u32 i = 0; i < constants.size(); ++i)
        {
            entries.EmplaceBack(i, i * sizeof(i32), sizeof(i32));
        }

        VkSpecializationInfo result = {};
        result.mapEntryCount        = static_cast<u32>(entries.Size());
        result.pMapEntries          = entries.GetData();
        result.dataSize             = constants.size() * sizeof(i32);
        result.pData                = constants.begin();

        return result;
    }

    bool VulkanShader::CreateGraphicsPipeline(VkPipelineCache pipelineCache, const std::initializer_list<i32>& constants)
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        createInfo.layout                       = m_layout;

        DynamicArray<VkSpecializationMapEntry> specializationEntries;
        VkSpecializationInfo specializationInfo = FillSpecializationInfo(specializationEntries, constants);

        DynamicArray<VkPipelineShaderStageCreateInfo> stages;
        for (auto shader : m_shaderModules)
        {
            VkPipelineShaderStageCreateInfo stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

            stageInfo.stage               = shader->GetShaderStage();
            stageInfo.module              = shader->GetHandle();
            stageInfo.pName               = "main";
            stageInfo.pSpecializationInfo = &specializationInfo;

            stages.PushBack(stageInfo);
        }

        createInfo.stageCount = stages.Size();
        createInfo.pStages    = stages.GetData();

        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        createInfo.pVertexInputState                     = &vertexInput;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        createInfo.pInputAssemblyState                       = &inputAssembly;

        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount                     = 1;
        viewportState.scissorCount                      = 1;
        createInfo.pViewportState                       = &viewportState;

        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.lineWidth                              = 1.f;
        rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.cullMode                               = VK_CULL_MODE_BACK_BIT;
        createInfo.pRasterizationState                            = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multiSampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multiSampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState                          = &multiSampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

        depthStencilState.depthTestEnable  = VK_TRUE;
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthCompareOp   = VK_COMPARE_OP_GREATER;
        createInfo.pDepthStencilState      = &depthStencilState;

        VkPipelineColorBlendAttachmentState colorAttachmentState = {};
        colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.attachmentCount                     = 1;
        colorBlendState.pAttachments                        = &colorAttachmentState;
        createInfo.pColorBlendState                         = &colorBlendState;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount                = 2;
        dynamicState.pDynamicStates                   = dynamicStates;
        createInfo.pDynamicState                      = &dynamicState;

        // NOTE: Because we are using dynamic rendering this can be set to VK_NULL_HANDLE
        createInfo.renderPass         = VK_NULL_HANDLE;
        createInfo.basePipelineHandle = VK_NULL_HANDLE;
        createInfo.basePipelineIndex  = -1;

        // NOTE: Because we are using dynamic rendering we need to provide this structure to pNext of createInfo
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        pipelineRenderingCreateInfo.colorAttachmentCount          = 1;
        // TODO: We are hardcoding the depth format here which we might want to make configurable later
        pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

        auto surfaceFormat = m_context->device.GetPreferredSurfaceFormat();

        pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;
        createInfo.pNext                                    = &pipelineRenderingCreateInfo;

        auto result = vkCreateGraphicsPipelines(m_context->device.GetLogical(), pipelineCache, 1, &createInfo, m_context->allocator, &m_pipeline);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create Graphics Pipeline with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_PIPELINE, m_pipeline, String::FromFormat("PIPELINE_{}", m_name));

        return true;
    }

    bool VulkanShader::CreateComputePipeline(VkPipelineCache pipelineCache, const std::initializer_list<i32>& constants)
    {
        C3D_ASSERT_MSG(m_shaderModules.Size() == 1, "Expected only a single ShaderModule for a Compute pipeline");

        VkComputePipelineCreateInfo createInfo    = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        VkPipelineShaderStageCreateInfo stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

        DynamicArray<VkSpecializationMapEntry> specializationEntries;
        VkSpecializationInfo specializationInfo = FillSpecializationInfo(specializationEntries, constants);

        stageInfo.stage               = m_shaderModules[0]->GetShaderStage();
        stageInfo.module              = m_shaderModules[0]->GetHandle();
        stageInfo.pName               = "main";
        stageInfo.pSpecializationInfo = &specializationInfo;

        createInfo.stage  = stageInfo;
        createInfo.layout = m_layout;

        m_localSizeX = m_shaderModules[0]->GetLocalSizeX();
        m_localSizeY = m_shaderModules[0]->GetLocalSizeY();
        m_localSizeZ = m_shaderModules[0]->GetLocalSizeZ();

        auto result = vkCreateComputePipelines(m_context->device.GetLogical(), pipelineCache, 1, &createInfo, m_context->allocator, &m_pipeline);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create Compute Pipeline with error: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        return true;
    }

    bool VulkanShader::CreateDescriptorUpdateTemplate()
    {
        DynamicArray<VkDescriptorUpdateTemplateEntry> entries;

        VkDescriptorType resourceTypes[32];
        u32 resourceMask = GatherResources(resourceTypes);

        for (u32 i = 0; i < ARRAY_SIZE(resourceTypes); ++i)
        {
            if (resourceMask & (1 << i))
            {
                VkDescriptorUpdateTemplateEntry entry = {};

                entry.dstBinding      = i;
                entry.dstArrayElement = 0;
                entry.descriptorCount = 1;
                entry.descriptorType  = resourceTypes[i];
                entry.offset          = sizeof(DescriptorInfo) * i;
                entry.stride          = sizeof(DescriptorInfo);

                entries.PushBack(entry);
            }
        }

        VkDescriptorUpdateTemplateCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };

        createInfo.descriptorUpdateEntryCount = entries.Size();
        createInfo.pDescriptorUpdateEntries   = entries.GetData();

        // NOTE: We might need to change this in the future
        createInfo.templateType      = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS;
        createInfo.pipelineBindPoint = m_bindPoint;
        createInfo.pipelineLayout    = m_layout;

        auto result = vkCreateDescriptorUpdateTemplate(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_updateTemplate);
        if (!VkUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create descriptor update template because: '{}'.", VkUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, m_updateTemplate,
                                 String::FromFormat("DESCRIPTOR_UPDATE_TEMPLATE_{}", m_name));

        return true;
    }
}  // namespace C3D