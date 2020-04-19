#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "Utility.h"
#include "RenderEngine.h"
#include "RenderPipeline.h"
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
            render_pipeline_color_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0, 0, 1, false);
        }

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/notexture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/notexture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_texture_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0, 0, 1, false);
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
        render_pipeline_texture_.Destroy();
        render_pipeline_color_.Destroy();
        render_engine_.DestroyIndexedPrimitive(texture_primitive_);
        render_engine_.DestroyIndexedPrimitive(color_primitive_);
        render_engine_.Destroy();
    }

    void Update(bool mouse_capture, int mouse_x, int mouse_y, std::array<bool, SDL_NUM_SCANCODES>& key_state) {
        glm::mat4 projection_matrix = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);

        if (mouse_capture) {
            camera_yaw_ = std::fmod(camera_yaw_ - 0.05f * mouse_x, 360.0f);
            camera_pitch_ = std::fmod(camera_pitch_ + 0.05f * mouse_y, 360.0f);
            camera_pitch_ = camera_pitch_ < -89.0f ? -89.0f : camera_pitch_ > 89.0f ? 89.0f : camera_pitch_;

            glm::mat4 rotation = glm::mat4{1.0f};
            rotation = glm::rotate(rotation, glm::radians(camera_pitch_), glm::vec3{-1.0f, 0.0f, 0.0f});
            rotation = glm::rotate(rotation, glm::radians(camera_yaw_), glm::vec3{0.0f, -1.0f, 0.0f});
            camera_forward_ = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) * rotation;
            camera_right_ = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) * rotation;
            camera_up_ = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f) * rotation;
        }

        const float speed = 0.03f;

        if (key_state[SDL_SCANCODE_W]) {
            camera_position_ += speed * camera_forward_;
        }

        if (key_state[SDL_SCANCODE_S]) {
            camera_position_ -= speed * camera_forward_;
        }

        if (key_state[SDL_SCANCODE_A]) {
            camera_position_ += speed * camera_right_;
        }

        if (key_state[SDL_SCANCODE_D]) {
            camera_position_ -= speed * camera_right_;
        }

        glm::mat4 view_matrix = glm::lookAt(camera_position_, camera_position_ + camera_forward_, camera_up_);

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
            RenderPipeline& render_pipeline = render_pipeline_color_;
            const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

            render_pipeline.UpdateUniformBuffer(image_index, 0, &uniform_buffer_1_);

            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
            VkBuffer vertex_buffers_1[] = {color_primitive_.vertex_buffer_};
            VkDeviceSize offsets_1[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
            vkCmdBindIndexBuffer(command_buffer, color_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDrawIndexed(command_buffer, color_primitive_.index_count_, 1, 0, 0, 0);
        }

        vkCmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        {
            RenderPipeline& render_pipeline = render_pipeline_texture_;
            const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

            render_pipeline.UpdateUniformBuffer(image_index, 0, &uniform_buffer_2_);

            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
            VkBuffer vertex_buffers_2[] = {texture_primitive_.vertex_buffer_};
            VkDeviceSize offsets_2[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_2, offsets_2);
            vkCmdBindIndexBuffer(command_buffer, texture_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
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
        render_pipeline_texture_.Reset();
        render_pipeline_color_.Reset();
    }

    void PipelineRebuild() {
        render_pipeline_color_.Rebuild();
        render_pipeline_texture_.Rebuild();
    }

private:
    int window_width_;
    int window_height_;

    glm::vec3 camera_position_{0.0f, 0.5f, -3.0f};
    glm::vec3 camera_forward_{0.0f, 0.0f, 1.0f};
    glm::vec3 camera_right_{1.0f, 0.0f, 0.0f};
    glm::vec3 camera_up_{0.0f, -1.0f, 0.0f};
    float camera_yaw_ = 0.0;
    float camera_pitch_ = 0.0;

    RenderEngine render_engine_{2};
    RenderPipeline render_pipeline_color_{render_engine_, Vertex_Color::getBindingDescription(), Vertex_Color::getAttributeDescriptions(), 0};
    RenderPipeline render_pipeline_texture_{render_engine_, Vertex_Texture::getBindingDescription(), Vertex_Texture::getAttributeDescriptions(), 1};

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
