#pragma once

#include "RenderEngine.h"

#include <array>

class RenderPipeline {
public:
    VkPipelineLayout pipeline_layout_{};
    VkPipeline graphics_pipeline_{};
    VkDescriptorPool descriptor_pool_{};
    VkDescriptorSetLayout descriptor_set_layout_{};

    std::vector<VkFramebuffer> framebuffers_{};
    std::vector<VkBuffer> uniform_buffers_{};
    std::vector<VkDeviceMemory> uniform_buffers_memory_{};
    std::vector<VkDescriptorSet> descriptor_sets_{};

    RenderPipeline(RenderEngine& render_engine, VkVertexInputBindingDescription binding_description, std::vector<VkVertexInputAttributeDescription> attribute_descriptions, uint32_t subpass, uint32_t subpass_count, uint32_t image_sampler_count) :
        render_engine_(render_engine), binding_description_(binding_description), attribute_descriptions_(attribute_descriptions), subpass_(subpass), image_sampler_count_(image_sampler_count) {
    }

    void Initialize(VkShaderModule& vertex_shader_module, VkShaderModule& fragment_shader_module, size_t uniform_buffer_size) {
        this->vertex_shader_module_ = vertex_shader_module;
        this->fragment_shader_module_ = fragment_shader_module;
        this->uniform_buffer_size_ = uniform_buffer_size;
        CreateUniformBuffers();
        CreateDescriptorSetLayout();
        Rebuild();
    }

    void Destroy() {
        Reset();
        vkDestroyDescriptorSetLayout(render_engine_.device_, descriptor_set_layout_, nullptr);
        DestroyUniformBuffers();
        vkDestroyShaderModule(render_engine_.device_, fragment_shader_module_, nullptr);
        vkDestroyShaderModule(render_engine_.device_, vertex_shader_module_, nullptr);
    }

    void Reset() {
        RenderEngine::Log("reseting pipeline");
        vkDestroyPipeline(render_engine_.device_, graphics_pipeline_, nullptr);
        vkDestroyPipelineLayout(render_engine_.device_, pipeline_layout_, nullptr);
        vkDestroyDescriptorPool(render_engine_.device_, descriptor_pool_, nullptr);

        for (auto framebuffer : framebuffers_) {
            vkDestroyFramebuffer(render_engine_.device_, framebuffer, nullptr);
        }
    }

    void Rebuild() {
        RenderEngine::Log("rebuilding pipeline");
        render_engine_.CreateFramebuffers(render_engine_.render_pass_, framebuffers_);
        CreateGraphicsPipeline(render_engine_.swapchain_extent_, render_engine_.render_pass_);
        CreateDescriptorPool();
        CreateDescriptorSets();
    }

