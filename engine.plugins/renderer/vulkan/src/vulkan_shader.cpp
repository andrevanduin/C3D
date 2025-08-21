
#include "vulkan_shader.h"

#include "vulkan_context.h"
#include "vulkan_shader_module.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanShader::Create(const VulkanShaderCreateInfo& createInfo)
    {
        INFO_LOG("Creating: '{}'.", createInfo.name);

        m_context = createInfo.context;
        m_name    = createInfo.name;

        if (createInfo.numModules == 0)
        {
            ERROR_LOG("A VulkanShader needs at least one module.");
            return false;
        }

        if (!createInfo.modules)
        {
            ERROR_LOG("No valid ShaderModules provided.");
            return false;
        }

        // Copy over the pointers to the shader modules we are going to be using
        m_numShaderModules = createInfo.numModules;
        m_shaderModules    = Memory.Allocate<VulkanShaderModule*>(MemoryType::Shader, m_numShaderModules);
        std::memcpy(m_shaderModules, createInfo.modules, sizeof(VulkanShaderModule*) * m_numShaderModules);

        // Verify that the passed in ShaderModule stages make sense
        if (m_shaderModules[0]->GetShaderStage() != VK_SHADER_STAGE_VERTEX_BIT && m_shaderModules[0]->GetShaderStage() != VK_SHADER_STAGE_MESH_BIT_EXT)
        {
            ERROR_LOG("Expected first ShaderModule to be of type VERTEX_SHADER or MESH_SHADER.");
            return false;
        }

        if (m_shaderModules[1]->GetShaderStage() != VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            ERROR_LOG("Expected second ShaderModule to be of type FRAGMENT_SHADER.");
            return false;
        }

        if (!CreateSetLayout())
        {
            ERROR_LOG("Failed to create SetLayout.");
            return false;
        }

        if (!CreatePipelineLayout())
        {
            ERROR_LOG("Failed to create PipelineLayout.");
            return false;
        }

        if (!CreateGraphicsPipeline(createInfo.cache, *createInfo.swapchain))
        {
            ERROR_LOG("Failed to create Graphics Pipeline.");
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

    void VulkanShader::PushDescriptorSet(VkCommandBuffer commandBuffer, DescriptorInfo* descriptors) const
    {
        vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, m_updateTemplate, m_layout, 0, descriptors);
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

            if (m_shaderModules)
            {
                Memory.Free(m_shaderModules);
                m_shaderModules    = nullptr;
                m_numShaderModules = 0;
            }
        }
    }

    bool VulkanShader::CreateSetLayout()
    {
        DynamicArray<VkDescriptorSetLayoutBinding> setBindings;

        u32 storageBufferMask = 0;
        for (u32 i = 0; i < m_numShaderModules; ++i)
        {
            storageBufferMask |= m_shaderModules[i]->GetStorageBufferMask();
        }

        for (u32 i = 0; i < 32; ++i)
        {
            if (storageBufferMask & (1 << i))
            {
                VkDescriptorSetLayoutBinding binding = {};

                binding.binding         = i;
                binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                binding.descriptorCount = 1;

                binding.stageFlags = 0;
                for (u32 j = 0; j < m_numShaderModules; ++j)
                {
                    if (m_shaderModules[j]->GetStorageBufferMask() & (1 << i))
                    {
                        binding.stageFlags |= m_shaderModules[j]->GetShaderStage();
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
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create DescriptorSetLayout with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_setLayout, String::FromFormat("DESCRIPTOR_SET_LAYOUT_{}", m_name));

        return true;
    }

    bool VulkanShader::CreatePipelineLayout()
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount             = 1;
        createInfo.pSetLayouts                = &m_setLayout;

        auto result = vkCreatePipelineLayout(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_layout);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create PipelineLayout with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_PIPELINE_LAYOUT, m_layout, String::FromFormat("PIPELINE_LAYOUT_{}", m_name));

        return true;
    }

    bool VulkanShader::CreateGraphicsPipeline(VkPipelineCache pipelineCache, VulkanSwapchain& swapchain)
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        createInfo.layout                       = m_layout;

        VkPipelineShaderStageCreateInfo stages[2] = {};

        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = m_shaderModules[0]->GetShaderStage();
        stages[0].module = m_shaderModules[0]->GetHandle();
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = m_shaderModules[1]->GetShaderStage();
        stages[1].module = m_shaderModules[1]->GetHandle();
        stages[1].pName  = "main";

        createInfo.stageCount = 2;
        createInfo.pStages    = stages;

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
        createInfo.pDepthStencilState                           = &depthStencilState;

        VkPipelineColorBlendAttachmentState colorAttachmentState = {};
        colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.logicOpEnable                       = VK_FALSE;
        colorBlendState.logicOp                             = VK_LOGIC_OP_COPY;
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

        // TODO: Quite bad since this assumes we will always have the same single format as was intially picked for the provided swapchain
        auto format                                         = swapchain.GetImageFormat();
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
        createInfo.pNext                                    = &pipelineRenderingCreateInfo;

        auto result = vkCreateGraphicsPipelines(m_context->device.GetLogical(), pipelineCache, 1, &createInfo, m_context->allocator, &m_pipeline);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create Graphics Pipeline with error: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_PIPELINE, m_pipeline, String::FromFormat("PIPELINE_{}", m_name));

        return true;
    }

    bool VulkanShader::CreateDescriptorUpdateTemplate()
    {
        DynamicArray<VkDescriptorUpdateTemplateEntry> entries;

        u32 storageBufferMask = 0;
        for (u32 i = 0; i < m_numShaderModules; ++i)
        {
            storageBufferMask |= m_shaderModules[i]->GetStorageBufferMask();
        }

        for (u32 i = 0; i < 32; ++i)
        {
            if (storageBufferMask & (1 << i))
            {
                VkDescriptorUpdateTemplateEntry entry = {};

                entry.dstBinding      = i;
                entry.dstArrayElement = 0;
                entry.descriptorCount = 1;
                entry.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                entry.offset          = sizeof(DescriptorInfo) * i;
                entry.stride          = sizeof(DescriptorInfo);

                entries.PushBack(entry);
            }
        }

        VkDescriptorUpdateTemplateCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };

        createInfo.descriptorUpdateEntryCount = entries.Size();
        createInfo.pDescriptorUpdateEntries   = entries.GetData();

        // NOTE: We might need to change this in the future
        createInfo.templateType        = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS;
        createInfo.pipelineBindPoint   = m_bindPoint;
        createInfo.descriptorSetLayout = m_setLayout;
        createInfo.pipelineLayout      = m_layout;

        auto result = vkCreateDescriptorUpdateTemplate(m_context->device.GetLogical(), &createInfo, m_context->allocator, &m_updateTemplate);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create descriptor update template because: '{}'.", VulkanUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, m_updateTemplate,
                                 String::FromFormat("DESCRIPTOR_UPDATE_TEMPLATE_{}", m_name));

        return true;
    }
}  // namespace C3D