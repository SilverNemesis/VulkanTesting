#pragma once

#include "Font.h"
#include "RenderEngine.h"

class Text {
public:
    Text(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void Register(std::shared_ptr<RenderEngine::RenderPass> render_pass) {
        render_pass_ = render_pass;
        {
            texture_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(CameraMatrix));

            texture_descriptor_set_ = render_engine_.CreateDescriptorSet({texture_uniform_buffer_}, 1);

            texture_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                "shaders/text/vert.spv",
                "shaders/text/frag.spv",
                {
                    PushConstant{offsetof(PushConstants, color), sizeof(push_constants_.color), VK_SHADER_STAGE_FRAGMENT_BIT},
                    PushConstant{offsetof(PushConstants, position), sizeof(push_constants_.position), VK_SHADER_STAGE_VERTEX_BIT}
                },
                Vertex_Text::getBindingDescription(),
                Vertex_Text::getAttributeDescriptions(),
                texture_descriptor_set_,
                0,
                false,
                true
            );
        }

        font_.Initialize("fonts/Inconsolata/Inconsolata-Regular.ttf", 36);

        render_engine_.UpdateDescriptorSets(texture_descriptor_set_, {font_.texture_});

        Geometry_Text geometry_text_1{};
        font_.RenderText("Hello world!", geometry_text_1, primitive_1_width_, primitive_1_height_);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_1.vertices, geometry_text_1.indices, primitive_1_);

        Geometry_Text geometry_text_2{};
        font_.RenderText("Goodbye world  :(", geometry_text_2, primitive_2_width_, primitive_2_height_);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_2.vertices, geometry_text_2.indices, primitive_2_);
    }

    void Unregister() {
        vkDeviceWaitIdle(render_engine_.device_);

        render_engine_.DestroyGraphicsPipeline(texture_graphics_pipeline_);
        render_engine_.DestroyDescriptorSet(texture_descriptor_set_);
        render_engine_.DestroyUniformBuffer(texture_uniform_buffer_);

        font_.Destroy();

        render_engine_.DestroyIndexedPrimitive(primitive_1_);
        render_engine_.DestroyIndexedPrimitive(primitive_2_);
    }

    void Update() {
    }

    void Render(VkCommandBuffer& command_buffer, uint32_t image_index) {
        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;
        uniform_buffer_.proj = glm::ortho(0.0f, static_cast<float>(window_width), static_cast<float>(window_height), 0.0f);
        render_engine_.UpdateUniformBuffers(texture_uniform_buffer_, &uniform_buffer_);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->graphics_pipeline);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->pipeline_layout, 0, 1, &texture_descriptor_set_->descriptor_sets[image_index], 0, nullptr);

        push_constants_.color = {1.0, 0.0, 0.0};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
        push_constants_.position = {window_width / 2 - primitive_1_width_ / 2, window_height / 2 + font_.height_ / 2};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
        render_engine_.DrawPrimitive(command_buffer, primitive_1_);

        push_constants_.color = {0.5, 0.5, 1.0};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
        push_constants_.position = {window_width / 2 - primitive_2_width_ / 2, window_height / 2 - font_.height_ / 2};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
        render_engine_.DrawPrimitive(command_buffer, primitive_2_);
    }

private:
    RenderEngine& render_engine_;
    std::shared_ptr<RenderEngine::RenderPass> render_pass_{};
    Font font_{render_engine_};
    std::shared_ptr<RenderEngine::GraphicsPipeline> texture_graphics_pipeline_{};
    std::shared_ptr<RenderEngine::DescriptorSet> texture_descriptor_set_{};
    std::shared_ptr<RenderEngine::UniformBuffer> texture_uniform_buffer_{};

    struct CameraMatrix {
        glm::mat4 proj;
    };

    CameraMatrix uniform_buffer_{};

    struct PushConstants {
        glm::vec3 color{};
        alignas(8) glm::vec2 position{};
    } push_constants_;

    IndexedPrimitive primitive_1_{};
    uint32_t primitive_1_width_{};
    uint32_t primitive_1_height_{};

    IndexedPrimitive primitive_2_{};
    uint32_t primitive_2_width_{};
    uint32_t primitive_2_height_{};
};
