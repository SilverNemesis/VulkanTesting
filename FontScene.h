#pragma once

#include "Math.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Text.h"

#define WORD_COUNT              2048

class FontScene : public Scene {
public:
    FontScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void OnQuit() {
        if (startup_) {
            text_.Unregister();
        }
    }

    void OnEntry() {
        if (!startup_) {
            startup_ = true;
            render_pass_ = render_engine_.CreateRenderPass();
            text_.Register(render_pass_);
        }
    }

    void OnExit() {
    }

    void Update(std::array<bool, SDL_NUM_SCANCODES>& key_state, bool mouse_capture, int mouse_x, int mouse_y) {
    }

    bool EventHandler(const SDL_Event* event) {
        return false;
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

        text_.DrawBegin();

        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;

        for (int i = 0; i < WORD_COUNT; i++) {
            const char* word = words_[rand() % words_.size()];

            uint32_t width;
            uint32_t height;
            text_.GetSize(word, width, height);

            glm::vec3 color = {
                (rand() % 256) / 255.0,
                (rand() % 256) / 255.0,
                (rand() % 256) / 255.0
            };

            glm::vec2 position = {
                rand() % (window_width - width),
                rand() % (window_height - height)
            };

            text_.Draw(color, position, word);
        }

        text_.DrawEnd();

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

        text_.Render(command_buffer, image_index);

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
    Text text_{render_engine_};

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
};
