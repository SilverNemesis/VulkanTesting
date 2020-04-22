#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "Utility.h"
#include "RenderEngine.h"
#include "Geometry.h"
#include "Geometry_Color.h"
#include "Geometry_Texture.h"

class CubeApplication {
public:
    void Startup(RenderApplication* render_application, int window_width, int window_height) {
        window_width_ = window_width;
        window_height_ = window_height;

        render_engine_.Initialize(render_application);

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/color/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/color/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());

            color_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(UniformBufferObject));

            color_descriptor_set_ = render_engine_.CreateDescriptorSet({color_uniform_buffer_}, 0);

            color_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                vertex_shader_module,
                fragment_shader_module,
                0,
                Vertex_Color::getBindingDescription(),
                Vertex_Color::getAttributeDescriptions(),
                color_descriptor_set_,
                0,
                true,
                false
            );
        }

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/notexture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/notexture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());

            texture_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(UniformBufferObject));

            texture_descriptor_set_ = render_engine_.CreateDescriptorSet({texture_uniform_buffer_}, 0);

            texture_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                vertex_shader_module,
                fragment_shader_module,
                0,
                Vertex_Texture::getBindingDescription(),
                Vertex_Texture::getAttributeDescriptions(),
                texture_descriptor_set_,
                0,
                true,
                false
            );
        }

        {
            std::vector<glm::vec3> vertices{};
            std::vector<std::vector<uint32_t>> faces{};
            Geometry::CreateCube(vertices, faces);

            std::vector<glm::vec3> colors = {
                {1.0, 1.0, 1.0},
                {1.0, 0.0, 0.0},
                {0.0, 1.0, 0.0},
                {0.0, 0.0, 1.0},
                {1.0, 1.0, 0.0},
                {1.0, 0.0, 1.0}
            };

            Geometry_Color geometry_color{};
            geometry_color.AddFaces(vertices, faces, colors);
            render_engine_.CreateIndexedPrimitive<Vertex_Color, uint32_t>(geometry_color.vertices, geometry_color.indices, color_primitive_);

            std::vector<glm::vec2> texture_coordinates = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1}
            };

            Geometry_Texture geometry_texture{};
            geometry_texture.AddFaces(vertices, faces, texture_coordinates);
            render_engine_.CreateIndexedPrimitive<Vertex_Texture, uint32_t>(geometry_texture.vertices, geometry_texture.indices, texture_primitive_);
        }
    }

    void Shutdown() {
        vkDeviceWaitIdle(render_engine_.device_);

        render_engine_.DestroyGraphicsPipeline(color_graphics_pipeline_);
        render_engine_.DestroyDescriptorSet(color_descriptor_set_);
        render_engine_.DestroyUniformBuffer(color_uniform_buffer_);

        render_engine_.DestroyGraphicsPipeline(texture_graphics_pipeline_);
        render_engine_.DestroyDescriptorSet(texture_descriptor_set_);
        render_engine_.DestroyUniformBuffer(texture_uniform_buffer_);

        render_engine_.DestroyIndexedPrimitive(texture_primitive_);
        render_engine_.DestroyIndexedPrimitive(color_primitive_);

        render_engine_.Destroy();
    }

    void Update(glm::mat4 view_matrix) {
        glm::mat4 projection_matrix = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);

        static float total_time;
        total_time += 4.0f / 1000.0f;

        float offset_1 = std::sin(total_time);
        float offset_2 = std::cos(total_time);

        uniform_buffer_1_.model = glm::mat4(1.0f);
        uniform_buffer_1_.model = glm::translate(uniform_buffer_1_.model, glm::vec3(-1.0f, 0.5f, offset_1 + 1.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_1_.model = glm::scale(uniform_buffer_1_.model, glm::vec3(1.5f, 1.5f, 1.5f));
        uniform_buffer_1_.view = view_matrix;
        uniform_buffer_1_.proj = projection_matrix;

        uniform_buffer_2_.model = glm::mat4(1.0f);
        uniform_buffer_2_.model = glm::translate(uniform_buffer_2_.model, glm::vec3(1.0f, 0.5f, offset_2 + 1.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_2_.model = glm::scale(uniform_buffer_2_.model, glm::vec3(1.5f, 1.5f, 1.5f));
        uniform_buffer_2_.view = view_matrix;
        uniform_buffer_2_.proj = projection_matrix;
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

        VkCommandBuffer& command_buffer = render_engine_.command_buffers_[image_index];

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_engine_.render_pass_;
        render_pass_info.framebuffer = render_engine_.framebuffers_[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = render_engine_.swapchain_extent_;

        std::array<VkClearValue, 2> clear_values = {};
        clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        {
            render_engine_.UpdateUniformBuffer(color_uniform_buffer_, image_index, &uniform_buffer_1_);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, color_graphics_pipeline_->graphics_pipeline);
            VkBuffer vertex_buffers_1[] = {color_primitive_.vertex_buffer_};
            VkDeviceSize offsets_1[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
            vkCmdBindIndexBuffer(command_buffer, color_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, color_graphics_pipeline_->pipeline_layout, 0, 1, &color_descriptor_set_->descriptor_sets[image_index], 0, nullptr);
            vkCmdDrawIndexed(command_buffer, color_primitive_.index_count_, 1, 0, 0, 0);
        }

        vkCmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        {
            render_engine_.UpdateUniformBuffer(texture_uniform_buffer_, image_index, &uniform_buffer_2_);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->graphics_pipeline);
            VkBuffer vertex_buffers_2[] = {texture_primitive_.vertex_buffer_};
            VkDeviceSize offsets_2[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_2, offsets_2);
            vkCmdBindIndexBuffer(command_buffer, texture_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->pipeline_layout, 0, 1, &texture_descriptor_set_->descriptor_sets[image_index], 0, nullptr);
            vkCmdDrawIndexed(command_buffer, texture_primitive_.index_count_, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);
    }

    void Resize(int window_width, int window_height) {
        window_width_ = window_width;
        window_height_ = window_height;
        render_engine_.RebuildSwapchain();
    }

    void PipelineReset() {
        render_engine_.ResetGraphicsPipeline(texture_graphics_pipeline_);
        render_engine_.ResetGraphicsPipeline(color_graphics_pipeline_);
    }

    void PipelineRebuild() {
        render_engine_.RebuildGraphicsPipeline(texture_graphics_pipeline_);
        render_engine_.RebuildGraphicsPipeline(color_graphics_pipeline_);
    }

private:
    int window_width_;
    int window_height_;

    RenderEngine render_engine_{2};

    std::shared_ptr<RenderEngine::UniformBuffer> color_uniform_buffer_{};
    std::shared_ptr<RenderEngine::DescriptorSet> color_descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> color_graphics_pipeline_{};

    std::shared_ptr<RenderEngine::UniformBuffer> texture_uniform_buffer_{};
    std::shared_ptr<RenderEngine::DescriptorSet> texture_descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> texture_graphics_pipeline_{};

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    UniformBufferObject uniform_buffer_1_{};
    UniformBufferObject uniform_buffer_2_{};

    IndexedPrimitive color_primitive_{};
    IndexedPrimitive texture_primitive_{};
};
