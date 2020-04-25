#pragma once

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

struct TextureSampler {
    VkSampler texture_sampler_{};
    VkImageView texture_image_view_{};
    VkImage texture_image_{};
    VkDeviceMemory texture_image_memory_{};
};

struct IndexedPrimitive {
    VkBuffer vertex_buffer_{};
    VkDeviceMemory vertex_buffer_memory_{};
    VkBuffer index_buffer_{};
    VkDeviceMemory index_buffer_memory_{};
    uint32_t index_count_{};
};

class RenderApplication {
public:
    virtual void GetRequiredExtensions(std::vector<const char*>& required_extensions) = 0;
    virtual void CreateSurface(VkInstance& instance, VkSurfaceKHR& surface) = 0;
    virtual void GetDrawableSize(int& window_width, int& window_height) = 0;
    virtual void PipelineReset() = 0;
    virtual void PipelineRebuild() = 0;
};

class RenderEngine {
public:
    struct UniformBuffer {
        uint32_t size_{};
        std::vector<VkBuffer> buffers{};
        std::vector<VkDeviceMemory> memories{};
    };

    struct DescriptorSet {
        std::vector<std::shared_ptr<UniformBuffer>> uniform_buffers{};
        uint32_t image_sampler_count{};
        VkDescriptorSetLayout descriptor_set_layout{};
        VkDescriptorPool descriptor_pool{};
        std::vector<VkDescriptorSet> descriptor_sets{};
    };