    void UpdateDescriptorSet(uint32_t image_index, VkImageView& texture_image_view, VkSampler& texture_sampler) {
        std::vector<VkWriteDescriptorSet> descriptor_writes = {};

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffers_[image_index];
        buffer_info.offset = 0;
        buffer_info.range = uniform_buffer_size_;

        VkWriteDescriptorSet descriptor_set{};
        descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_set.dstSet = descriptor_sets_[image_index];
        descriptor_set.dstBinding = 0;
        descriptor_set.dstArrayElement = 0;
        descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set.descriptorCount = 1;
        descriptor_set.pBufferInfo = &buffer_info;
        descriptor_writes.push_back(descriptor_set);

        for (uint32_t index = 0; index < image_sampler_count_; index++) {
            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = texture_image_view;
            image_info.sampler = texture_sampler;

            VkWriteDescriptorSet descriptor_set{};
            descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_set.dstSet = descriptor_sets_[image_index];
            descriptor_set.dstBinding = 1;
            descriptor_set.dstArrayElement = 0;
            descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_set.descriptorCount = 1;
            descriptor_set.pImageInfo = &image_info;
            descriptor_writes.push_back(descriptor_set);
        }

        vkUpdateDescriptorSets(render_engine_.device_, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
    }

private:
    RenderEngine& render_engine_;
    VkVertexInputBindingDescription binding_description_;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions_;
    uint32_t subpass_;
    uint32_t image_sampler_count_;
    VkShaderModule vertex_shader_module_{};
    VkShaderModule fragment_shader_module_{};
    size_t uniform_buffer_size_{};

    void CreateDescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        VkDescriptorSetLayoutBinding uniform_layout_binding = {};
        uniform_layout_binding.binding = 0;
        uniform_layout_binding.descriptorCount = 1;
        uniform_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_layout_binding.pImmutableSamplers = nullptr;
        uniform_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        bindings.push_back(uniform_layout_binding);

        for (uint32_t index = 0; index < image_sampler_count_; index++) {
            VkDescriptorSetLayoutBinding sampler_layout_binding = {};
            sampler_layout_binding.binding = index + 1;
            sampler_layout_binding.descriptorCount = index + 1;
            sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sampler_layout_binding.pImmutableSamplers = nullptr;
            sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(sampler_layout_binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(render_engine_.device_, &layout_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout");
        }
    }

    void CreateDescriptorPool() {
        std::vector<VkDescriptorPoolSize> pool_sizes{};

        {
            VkDescriptorPoolSize pool_size{};
            pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_size.descriptorCount = static_cast<uint32_t>(render_engine_.image_count_);
            pool_sizes.push_back(pool_size);
        }

        for (uint32_t index = 0; index < image_sampler_count_; index++) {
            VkDescriptorPoolSize pool_size{};
            pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_size.descriptorCount = static_cast<uint32_t>(render_engine_.image_count_);
            pool_sizes.push_back(pool_size);
        }

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(render_engine_.image_count_);

        if (vkCreateDescriptorPool(render_engine_.device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool");
        }
    }

    void CreateDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(render_engine_.image_count_, descriptor_set_layout_);
        VkDescriptorSetAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = descriptor_pool_;
        allocate_info.descriptorSetCount = static_cast<uint32_t>(render_engine_.image_count_);
        allocate_info.pSetLayouts = layouts.data();

        descriptor_sets_.resize(render_engine_.image_count_);
        if (vkAllocateDescriptorSets(render_engine_.device_, &allocate_info, descriptor_sets_.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }
        std::vector<VkWriteDescriptorSet> descriptor_writes = {};

        if (image_sampler_count_ == 0) {
            for (uint32_t image_index = 0; image_index < render_engine_.image_count_; image_index++) {
                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer = uniform_buffers_[image_index];
                buffer_info.offset = 0;
                buffer_info.range = uniform_buffer_size_;

                VkWriteDescriptorSet descriptor_set{};
                descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor_set.dstSet = descriptor_sets_[image_index];
                descriptor_set.dstBinding = 0;
                descriptor_set.dstArrayElement = 0;
                descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptor_set.descriptorCount = 1;
                descriptor_set.pBufferInfo = &buffer_info;
                descriptor_writes.push_back(descriptor_set);
            }

            vkUpdateDescriptorSets(render_engine_.device_, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
        }
    }

    void CreateGraphicsPipeline(VkExtent2D& swapchain_extent, VkRenderPass& render_pass) {
        VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
        vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_stage_info.module = vertex_shader_module_;
        vertex_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {};
        fragment_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_stage_info.module = fragment_shader_module_;
        fragment_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_stage_info, fragment_shader_stage_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions_.size());
        vertex_input_info.pVertexBindingDescriptions = &binding_description_;
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions_.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_extent.width;
        viewport.height = (float)swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapchain_extent;

        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = render_engine_.msaa_samples_;

        VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending = {};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;

        if (vkCreatePipelineLayout(render_engine_.device_, &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout");
        }

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.layout = pipeline_layout_;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = subpass_;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(render_engine_.device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline");
        }
    }

    void CreateUniformBuffers() {
        uniform_buffers_.resize(render_engine_.image_count_);
        uniform_buffers_memory_.resize(render_engine_.image_count_);

        for (size_t i = 0; i < render_engine_.image_count_; i++) {
            render_engine_.CreateBuffer(uniform_buffer_size_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffers_[i], uniform_buffers_memory_[i]);
        }
    }

    void DestroyUniformBuffers() {
        for (size_t i = 0; i < uniform_buffers_.size(); i++) {
            vkDestroyBuffer(render_engine_.device_, uniform_buffers_[i], nullptr);
            vkFreeMemory(render_engine_.device_, uniform_buffers_memory_[i], nullptr);
        }
    }
};
