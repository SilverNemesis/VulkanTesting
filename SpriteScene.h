#pragma once

#include "Math.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Geometry.h"
#include "Geometry_2D.h"

static const char* SPRITE_PATH = "textures/texture.jpg";

class SpriteScene : public Scene {
public:
    SpriteScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void OnQuit() {
        if (startup_) {
            vkDeviceWaitIdle(render_engine_.device_);

            render_engine_.DestroyGraphicsPipeline(graphics_pipeline_);
            render_engine_.DestroyDescriptorSet(descriptor_set_);

            render_engine_.DestroyIndexedPrimitive(primitive_);
            render_engine_.DestroyTexture(texture_);
        }
    }

    void OnEntry() {
        if (!startup_) {
            startup_ = true;
            Startup();
        }
    }

    void OnExit() {
    }

    void Update(std::array<bool, SDL_NUM_SCANCODES>& key_state, bool mouse_capture, int mouse_x, int mouse_y) {
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
        render_pass_info.renderPass = render_pass_->render_pass_;
        render_pass_info.framebuffer = render_pass_->framebuffers_[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = render_engine_.swapchain_extent_;

        std::array<VkClearValue, 2> clear_values = {};
        clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->graphics_pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->pipeline_layout, 0, 1, &descriptor_set_->descriptor_sets[image_index], 0, nullptr);
        render_engine_.DrawPrimitive(command_buffer, primitive_);

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);
    }

private:
    RenderEngine& render_engine_;
    bool startup_ = false;

    std::shared_ptr<RenderEngine::DescriptorSet> descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> graphics_pipeline_{};
    std::shared_ptr<RenderEngine::RenderPass> render_pass_{};

    IndexedPrimitive primitive_{};
    TextureSampler texture_;

    void Startup() {
        render_pass_ = render_engine_.CreateRenderPass();

        {
            descriptor_set_ = render_engine_.CreateDescriptorSet({}, 1);

            graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                "shaders/ortho2d/vert.spv",
                "shaders/ortho2d/frag.spv",
                {},
                Vertex_2D::getBindingDescription(),
                Vertex_2D::getAttributeDescriptions(),
                descriptor_set_,
                0,
                true,
                false
            );
        }

        render_engine_.LoadTexture(SPRITE_PATH, texture_);

        render_engine_.UpdateDescriptorSets(descriptor_set_, {texture_});

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
            render_engine_.CreateIndexedPrimitive<Vertex_2D, uint32_t>(geometry_sprite.vertices, geometry_sprite.indices, primitive_);
        }
    }
};
