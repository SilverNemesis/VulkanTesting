#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "Utility.h"
#include "RenderEngine.h"
#include "RenderPipeline.h"
#include "Geometry.h"
#include "Geometry_2D.h"

static const char* SPRITE_PATH = "textures/texture.jpg";

class SpriteApplication {
public:
    void Startup(RenderApplication* render_application, int window_width, int window_height) {
        window_width_ = window_width;
        window_height_ = window_height;

        render_engine_.Initialize(render_application);

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/ortho2d/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/ortho2d/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_sprite_.Initialize(vertex_shader_module, fragment_shader_module, 0, 0, 1, 1, false);
        }

        LoadTexture(SPRITE_PATH, sprite_texture_);

        render_pipeline_sprite_.UpdateDescriptorSets(0, {sprite_texture_});

        {
            std::vector<glm::vec2> vertices{};
            std::vector<std::vector<uint32_t>> faces{};
            Geometry2D::CreateSquare(0.35f, vertices, faces);

            std::vector<glm::vec2> texture_coordinates = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1}
            };

            Geometry_2D geometry_sprite{};
            geometry_sprite.AddFaces(vertices, faces, texture_coordinates);
            render_engine_.CreateIndexedPrimitive<Vertex_2D, uint32_t>(geometry_sprite.vertices, geometry_sprite.indices, sprite_primitive_);
        }
    }

    void Shutdown() {
        vkDeviceWaitIdle(render_engine_.device_);
        render_pipeline_sprite_.Destroy();
        render_engine_.DestroyIndexedPrimitive(sprite_primitive_);
        render_engine_.DestroyTexture(sprite_texture_);
        render_engine_.Destroy();
    }

    void Update(glm::mat4 view_matrix) {
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

        RenderPipeline& render_pipeline = render_pipeline_sprite_;
        const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
        if (sprite_primitive_.index_count_ > 0) {
            VkBuffer vertex_buffers_1[] = {sprite_primitive_.vertex_buffer_};
            VkDeviceSize offsets_1[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
            vkCmdBindIndexBuffer(command_buffer, sprite_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDrawIndexed(command_buffer, sprite_primitive_.index_count_, 1, 0, 0, 0);
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
        render_pipeline_sprite_.Reset();
    }

    void PipelineRebuild() {
        render_pipeline_sprite_.Rebuild();
        render_pipeline_sprite_.UpdateDescriptorSets(0, {sprite_texture_});
    }

private:
    int window_width_;
    int window_height_;
    RenderEngine render_engine_{1};
    RenderPipeline render_pipeline_sprite_{render_engine_, Vertex_2D::getBindingDescription(), Vertex_2D::getAttributeDescriptions(), 0};

    IndexedPrimitive sprite_primitive_{};
    TextureSampler sprite_texture_;

    void LoadTexture(const char* fileName, TextureSampler& texture_sampler) {
        Utility::Image texture;
        Utility::LoadImage(fileName, texture);

        render_engine_.CreateTexture(texture.pixels, texture.texture_width, texture.texture_height, texture_sampler);

        Utility::FreeImage(texture);
    }
};
