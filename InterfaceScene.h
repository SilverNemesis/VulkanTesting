#pragma once

#include <Windows.h>

#include "Math.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Font.h"

struct Vertex_UI {
    glm::vec2 pos;
    glm::vec2 uv;
    glm::tvec4<unsigned char> col;

    bool operator==(const Vertex_UI& other) const {
        return pos == other.pos && uv == other.uv && col == other.col;
    }

    static VkVertexInputBindingDescription getBindingDescription() {
        static VkVertexInputBindingDescription bindingDescription = {0, sizeof(Vertex_UI), VK_VERTEX_INPUT_RATE_VERTEX};
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_UI, pos)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex_UI, uv)},
            {2, 0, VK_FORMAT_R8G8B8A8_UINT, offsetof(Vertex_UI, col)}
            }};
        return attributeDescriptions;
    }
};

class InterfaceScene : public Scene {
public:
    InterfaceScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void OnQuit() {
        if (startup_) {
            vkDeviceWaitIdle(render_engine_.device_);
            render_engine_.DestroyGraphicsPipeline(graphics_pipeline_);
            render_engine_.DestroyDescriptorSet(descriptor_set_);
            render_engine_.DestroyUniformBuffer(uniform_buffer_);

            font_.Destroy();

            render_engine_.DestroyIndexedPrimitive(primitive_);
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

    void Update(glm::mat4 view_matrix) {
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

        static int count = 0;

        if (count % 60 == 0) {
            const char* phrase = phrases_[rand() % phrases_.size()];
            Geometry_Text geometry_text{};
            font_.RenderText(phrase, geometry_text, primitive_width_, primitive_height_);
            render_engine_.UpdateDynamicIndexedPrimitive<Vertex_Text, uint32_t>(
                geometry_text.vertices.data(),
                static_cast<uint32_t>(geometry_text.vertices.size()),
                geometry_text.indices.data(),
                static_cast<uint32_t>(geometry_text.indices.size()),
                primitive_);
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

        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;
        camera_.proj = glm::ortho(0.0f, static_cast<float>(window_width), static_cast<float>(window_height), 0.0f);
        render_engine_.UpdateUniformBuffers(uniform_buffer_, &camera_);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->graphics_pipeline);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->pipeline_layout, 0, 1, &descriptor_set_->descriptor_sets[image_index], 0, nullptr);

        if (count % 30 == 0) {
            push_constants_.color = {
                (rand() % 256) / 255.0,
                (rand() % 256) / 255.0,
                (rand() % 256) / 255.0
            };
        }
        vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
        push_constants_.position = {window_width / 2 - primitive_width_ / 2, window_height / 2 + font_.height_ / 2};
        vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
        render_engine_.DrawPrimitive(command_buffer, primitive_);

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);

        count++;
    }

private:
    RenderEngine& render_engine_;
    bool startup_ = false;

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

    std::vector<const char*> phrases_ = {"Hello world!", "Goodbye world!", "Cruel world", "HAPPY", "SAD", "LOVE", "HATE"};

    IndexedPrimitive primitive_{};
    uint32_t primitive_width_{};
    uint32_t primitive_height_{};

    void Startup() {
        render_pass_ = render_engine_.CreateRenderPass();

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
                true
            );
        }

        font_.Initialize("fonts/Inconsolata/Inconsolata-Regular.ttf", 36);

        render_engine_.UpdateDescriptorSets(descriptor_set_, {font_.texture_});

        const char* phrase = phrases_[0];

        for (size_t i = 1; i < phrases_.size(); i++) {
            if (strlen(phrases_[i]) > strlen(phrase)) {
                phrase = phrases_[i];
            }
        }

        Geometry_Text geometry_text{};
        font_.RenderText(phrase, geometry_text, primitive_width_, primitive_height_);
        render_engine_.CreateDynamicIndexedPrimitive<Vertex_Text, uint32_t>(
            geometry_text.vertices.data(),
            static_cast<uint32_t>(geometry_text.vertices.size()),
            geometry_text.indices.data(),
            static_cast<uint32_t>(geometry_text.indices.size()),
            primitive_);
    }
};
