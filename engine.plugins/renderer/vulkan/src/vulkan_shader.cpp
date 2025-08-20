
#include "vulkan_shader.h"

#include <assets/managers/binary_manager.h>

#include "vulkan_context.h"
#include "vulkan_utils.h"

namespace C3D
{
    bool VulkanShader::Create(VulkanContext* context, VkPipelineCache pipelineCache, VulkanSwapchain& swapchain, const char* name, const char* vertexShaderName,
                              const char* fragmentShaderName, bool meshShadingEnabled)
    {
        m_context            = context;
        m_meshShadingEnabled = meshShadingEnabled;
        m_name               = name;

        INFO_LOG("Creating: '{}'.", m_name);

        if (!LoadShaderModule(vertexShaderName, m_vertexShaderModule))
        {
            ERROR_LOG("Failed to create ShaderModule.");
            return false;
        }

        if (!LoadShaderModule(fragmentShaderName, m_fragmentShaderModule))
        {
            ERROR_LOG("Failed to create ShaderModule.");
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

        if (!CreateGraphicsPipeline(pipelineCache, swapchain))
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
        INFO_LOG("Destroying: '{}'.", m_name);

        auto device = m_context->device.GetLogical();

        vkDestroyDescriptorUpdateTemplate(device, m_updateTemplate, m_context->allocator);
        vkDestroyDescriptorSetLayout(device, m_setLayout, m_context->allocator);
        vkDestroyPipelineLayout(device, m_layout, m_context->allocator);
        vkDestroyPipeline(device, m_pipeline, m_context->allocator);

        vkDestroyShaderModule(device, m_vertexShaderModule, m_context->allocator);
        vkDestroyShaderModule(device, m_fragmentShaderModule, m_context->allocator);
    }

    bool VulkanShader::LoadShaderModule(const char* name, VkShaderModule& module)
    {
        BinaryManager binaryManager;
        BinaryAsset binary;

        if (!binaryManager.Read(name, binary))
        {
            ERROR_LOG("Failed to read the binary source for: '{}'.", name);
            return false;
        }

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize                 = binary.size;
        createInfo.pCode                    = reinterpret_cast<const u32*>(binary.data);

        auto result = vkCreateShaderModule(m_context->device.GetLogical(), &createInfo, m_context->allocator, &module);
        if (!VulkanUtils::IsSuccess(result))
        {
            ERROR_LOG("Failed to create shader module: '{}' with error: '{}'.", name, VulkanUtils::ResultString(result));
            return false;
        }

        VK_SET_DEBUG_OBJECT_NAME(m_context, VK_OBJECT_TYPE_SHADER_MODULE, module, String::FromFormat("SHADER_MODULE_{}", name));

        BinaryManager::Cleanup(binary);
        return true;
    }

    bool VulkanShader::CreateSetLayout()
    {
        VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        setCreateInfo.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

        if (m_meshShadingEnabled)
        {
            VkDescriptorSetLayoutBinding setBindings[2] = {};
            setBindings[0].binding                      = 0;
            setBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            setBindings[0].descriptorCount              = 1;
            setBindings[0].stageFlags                   = VK_SHADER_STAGE_MESH_BIT_EXT;
            setBindings[1].binding                      = 1;
            setBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            setBindings[1].descriptorCount              = 1;
            setBindings[1].stageFlags                   = VK_SHADER_STAGE_MESH_BIT_EXT;

            setCreateInfo.bindingCount = ARRAY_SIZE(setBindings);
            setCreateInfo.pBindings    = setBindings;
        }
        else
        {
            VkDescriptorSetLayoutBinding setBindings[1] = {};
            setBindings[0].binding                      = 0;
            setBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            setBindings[0].descriptorCount              = 1;
            setBindings[0].stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;

            setCreateInfo.bindingCount = ARRAY_SIZE(setBindings);
            setCreateInfo.pBindings    = setBindings;
        }

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

        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        if (m_meshShadingEnabled)
        {
            stages[0].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
        }
        else
        {
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        }

        stages[0].module = m_vertexShaderModule;
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = m_fragmentShaderModule;
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

        if (m_meshShadingEnabled)
        {
            entries.Resize(2);

            entries[0].dstBinding      = 0;
            entries[0].dstArrayElement = 0;
            entries[0].descriptorCount = 1;
            entries[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            entries[0].offset          = sizeof(DescriptorInfo) * 0;
            entries[0].stride          = sizeof(DescriptorInfo);

            entries[1].dstBinding      = 1;
            entries[1].dstArrayElement = 0;
            entries[1].descriptorCount = 1;
            entries[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            entries[1].offset          = sizeof(DescriptorInfo) * 1;
            entries[1].stride          = sizeof(DescriptorInfo);
        }
        else
        {
            entries.Resize(1);

            entries[0].dstBinding      = 0;
            entries[0].dstArrayElement = 0;
            entries[0].descriptorCount = 1;
            entries[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            entries[0].offset          = sizeof(DescriptorInfo) * 0;
            entries[0].stride          = sizeof(DescriptorInfo);
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