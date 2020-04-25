#pragma once

#include "Math.h"
#include "Utility.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Geometry.h"
#include "Geometry_2D.h"

static const char* SPRITE_PATH = "textures/texture.jpg";

class SpriteScene : public Scene {
public:
    SpriteScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void Startup() {
        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/ortho2d/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/ortho2d/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());

            texture_descriptor_set_ = render_engine_.CreateDescriptorSet({}, 1);

            texture_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                vertex_shader_module,
                fragment_shader_module,
                {},
                Vertex_2D::getBindingDescription(),
                Vertex_2D::getAttributeDescriptions(),
                texture_descriptor_set_,
                0,
                true,
                false
            );
        }

        LoadTexture(SPRITE_PATH, sprite_texture_);

        render_engine_.UpdateDescriptorSets(texture_descriptor_set_, {sprite_texture_});

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

        render_engine_.DestroyGraphicsPipeline(texture_graphics_pipeline_);
        render_engine_.DestroyDescriptorSet(texture_descriptor_set_);

        render_engine_.DestroyIndexedPrimitive(sprite_primitive_);
        render_engine_.DestroyTexture(sprite_texture_);
    }

    void OnEntry() {
        if (!startup_) {
            startup_ = true;
            Startup();
        }
        render_engine_.RebuildRenderPass(1);
    }

    void OnExit() {
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

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->graphics_pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->pipeline_layout, 0, 1, &texture_descriptor_set_->descriptor_sets[image_index], 0, nullptr);
        render_engine_.DrawPrimitive(command_buffer, sprite_primitive_);

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);
    }

    void PipelineReset() {
        render_engine_.ResetGraphicsPipeline(texture_graphics_pipeline_);
    }

    void PipelineRebuild() {
        render_engine_.RebuildGraphicsPipeline(texture_graphics_pipeline_);
    }

private:
    RenderEngine& render_engine_;
    bool startup_ = false;

    std::shared_ptr<RenderEngine::DescriptorSet> texture_descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> texture_graphics_pipeline_{};

    IndexedPrimitive sprite_primitive_{};
    TextureSampler sprite_texture_;

    void LoadTexture(const char* fileName, TextureSampler& texture_sampler) {
        Utility::Image texture;
        Utility::LoadImage(fileName, texture);

        render_engine_.CreateTexture(texture.pixels, texture.texture_width, texture.texture_height, texture_sampler);

        Utility::FreeImage(texture);
    }
};
