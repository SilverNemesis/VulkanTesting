#pragma once

#include <thread>
#include <atomic>

#include "Math.h"
#include "Utility.h"
#include "Scene.h"
#include "Camera.h"
#include "RenderEngine.h"
#include "Geometry.h"
#include "Geometry_Texture.h"

static const char* MODEL_PATH = "models/chalet.obj";
static const char* TEXTURE_PATH = "textures/chalet.jpg";

class ModelScene : public Scene {
public:
    ModelScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void OnQuit() {
        if (startup_) {
            vkDeviceWaitIdle(render_engine_.device_);

            render_engine_.DestroyGraphicsPipeline(texture_graphics_pipeline_);
            render_engine_.DestroyDescriptorSet(texture_descriptor_set_);
            render_engine_.DestroyUniformBuffer(texture_uniform_buffer_);

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
        camera_.Update(key_state, mouse_capture, mouse_x, mouse_y);

        static float total_time;
        total_time += 4.0f / 1000.0f;

        uniform_buffer_.model = glm::mat4(1.0f);
        uniform_buffer_.model = glm::translate(uniform_buffer_.model, glm::vec3(0.0f, 0.0f, 0.0f));
        uniform_buffer_.model = glm::rotate(uniform_buffer_.model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_.model = glm::rotate(uniform_buffer_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_.view = camera_.camera_.view_matrix;
        uniform_buffer_.proj = camera_.camera_.projection_matrix;
    }

    bool EventHandler(const SDL_Event* event) {
        return false;
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

        if (model_loaded_) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->graphics_pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->pipeline_layout, 0, 1, &texture_descriptor_set_->descriptor_sets[image_index], 0, nullptr);
            render_engine_.DrawPrimitive(command_buffer, primitive_);
        }

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.UpdateUniformBuffer(texture_uniform_buffer_, image_index, &uniform_buffer_);

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);
    }

private:
    RenderEngine& render_engine_;
    bool startup_ = false;

    std::shared_ptr<RenderEngine::UniformBuffer> texture_uniform_buffer_{};
    std::shared_ptr<RenderEngine::DescriptorSet> texture_descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> texture_graphics_pipeline_{};
    std::shared_ptr<RenderEngine::RenderPass> render_pass_{};

    Camera camera_{render_engine_};

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    UniformBufferObject uniform_buffer_{};

    std::thread thread_object_;
    std::atomic<bool> model_loaded_ = false;
    IndexedPrimitive primitive_{};
    TextureSampler texture_{};

    void Startup() {
        render_pass_ = render_engine_.CreateRenderPass();

        {
            texture_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(UniformBufferObject));

            texture_descriptor_set_ = render_engine_.CreateDescriptorSet({texture_uniform_buffer_}, 1);

            texture_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                "shaders/texture/vert.spv",
                "shaders/texture/frag.spv",
                {},
                Vertex_Texture::getBindingDescription(),
                Vertex_Texture::getAttributeDescriptions(),
                texture_descriptor_set_,
                0,
                true,
                false,
                false,
                false
            );
        }

        render_engine_.LoadTexture(TEXTURE_PATH, texture_);

        render_engine_.UpdateDescriptorSets(texture_descriptor_set_, {texture_});

        thread_object_ = std::thread([this]() {
            std::vector<Vertex_Texture> vertices{};
            std::vector<uint32_t> indices{};
            Utility::LoadModel(MODEL_PATH, vertices, indices);
            render_engine_.CreateIndexedPrimitive<Vertex_Texture, uint32_t>(vertices, indices, primitive_);
            model_loaded_ = true;
            });
    }
};
