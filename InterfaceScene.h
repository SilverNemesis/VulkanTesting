#pragma once

#include <Windows.h>

#include "Math.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Font.h"

#define WORD_COUNT              2048
#define VERTEX_COUNT            (128*1024)
#define INDEX_COUNT             (128*1024)

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

    void Update(std::array<bool, SDL_NUM_SCANCODES>& key_state, bool mouse_capture, int mouse_x, int mouse_y) {
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;

        camera_.proj = glm::ortho(0.0f, static_cast<float>(window_width), static_cast<float>(window_height), 0.0f);
        render_engine_.UpdateUniformBuffers(uniform_buffer_, &camera_);

        Geometry_Text geometry_text{};

        texts_.clear();

        for (int i = 0; i < WORD_COUNT; i++) {
            text text{};
            const char* word = words_[rand() % words_.size()];
            text.offset = static_cast<uint32_t>(geometry_text.indices.size());
            font_.RenderText(word, geometry_text, text.width, text.height);
            text.count = static_cast<uint32_t>(geometry_text.indices.size() - text.offset);
            text.color = {
                (rand() % 256) / 255.0,
                (rand() % 256) / 255.0,
                (rand() % 256) / 255.0
            };
            text.position = {
                rand() % (window_width - text.width),
                rand() % (window_height - font_.height_)
            };
            texts_.push_back(text);
        }

        render_engine_.UpdateDynamicIndexedPrimitive<Vertex_Text, uint32_t>(
            geometry_text.vertices.data(),
            static_cast<uint32_t>(geometry_text.vertices.size()),
            geometry_text.indices.data(),
            static_cast<uint32_t>(geometry_text.indices.size()),
            primitive_);

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

        render_engine_.BindPrimitive(command_buffer, primitive_);

        for (auto& text : texts_) {
            push_constants_.color = text.color;
            push_constants_.position = text.position;
            vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
            vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
            vkCmdDrawIndexed(command_buffer, text.count, 1, text.offset, 0, 0);
        }

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

    struct text {
        uint32_t offset;
        uint32_t count;
        uint32_t width;
        uint32_t height;
        glm::vec3 color{};
        glm::vec2 position{};
    };

    std::vector<text> texts_;

    IndexedPrimitive primitive_{};

    std::vector<const char*> words_ = {
        "acceptable",
        "accessible",
        "adhesive",
        "admire",
        "advise",
        "appliance",
        "arrogant",
        "bawdy",
        "behave",
        "bell",
        "best",
        "breath",
        "cable",
        "cake",
        "carve",
        "cemetery",
        "comb",
        "comfortable",
        "crown",
        "curve",
        "decorate",
        "depend",
        "disagreeable",
        "disastrous",
        "discover",
        "discreet",
        "disillusioned",
        "dog",
        "draconian",
        "endurable",
        "entertain",
        "ethereal",
        "expect",
        "fang",
        "fax",
        "fertile",
        "first",
        "fish",
        "front",
        "grey",
        "grouchy",
        "hilarious",
        "hug",
        "impress",
        "injure",
        "ink",
        "invent",
        "irritate",
        "join",
        "knife",
        "lamentable",
        "lick",
        "likeable",
        "lying",
        "marked",
        "mist",
        "mouth",
        "nebulous",
        "noise",
        "numerous",
        "occur",
        "old",
        "overrated",
        "payment",
        "peel",
        "prepare",
        "preserve",
        "public",
        "punishment",
        "quarter",
        "quizzical",
        "rainy",
        "rightful",
        "salt",
        "scare",
        "scream",
        "short",
        "sick",
        "signal",
        "sock",
        "sofa",
        "soup",
        "stiff",
        "stingy",
        "strip",
        "supply",
        "suspect",
        "table",
        "tawdry",
        "temporary",
        "tenuous",
        "texture",
        "thunder",
        "trade",
        "treatment",
        "two",
        "wax",
        "wire",
        "wish",
        "wistful"
    };

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

        VkDeviceSize vertex_size = static_cast<VkDeviceSize>(VERTEX_COUNT * sizeof(Vertex_Text));
        VkDeviceSize index_size = static_cast<VkDeviceSize>(INDEX_COUNT * sizeof(uint32_t));
        render_engine_.AllocateDynamicIndexedPrimitive<Vertex_Text, uint32_t>(vertex_size, index_size, primitive_);
    }
};