    struct GraphicsPipeline {
        VkShaderModule vertex_shader_module{};
        VkShaderModule fragment_shader_module{};
        uint32_t push_constant_size_fragment{};
        VkVertexInputBindingDescription binding_description{};
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};
        std::shared_ptr<DescriptorSet> descriptor_set{};
        uint32_t subpass{};
        bool use_depth{};
        bool use_alpha{};
        VkPipelineLayout pipeline_layout{};
        VkPipeline graphics_pipeline{};
    };

    VkPhysicalDeviceLimits limits_;
    VkDevice device_ = nullptr;
    VkExtent2D swapchain_extent_{};
    std::vector<VkCommandBuffer> command_buffers_{};
    VkRenderPass render_pass_{};
    std::vector<VkFramebuffer> framebuffers_{};

    RenderEngine(uint32_t subpass_count) : subpass_count_(subpass_count) {}

    void Initialize(RenderApplication* render_application) {
#ifdef _DEBUG
        debug_layers_ = true;
#endif
        render_application_ = render_application;
        std::vector<const char*> required_extensions{};
        render_application_->GetRequiredExtensions(required_extensions);
        CreateInstance(required_extensions);
        if (debug_layers_) {
            SetupDebugMessenger();
        }
        render_application_->CreateSurface(instance_, surface_);
        PickPhysicalDevice();
        msaa_samples_ = GetMaxUsableSampleCount();
        CreateLogicalDevice();
        CreateCommandPool();
        int window_width;
        int window_height;
        render_application_->GetDrawableSize(window_width, window_height);
        ChooseSwapExtent(window_width, window_height);
        image_count_ = capabilities_.minImageCount > 2 ? capabilities_.minImageCount : 2;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
        QuerySwapChainSupport(physical_device_, capabilities_, formats, present_modes);
        if (capabilities_.maxImageCount > 0 && image_count_ > capabilities_.maxImageCount) {
            image_count_ = capabilities_.maxImageCount;
        }
        surface_format_ = ChooseSwapSurfaceFormat(formats);
        present_mode_ = ChooseSwapPresentMode(present_modes);
        depth_format_ = FindDepthFormat();
        CreateSwapchain(window_width, window_height);
        CreateRenderPass();
        CreateFramebuffers(render_pass_, framebuffers_);
        CreateSyncObjects();
    }

    void Destroy() {
        for (size_t i = 0; i < max_frames_in_flight_; i++) {
            vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
        }
        for (auto framebuffer : framebuffers_) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        DestroySwapchain();
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (debug_layers_) {
            DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
        }
        vkDestroyInstance(instance_, nullptr);
    }

    void RebuildSwapchain() {
        vkDeviceWaitIdle(device_);
        render_application_->PipelineReset();
        int window_width;
        int window_height;
        render_application_->GetDrawableSize(window_width, window_height);
        if (swapchain_extent_.width != window_width || swapchain_extent_.height != window_height) {
            for (auto framebuffer : framebuffers_) {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
            DestroySwapchain();
            CreateSwapchain(window_width, window_height);
            CreateFramebuffers(render_pass_, framebuffers_);
        }
        render_application_->PipelineRebuild();
    }

    bool AcquireNextImage(uint32_t& image_index) {
        vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RebuildSwapchain();
            return false;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        return true;
    }

    void SubmitDrawCommands(uint32_t image_index) {
        if (images_in_flight_[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(device_, 1, &images_in_flight_[image_index], VK_TRUE, UINT64_MAX);
        }
        images_in_flight_[image_index] = in_flight_fences_[current_frame_];

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {image_available_semaphores_[current_frame_]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers_[image_index];

        VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame_]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer");
        }
    }

    void PresentImage(uint32_t image_index) {
        VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame_]};
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;
        VkSwapchainKHR swap_chains[] = {swapchain_};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;

        VkResult result = vkQueuePresentKHR(present_queue_, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            RebuildSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        current_frame_ = (current_frame_ + 1) % max_frames_in_flight_;
    }

    std::shared_ptr<UniformBuffer> CreateUniformBuffer(uint32_t buffer_size) {
        std::shared_ptr<UniformBuffer> uniform_buffer = std::make_shared<UniformBuffer>();
        uniform_buffer->size_ = buffer_size;
        uniform_buffer->buffers.resize(image_count_);
        uniform_buffer->memories.resize(image_count_);
        for (size_t i = 0; i < image_count_; i++) {
            CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniform_buffer->buffers[i], uniform_buffer->memories[i]);
        }
        return uniform_buffer;
    }

    void DestroyUniformBuffer(std::shared_ptr<UniformBuffer>& uniform_buffer) {
        for (size_t i = 0; i < uniform_buffer->buffers.size(); i++) {
            vkDestroyBuffer(device_, uniform_buffer->buffers[i], nullptr);
            vkFreeMemory(device_, uniform_buffer->memories[i], nullptr);
        }
        uniform_buffer.reset();
    }

    void UpdateUniformBuffer(std::shared_ptr<UniformBuffer>& uniform_buffer, uint32_t image_index, void* data) {
        void* memory;
        vkMapMemory(device_, uniform_buffer->memories[image_index], 0, uniform_buffer->size_, 0, &memory);
        memcpy(memory, data, uniform_buffer->size_);
        vkUnmapMemory(device_, uniform_buffer->memories[image_index]);
    }

    void UpdateUniformBuffers(std::shared_ptr<UniformBuffer>& uniform_buffer, void* data) {
        void* memory;
        for (uint32_t image_index = 0; image_index < image_count_; image_index++) {
            vkMapMemory(device_, uniform_buffer->memories[image_index], 0, uniform_buffer->size_, 0, &memory);
            memcpy(memory, data, uniform_buffer->size_);
            vkUnmapMemory(device_, uniform_buffer->memories[image_index]);
        }
    }

    std::shared_ptr<DescriptorSet> CreateDescriptorSet(std::vector<std::shared_ptr<UniformBuffer>> uniform_buffers, uint32_t image_sampler_count) {
        std::shared_ptr<DescriptorSet> descriptor_set = std::make_shared<DescriptorSet>();

        descriptor_set->uniform_buffers = uniform_buffers;
        descriptor_set->image_sampler_count = image_sampler_count;

        uint32_t binding = 0;

        std::vector<VkDescriptorSetLayoutBinding> bindings;

        for (auto& uniform_buffer : uniform_buffers) {
            VkDescriptorSetLayoutBinding uniform_layout_binding = {};
            uniform_layout_binding.binding = binding++;
            uniform_layout_binding.descriptorCount = 1;
            uniform_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniform_layout_binding.pImmutableSamplers = nullptr;
            uniform_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            bindings.push_back(uniform_layout_binding);
        }

        for (uint32_t index = 0; index < image_sampler_count; index++) {
            VkDescriptorSetLayoutBinding sampler_layout_binding = {};
            sampler_layout_binding.binding = binding++;
            sampler_layout_binding.descriptorCount = 1;
            sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sampler_layout_binding.pImmutableSamplers = nullptr;
            sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(sampler_layout_binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set->descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout");
        }

        std::vector<VkDescriptorPoolSize> pool_sizes{};

        for (auto& uniform_buffer : uniform_buffers) {
            VkDescriptorPoolSize pool_size{};
            pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_size.descriptorCount = static_cast<uint32_t>(image_count_);
            pool_sizes.push_back(pool_size);
        }

        for (uint32_t index = 0; index < image_sampler_count; index++) {
            VkDescriptorPoolSize pool_size{};
            pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_size.descriptorCount = static_cast<uint32_t>(image_count_);
            pool_sizes.push_back(pool_size);
        }

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(image_count_);

        if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_set->descriptor_pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool");
        }

        std::vector<VkDescriptorSetLayout> layouts(image_count_, descriptor_set->descriptor_set_layout);

        VkDescriptorSetAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = descriptor_set->descriptor_pool;
        allocate_info.descriptorSetCount = static_cast<uint32_t>(image_count_);
        allocate_info.pSetLayouts = layouts.data();

        descriptor_set->descriptor_sets.resize(image_count_);

        if (vkAllocateDescriptorSets(device_, &allocate_info, descriptor_set->descriptor_sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        std::vector<VkWriteDescriptorSet> descriptor_writes = {};

        if (image_sampler_count == 0) {
            uint32_t uniform_buffer_count = static_cast<uint32_t>(uniform_buffers.size());
            VkDescriptorBufferInfo* buffer_info = new VkDescriptorBufferInfo[image_count_ * uniform_buffer_count];

            for (uint32_t image_index = 0; image_index < image_count_; image_index++) {            
                for (uint32_t uniform_buffer_index = 0; uniform_buffer_index < uniform_buffer_count; uniform_buffer_index++) {
                    buffer_info[image_index * uniform_buffer_count + uniform_buffer_index].buffer = uniform_buffers[uniform_buffer_index]->buffers[image_index];
                    buffer_info[image_index * uniform_buffer_count + uniform_buffer_index].offset = 0;
                    buffer_info[image_index * uniform_buffer_count + uniform_buffer_index].range = uniform_buffers[uniform_buffer_index]->size_;
                }

                VkWriteDescriptorSet write_descriptor_set{};
                write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write_descriptor_set.dstSet = descriptor_set->descriptor_sets[image_index];
                write_descriptor_set.dstBinding = 0;
                write_descriptor_set.dstArrayElement = 0;
                write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write_descriptor_set.descriptorCount = uniform_buffer_count;
                write_descriptor_set.pBufferInfo = buffer_info + (image_index * uniform_buffer_count);
                descriptor_writes.push_back(write_descriptor_set);
            }

            vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);

            delete[] buffer_info;
        }

        return descriptor_set;
    }

    void DestroyDescriptorSet(std::shared_ptr<DescriptorSet>& descriptor_set) {
        vkDestroyDescriptorPool(device_, descriptor_set->descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device_, descriptor_set->descriptor_set_layout, nullptr);
        descriptor_set.reset();
    }

    void UpdateDescriptorSet(std::shared_ptr<DescriptorSet>& descriptor_set, uint32_t image_index, std::vector<TextureSampler> textures) {
        std::vector<VkWriteDescriptorSet> descriptor_writes = {};

        uint32_t binding = 0;

        uint32_t uniform_buffer_count = static_cast<uint32_t>(descriptor_set->uniform_buffers.size());
        VkDescriptorBufferInfo* buffer_info = new VkDescriptorBufferInfo[uniform_buffer_count];

        for (uint32_t uniform_buffer_index = 0; uniform_buffer_index < uniform_buffer_count; uniform_buffer_index++) {
            buffer_info[image_index * uniform_buffer_count + uniform_buffer_index].buffer = descriptor_set->uniform_buffers[uniform_buffer_index]->buffers[image_index];
            buffer_info[image_index * uniform_buffer_count + uniform_buffer_index].offset = 0;
            buffer_info[image_index * uniform_buffer_count + uniform_buffer_index].range = descriptor_set->uniform_buffers[uniform_buffer_index]->size_;
        }

        VkWriteDescriptorSet write_descriptor_set{};
        write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_set.dstSet = descriptor_set->descriptor_sets[image_index];
        write_descriptor_set.dstBinding = binding;
        write_descriptor_set.dstArrayElement = 0;
        write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write_descriptor_set.descriptorCount = uniform_buffer_count;
        write_descriptor_set.pBufferInfo = buffer_info;

        if (uniform_buffer_count > 0) {
            descriptor_writes.push_back(write_descriptor_set);
            binding++;
        }

        uint32_t image_sampler_count = static_cast<uint32_t>(descriptor_set->image_sampler_count);
        VkDescriptorImageInfo* descriptor_images = new VkDescriptorImageInfo[image_sampler_count];

        for (uint32_t index = 0; index < image_sampler_count; index++) {
            descriptor_images[index] = {};
            descriptor_images[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptor_images[index].imageView = textures[index].texture_image_view_;
            descriptor_images[index].sampler = textures[index].texture_sampler_;

            VkWriteDescriptorSet write_descriptor_set{};
            write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_descriptor_set.dstSet = descriptor_set->descriptor_sets[image_index];
            write_descriptor_set.dstBinding = binding++;
            write_descriptor_set.dstArrayElement = 0;
            write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_descriptor_set.descriptorCount = 1;
            write_descriptor_set.pImageInfo = &descriptor_images[index];
            descriptor_writes.push_back(write_descriptor_set);
        }

        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);

        delete[] descriptor_images;
        delete[] buffer_info;
    }

    void UpdateDescriptorSets(std::shared_ptr<DescriptorSet>& descriptor_set, std::vector<TextureSampler> textures) {
        for (uint32_t image_index = 0; image_index < image_count_; image_index++) {
            UpdateDescriptorSet(descriptor_set, image_index, textures);
        }
    }

    VkShaderModule CreateShaderModule(const unsigned char* byte_code, size_t byte_code_length) {
        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = byte_code_length;
        create_info.pCode = reinterpret_cast<const uint32_t*>(byte_code);

        VkShaderModule shader_module;
        if (vkCreateShaderModule(device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module");
        }

        return shader_module;
    }

    std::shared_ptr<GraphicsPipeline> CreateGraphicsPipeline
    (
        VkShaderModule& vertex_shader_module,
        VkShaderModule& fragment_shader_module,
        uint32_t push_constant_size_fragment,
        VkVertexInputBindingDescription binding_description,
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions,
        std::shared_ptr<DescriptorSet>& descriptor_set,
        uint32_t subpass,
        bool use_depth,
        bool use_alpha
    ) {
        std::shared_ptr<GraphicsPipeline> graphics_pipeline = std::make_shared<GraphicsPipeline>();

        graphics_pipeline->vertex_shader_module = vertex_shader_module;
        graphics_pipeline->fragment_shader_module = fragment_shader_module;
        graphics_pipeline->push_constant_size_fragment = push_constant_size_fragment;
        graphics_pipeline->binding_description = binding_description;
        graphics_pipeline->attribute_descriptions = attribute_descriptions;
        graphics_pipeline->descriptor_set = descriptor_set;
        graphics_pipeline->subpass = subpass;
        graphics_pipeline->use_depth = use_depth;
        graphics_pipeline->use_alpha = use_alpha;

        RebuildGraphicsPipeline(graphics_pipeline);

        return graphics_pipeline;
    }

    void RebuildGraphicsPipeline(std::shared_ptr<GraphicsPipeline>& graphics_pipeline) {
        VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
        vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_stage_info.module = graphics_pipeline->vertex_shader_module;
        vertex_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {};
        fragment_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_stage_info.module = graphics_pipeline->fragment_shader_module;
        fragment_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_stage_info, fragment_shader_stage_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(graphics_pipeline->attribute_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = &graphics_pipeline->binding_description;
        vertex_input_info.pVertexAttributeDescriptions = graphics_pipeline->attribute_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_extent_.width;
        viewport.height = (float)swapchain_extent_.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapchain_extent_;

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
        multisampling.rasterizationSamples = msaa_samples_;

        VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (graphics_pipeline->use_depth) {
            depth_stencil.depthTestEnable = VK_TRUE;
            depth_stencil.depthWriteEnable = VK_TRUE;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.stencilTestEnable = VK_FALSE;
        } else {
            depth_stencil.depthTestEnable = VK_FALSE;
            depth_stencil.depthWriteEnable = VK_FALSE;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.stencilTestEnable = VK_FALSE;
        }

        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (graphics_pipeline->use_alpha) {
            color_blend_attachment.blendEnable = VK_TRUE;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        } else {
            color_blend_attachment.blendEnable = VK_FALSE;
        }

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

        std::vector<VkPushConstantRange>push_constant_ranges;
        if (graphics_pipeline->push_constant_size_fragment > 0) {
            VkPushConstantRange push_constant_range;
            push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            push_constant_range.offset = 0;
            push_constant_range.size = graphics_pipeline->push_constant_size_fragment;
            push_constant_ranges.push_back(push_constant_range);
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &graphics_pipeline->descriptor_set->descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
        pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

        if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &graphics_pipeline->pipeline_layout) != VK_SUCCESS) {
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
        pipeline_info.layout = graphics_pipeline->pipeline_layout;
        pipeline_info.renderPass = render_pass_;
        pipeline_info.subpass = graphics_pipeline->subpass;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline->graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline");
        }
    }

    void ResetGraphicsPipeline(std::shared_ptr<GraphicsPipeline>& graphics_pipeline) {
        vkDestroyPipeline(device_, graphics_pipeline->graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(device_, graphics_pipeline->pipeline_layout, nullptr);
    }

    void DestroyGraphicsPipeline(std::shared_ptr<GraphicsPipeline>& graphics_pipeline) {
        ResetGraphicsPipeline(graphics_pipeline);
        vkDestroyShaderModule(device_, graphics_pipeline->fragment_shader_module, nullptr);
        vkDestroyShaderModule(device_, graphics_pipeline->vertex_shader_module, nullptr);
        graphics_pipeline.reset();
    }

    void CreateTexture(unsigned char* pixels, int texWidth, int texHeight, TextureSampler& texture_sampler) {
        VkDeviceSize image_size = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;
        uint32_t mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        CreateBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(device_, staging_buffer_memory, 0, image_size, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(image_size));
        vkUnmapMemory(device_, staging_buffer_memory);

        CreateImage(texWidth, texHeight, mip_levels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_sampler.texture_image_, texture_sampler.texture_image_memory_);

        TransformImageLayout(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels);
        CopyBufferToImage(staging_buffer, texture_sampler.texture_image_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_buffer_memory, nullptr);

        GenerateMipmaps(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mip_levels);

        texture_sampler.texture_image_view_ = CreateImageView(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels);

        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.minLod = 0;
        sampler_info.maxLod = static_cast<float>(mip_levels);
        sampler_info.mipLodBias = 0;

        if (vkCreateSampler(device_, &sampler_info, nullptr, &texture_sampler.texture_sampler_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler");
        }
    }

    void CreateAlphaTexture(unsigned char* pixels, int texWidth, int texHeight, TextureSampler& texture_sampler) {
        VkDeviceSize image_size = static_cast<VkDeviceSize>(texWidth) * texHeight;

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        CreateBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(device_, staging_buffer_memory, 0, image_size, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(image_size));
        vkUnmapMemory(device_, staging_buffer_memory);

        CreateImage(texWidth, texHeight, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_sampler.texture_image_, texture_sampler.texture_image_memory_);

        TransformImageLayout(texture_sampler.texture_image_, VK_FORMAT_R8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
        CopyBufferToImage(staging_buffer, texture_sampler.texture_image_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_buffer_memory, nullptr);

        GenerateMipmaps(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, 1);

        texture_sampler.texture_image_view_ = CreateImageView(texture_sampler.texture_image_, VK_FORMAT_R8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.minLod = 0;
        sampler_info.maxLod = 1;
        sampler_info.mipLodBias = 0;

        if (vkCreateSampler(device_, &sampler_info, nullptr, &texture_sampler.texture_sampler_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler");
        }
    }

    void DestroyTexture(TextureSampler& texture_sampler) {
        vkDestroySampler(device_, texture_sampler.texture_sampler_, nullptr);
        vkDestroyImageView(device_, texture_sampler.texture_image_view_, nullptr);
        vkDestroyImage(device_, texture_sampler.texture_image_, nullptr);
        vkFreeMemory(device_, texture_sampler.texture_image_memory_, nullptr);
    }

    template <class Vertex, class Index>
    void CreateIndexedPrimitive(std::vector<Vertex>& vertices, std::vector<Index>& indices, IndexedPrimitive& primitive) {
        VkDeviceSize bufferSize = vertices.size() * sizeof(vertices[0]);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t)bufferSize);
        vkUnmapMemory(device_, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, primitive.vertex_buffer_, primitive.vertex_buffer_memory_);

        CopyBuffer(stagingBuffer, primitive.vertex_buffer_, bufferSize);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingBufferMemory, nullptr);

        primitive.index_count_ = static_cast<uint32_t>(indices.size());

        bufferSize = indices.size() * sizeof(indices[0]);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)bufferSize);
        vkUnmapMemory(device_, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, primitive.index_buffer_, primitive.index_buffer_memory_);

        CopyBuffer(stagingBuffer, primitive.index_buffer_, bufferSize);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingBufferMemory, nullptr);
    }

    void DestroyIndexedPrimitive(IndexedPrimitive& primitive) {
        vkDestroyBuffer(device_, primitive.index_buffer_, nullptr);
        vkFreeMemory(device_, primitive.index_buffer_memory_, nullptr);
        vkDestroyBuffer(device_, primitive.vertex_buffer_, nullptr);
        vkFreeMemory(device_, primitive.vertex_buffer_memory_, nullptr);
    }

private:
    uint32_t subpass_count_;
    uint32_t max_frames_in_flight_{2};
    RenderApplication* render_application_{};
    bool debug_layers_ = false;

    VkSurfaceKHR surface_ = nullptr;
    uint32_t graphics_family_index_ = 0;
    uint32_t present_family_index_ = 0;
    VkQueue graphics_queue_ = nullptr;
    VkQueue present_queue_ = nullptr;
    VkCommandPool command_pool_ = nullptr;
    VkSurfaceCapabilitiesKHR capabilities_{};
    VkSurfaceFormatKHR surface_format_{};
    VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
    VkPresentModeKHR present_mode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;

    VkInstance instance_{};
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;
    size_t current_frame_ = 0;

    VkDebugUtilsMessengerEXT debug_messenger_;

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_properties_{};
    const std::vector<const char*> device_extensions_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char*> validation_layers_{"VK_LAYER_KHRONOS_validation"};

    VkSampleCountFlagBits msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
    uint32_t image_count_ = 0;

    VkSwapchainKHR swapchain_{};
    std::vector<VkImage> swapchain_images_{};
    std::vector<VkImageView> swapchain_image_views_{};

    VkImage color_image_{};
    VkDeviceMemory color_image_memory_{};
    VkImageView color_image_view_{};

    VkImage depth_image_{};
    VkDeviceMemory depth_image_memory_{};
    VkImageView depth_image_view_{};

    void CreateInstance(std::vector<const char*>& required_extensions) {
        if (debug_layers_) {
            if (!CheckValidationLayerSupport()) {
                throw std::runtime_error("validation layers are not available");
            }
        }

        VkApplicationInfo application_info = {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "Vulkan Testing";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "No Engine";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &application_info;

        if (debug_layers_) {
            required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
        create_info.ppEnabledExtensionNames = required_extensions.data();

        if (debug_layers_) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
            create_info.ppEnabledLayerNames = validation_layers_.data();

            VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
            PopulateDebugMessengerCreateInfo(debug_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
        } else {
            create_info.enabledLayerCount = 0;
            create_info.pNext = nullptr;
        }

        if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance");
        }
    }

    bool CheckValidationLayerSupport() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const char* layer_name : validation_layers_) {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers) {
                if (strcmp(layer_name, layer_properties.layerName) == 0) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) {
                return false;
            }
        }

        return true;
    }

    void SetupDebugMessenger() {
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
        PopulateDebugMessengerCreateInfo(debug_create_info);

        if (CreateDebugUtilsMessengerEXT(instance_, &debug_create_info, nullptr, &debug_messenger_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create debug messenger");
        }
    }

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
        create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = DebugCallback;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* debug_create_info, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debug_messenger) {
        auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (vkCreateDebugUtilsMessengerEXT != nullptr) {
            return vkCreateDebugUtilsMessengerEXT(instance, debug_create_info, allocator, debug_messenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator) {
        auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (vkDestroyDebugUtilsMessengerEXT != nullptr) {
            vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, allocator);
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
        return VK_FALSE;
    }

    void PickPhysicalDevice() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

        if (device_count == 0) {
            throw std::runtime_error("unable to find a GPU with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

        for (const auto& device : devices) {
            if (IsDeviceSuitable(device)) {
                physical_device_ = device;
                vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties_);
                limits_ = physical_device_properties_.limits;
                break;
            }
        }

        if (physical_device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("unable to find a GPU with the required features");
        }
    }

    bool IsDeviceSuitable(VkPhysicalDevice physical_device) {
        if (!FindQueueFamilies(physical_device)) {
            return false;
        }

        bool extensions_supported = CheckDeviceExtensionSupport(physical_device);

        bool swap_chain_adequate = false;
        if (extensions_supported) {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
            QuerySwapChainSupport(physical_device, capabilities, formats, presentModes);
            swap_chain_adequate = !formats.empty() && !presentModes.empty();
        }

        VkPhysicalDeviceFeatures supported_features;
        vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

        return extensions_supported && swap_chain_adequate && supported_features.samplerAnisotropy;
    }

    VkSampleCountFlagBits GetMaxUsableSampleCount() {
        VkSampleCountFlags counts = limits_.framebufferColorSampleCounts & limits_.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
        return VK_SAMPLE_COUNT_1_BIT;
    }

    bool FindQueueFamilies(VkPhysicalDevice physical_device) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        graphics_family_index_ = -1;
        present_family_index_ = -1;
        int index = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family_index_ = index;
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface_, &present_support);

            if (present_support) {
                present_family_index_ = index;
            }

            if (graphics_family_index_ != -1 && present_family_index_ != -1) {
                return true;
            }

            index++;
        }

        return false;
    }

    bool CheckDeviceExtensionSupport(VkPhysicalDevice physical_device) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions;
        for (auto device_extension : device_extensions_) {
            required_extensions.insert(device_extension);
        }

        for (const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    void QuerySwapChainSupport(VkPhysicalDevice physical_device, VkSurfaceCapabilitiesKHR& capabilities, std::vector<VkSurfaceFormatKHR>& formats, std::vector<VkPresentModeKHR>& present_modes) {
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &format_count, nullptr);

        if (format_count != 0) {
            formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &format_count, formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface_, &present_mode_count, nullptr);

        if (present_mode_count != 0) {
            present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface_, &present_mode_count, present_modes.data());
        }
    }

    void CreateLogicalDevice() {
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = {graphics_family_index_, present_family_index_};

        float queuePriority = 1.0f;
        for (uint32_t queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_create_info = {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queuePriority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();

        create_info.pEnabledFeatures = &device_features;

        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions_.size());
        create_info.ppEnabledExtensionNames = device_extensions_.data();

        if (debug_layers_) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
            create_info.ppEnabledLayerNames = validation_layers_.data();
        } else {
            create_info.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device");
        }

        vkGetDeviceQueue(device_, graphics_family_index_, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, present_family_index_, 0, &present_queue_);
    }

    void CreateCommandPool() {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = graphics_family_index_;

        if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool");
        }
    }

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device_, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer");
        }

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = FindMemoryType(memory_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device_, &allocate_info, nullptr, &buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory");
        }

        vkBindBufferMemory(device_, buffer, buffer_memory, 0);
    }

    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);

        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("unable to find required memory type");
    }

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        for (const auto& available_format : available_formats) {
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return available_format;
            }
        }

        return available_formats[0];
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
        //for (const auto& available_present_mode : available_present_modes) {
        //    if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        //        return available_present_mode;
        //    }
        //}

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkFormat FindDepthFormat() {
        return FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format");
    }

    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = BeginCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndCommands(commandBuffer);
    }

    void CreateCommandBuffers() {
        command_buffers_.resize(image_count_);

        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = command_pool_;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = (uint32_t)command_buffers_.size();

        if (vkAllocateCommandBuffers(device_, &allocate_info, command_buffers_.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    void CreateSwapchain(uint32_t window_width, uint32_t window_height) {
        VkExtent2D extent = ChooseSwapExtent(window_width, window_height);

        VkSwapchainCreateInfoKHR create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface_;

        create_info.minImageCount = image_count_;
        create_info.imageFormat = surface_format_.format;
        create_info.imageColorSpace = surface_format_.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queue_family_indices[] = {graphics_family_index_, present_family_index_};

        if (graphics_family_index_ != present_family_index_) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        create_info.preTransform = capabilities_.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode_;
        create_info.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain");
        }

        vkGetSwapchainImagesKHR(device_, swapchain_, &image_count_, nullptr);
        swapchain_images_.resize(image_count_);
        vkGetSwapchainImagesKHR(device_, swapchain_, &image_count_, swapchain_images_.data());

        swapchain_extent_ = extent;

        swapchain_image_views_.resize(image_count_);

        for (size_t i = 0; i < image_count_; i++) {
            swapchain_image_views_[i] = CreateImageView(swapchain_images_[i], surface_format_.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }

        VkFormat color_format = surface_format_.format;
        CreateImage(swapchain_extent_.width, swapchain_extent_.height, 1, msaa_samples_, color_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_image_, color_image_memory_);
        color_image_view_ = CreateImageView(color_image_, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        VkFormat depth_format = depth_format_;
        CreateImage(swapchain_extent_.width, swapchain_extent_.height, 1, msaa_samples_, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_, depth_image_memory_);
        depth_image_view_ = CreateImageView(depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

        CreateCommandBuffers();
    }

    void DestroySwapchain() {
        vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());

        vkDestroyImageView(device_, depth_image_view_, nullptr);
        vkDestroyImage(device_, depth_image_, nullptr);
        vkFreeMemory(device_, depth_image_memory_, nullptr);

        vkDestroyImageView(device_, color_image_view_, nullptr);
        vkDestroyImage(device_, color_image_, nullptr);
        vkFreeMemory(device_, color_image_memory_, nullptr);

        for (auto image_view : swapchain_image_views_) {
            vkDestroyImageView(device_, image_view, nullptr);
        }

        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    }

    void CreateImage(uint32_t width, uint32_t height, uint32_t mip_levels, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory) {
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = mip_levels;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = num_samples;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device_, &image_info, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image");
        }

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(device_, image, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = FindMemoryType(memory_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device_, &allocate_info, nullptr, &image_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory");
        }

        vkBindImageMemory(device_, image, image_memory, 0);
    }

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_levels) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_levels;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (vkCreateImageView(device_, &view_info, nullptr, &image_view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view");
        }

        return image_view;
    }

    void CreateRenderPass() {
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = surface_format_.format;
        color_attachment.samples = msaa_samples_;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = depth_format_;
        depth_attachment.samples = msaa_samples_;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription color_attachment_resolve = {};
        color_attachment_resolve.format = surface_format_.format;
        color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_reference = {};
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_resolve_reference = {};
        color_attachment_resolve_reference.attachment = 2;
        color_attachment_resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        std::vector<VkSubpassDescription> subpasses = {};
        std::vector<VkSubpassDependency> dependencies = {};

        for (uint32_t i = 0; i < subpass_count_; i++) {
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment_reference;
            subpass.pDepthStencilAttachment = &depth_attachment_reference;
            subpass.pResolveAttachments = &color_attachment_resolve_reference;
            subpasses.push_back(subpass);
        }

        VkSubpassDependency dependency = {};
        if (subpass_count_ == 1) {
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies.push_back(dependency);
        } else if (subpass_count_ > 1) {
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies.push_back(dependency);

            for (uint32_t subpass = 1; subpass < subpass_count_; subpass++) {
                dependency.srcSubpass = subpass - 1;
                dependency.dstSubpass = subpass;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependencies.push_back(dependency);
            }
        }

        std::array<VkAttachmentDescription, 3> attachments = {color_attachment, depth_attachment, color_attachment_resolve};
        VkRenderPassCreateInfo render_pass_Info = {};
        render_pass_Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_Info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_Info.pAttachments = attachments.data();
        render_pass_Info.subpassCount = static_cast<uint32_t>(subpasses.size());
        render_pass_Info.pSubpasses = subpasses.data();
        render_pass_Info.dependencyCount = static_cast<uint32_t>(dependencies.size());
        render_pass_Info.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device_, &render_pass_Info, nullptr, &render_pass_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass");
        }
    }

    void CreateFramebuffers(VkRenderPass render_pass, std::vector<VkFramebuffer>& framebuffers) {
        framebuffers.resize(swapchain_image_views_.size());

        for (size_t i = 0; i < swapchain_image_views_.size(); i++) {
            std::array<VkImageView, 3> attachments = {
                color_image_view_,
                depth_image_view_,
                swapchain_image_views_[i]
            };

            VkFramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = swapchain_extent_.width;
            framebuffer_info.height = swapchain_extent_.height;
            framebuffer_info.layers = 1;

            if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    void CreateSyncObjects() {
        image_available_semaphores_.resize(max_frames_in_flight_);
        render_finished_semaphores_.resize(max_frames_in_flight_);
        in_flight_fences_.resize(max_frames_in_flight_);
        images_in_flight_.resize(image_count_, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < max_frames_in_flight_; i++) {
            if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
        }
    }

    VkExtent2D ChooseSwapExtent(uint32_t windowWidth, uint32_t windowHeight) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities_);

        if (capabilities_.currentExtent.width != UINT32_MAX) {
            return capabilities_.currentExtent;
        } else {
            VkExtent2D actual_extent = {windowWidth, windowHeight};

            actual_extent.width = std::max(capabilities_.minImageExtent.width, std::min(capabilities_.maxImageExtent.width, actual_extent.width));
            actual_extent.height = std::max(capabilities_.minImageExtent.height, std::min(capabilities_.maxImageExtent.height, actual_extent.height));

            return actual_extent;
        }
    }

    void GenerateMipmaps(VkImage image, VkFormat image_format, int32_t texture_width, int32_t texture_height, uint32_t mip_levels) {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(physical_device_, image_format, &format_properties);

        if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting");
        }

        VkCommandBuffer command_buffer = BeginCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mip_width = texture_width;
        int32_t mip_height = texture_height;

        for (uint32_t i = 1; i < mip_levels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit = {};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_width, mip_height, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            if (mip_width > 1) {
                mip_width /= 2;
            }

            if (mip_height > 1) {
                mip_height /= 2;
            }
        }

        barrier.subresourceRange.baseMipLevel = mip_levels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndCommands(command_buffer);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer command_buffer = BeginCommands();

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        EndCommands(command_buffer);
    }

    void TransformImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t mipLevels) {
        VkCommandBuffer command_buffer = BeginCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags destination_stage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition");
        }

        vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndCommands(command_buffer);
    }

    VkCommandBuffer BeginCommands() {
        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandPool = command_pool_;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        return command_buffer;
    }

    void EndCommands(VkCommandBuffer command_buffer) {
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue_);

        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    }
};
