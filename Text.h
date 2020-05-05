#pragma once

#include "Font.h"
#include "RenderEngine.h"
#include "Geometry_Text.h"

#define VERTEX_COUNT            (128*1024)
#define INDEX_COUNT             (128*1024)

class Text {
public:
    Text(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void Register(std::shared_ptr<RenderEngine::RenderPass> render_pass) {
        render_pass_ = render_pass;
        {
            uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(CameraMatrix));

            descriptor_set_ = render_engine_.CreateDescriptorSet({uniform_buffer_}, 1);

            graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
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
                descriptor_set_,
                0,
                false,
                true,
                false,
                false
            );
        }

        font_.Initialize("fonts/Inconsolata/Inconsolata-Regular.ttf", 36);

        render_engine_.UpdateDescriptorSets(descriptor_set_, {font_.texture_});

        VkDeviceSize vertex_size = static_cast<VkDeviceSize>(VERTEX_COUNT * sizeof(Vertex_Text));
        VkDeviceSize index_size = static_cast<VkDeviceSize>(INDEX_COUNT * sizeof(uint32_t));
        render_engine_.AllocateDynamicIndexedPrimitive<Vertex_Text, uint32_t>(vertex_size, index_size, primitive_);
    }

    void Unregister() {
        vkDeviceWaitIdle(render_engine_.device_);
        render_engine_.DestroyGraphicsPipeline(graphics_pipeline_);
        render_engine_.DestroyDescriptorSet(descriptor_set_);
        render_engine_.DestroyUniformBuffer(uniform_buffer_);

        font_.Destroy();

        render_engine_.DestroyIndexedPrimitive(primitive_);
    }

    void DrawBegin() {
        texts_.clear();
        geometry_text_.vertices.clear();
        geometry_text_.indices.clear();
    }

    void GetSize(const char* word, uint32_t& width, uint32_t& height) {
        font_.GetSize(word, width, height);
    }

    void Draw(glm::vec3& color, glm::vec2& position, const char* word) {
        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;

        Model text{};
        text.offset = static_cast<uint32_t>(geometry_text_.indices.size());
        font_.RenderText(word, geometry_text_, text.width, text.height);
        text.count = static_cast<uint32_t>(geometry_text_.indices.size() - text.offset);
        text.color = color;
        text.position = position;
        texts_.push_back(text);
    }

    void DrawEnd() {
        render_engine_.UpdateDynamicIndexedPrimitive<Vertex_Text, uint32_t>(
            geometry_text_.vertices.data(),
            static_cast<uint32_t>(geometry_text_.vertices.size()),
            geometry_text_.indices.data(),
            static_cast<uint32_t>(geometry_text_.indices.size()),
            primitive_);
    }

    void Render(VkCommandBuffer& command_buffer, uint32_t image_index) {
        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;

        camera_.proj = glm::ortho(0.0f, static_cast<float>(window_width), static_cast<float>(window_height), 0.0f);
        render_engine_.UpdateUniformBuffers(uniform_buffer_, &camera_);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->graphics_pipeline);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->pipeline_layout, 0, 1, &descriptor_set_->descriptor_sets[image_index], 0, nullptr);

        render_engine_.BindPrimitive(command_buffer, primitive_);

        for (auto& text : texts_) {
            push_constants_.color = text.color;
            push_constants_.position = text.position;
            vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
            vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
            vkCmdDrawIndexed(command_buffer, text.count, 1, text.offset, 0, 0);
        }
    }

private:
    RenderEngine& render_engine_;
    std::shared_ptr<RenderEngine::RenderPass> render_pass_{};

    Font font_{render_engine_};

    std::shared_ptr<RenderEngine::GraphicsPipeline> graphics_pipeline_{};
    std::shared_ptr<RenderEngine::DescriptorSet> descriptor_set_{};
    std::shared_ptr<RenderEngine::UniformBuffer> uniform_buffer_{};

    struct CameraMatrix {
        glm::mat4 proj;
    };

    CameraMatrix camera_{};

    struct PushConstants {
        glm::vec3 color{};
        alignas(8) glm::vec2 position{};
    } push_constants_;

    Geometry_Text geometry_text_{};

    struct Model {
        uint32_t offset;
        uint32_t count;
        uint32_t width;
        uint32_t height;
        glm::vec3 color{};
        glm::vec2 position{};
    };

    std::vector<Model> texts_;

    IndexedPrimitive primitive_{};
};
