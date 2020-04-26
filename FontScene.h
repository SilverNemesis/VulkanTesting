#pragma once

#include <map>

#include "Math.h"
#include "Utility.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Geometry_Text.h"

struct Font {
    float size;
    uint32_t height;
    TextureSampler texture;
    std::map<unsigned char, Utility::FontCharacter> characters;
};

class FontScene : public Scene {
public:
    FontScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void Shutdown() {
        if (startup_) {
            vkDeviceWaitIdle(render_engine_.device_);

            render_engine_.DestroyGraphicsPipeline(texture_graphics_pipeline_);

            render_engine_.DestroyDescriptorSet(texture_descriptor_set_);
            render_engine_.DestroyUniformBuffer(texture_uniform_buffer_);

            render_engine_.DestroyIndexedPrimitive(primitive_1_);
            render_engine_.DestroyIndexedPrimitive(primitive_2_);

            DestroyFont(font_);
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

        VkCommandBuffer& command_buffer = render_engine_.command_buffers_[image_index];

        uint32_t window_width = render_engine_.swapchain_extent_.width;
        uint32_t window_height = render_engine_.swapchain_extent_.height;

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

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->graphics_pipeline);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->pipeline_layout, 0, 1, &texture_descriptor_set_->descriptor_sets[image_index], 0, nullptr);

        push_constants_.color = {1.0, 0.0, 0.0};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
        push_constants_.position = {window_width / 2 - primitive_1_width_ / 2, window_height / 2 + font_.height / 2};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
        render_engine_.DrawPrimitive(command_buffer, primitive_1_);

        push_constants_.color = {0.5, 0.5, 1.0};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConstants, color), sizeof(push_constants_.color), &push_constants_.color);
        push_constants_.position = {window_width / 2 - primitive_2_width_ / 2, window_height / 2 - font_.height / 2};
        vkCmdPushConstants(command_buffer, texture_graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(PushConstants, position), sizeof(push_constants_.position), &push_constants_.position);
        render_engine_.DrawPrimitive(command_buffer, primitive_2_);

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        uniform_buffer_.proj = glm::ortho(0.0f, static_cast<float>(window_width), static_cast<float>(window_height), 0.0f);
        render_engine_.UpdateUniformBuffers(texture_uniform_buffer_, &uniform_buffer_);

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

    struct CameraMatrix {
        glm::mat4 proj;
    };

    CameraMatrix uniform_buffer_{};

    struct PushConstants {
        glm::vec3 color{};
        alignas(8) glm::vec2 position{};
    } push_constants_;

    Font font_{};

    IndexedPrimitive primitive_1_{};
    uint32_t primitive_1_width_{};
    uint32_t primitive_1_height_{};

    IndexedPrimitive primitive_2_{};
    uint32_t primitive_2_width_{};
    uint32_t primitive_2_height_{};

    void Startup() {
        render_pass_ = render_engine_.CreateRenderPass();

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/text/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/text/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());

            texture_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(CameraMatrix));

            texture_descriptor_set_ = render_engine_.CreateDescriptorSet({texture_uniform_buffer_}, 1);

            texture_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                vertex_shader_module,
                fragment_shader_module,
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

        LoadFont("fonts/Inconsolata/Inconsolata-Regular.ttf", 36, font_);

        render_engine_.UpdateDescriptorSets(texture_descriptor_set_, {font_.texture});

        std::vector<glm::vec2> texture_coordinates = {
            {0, 1},
            {1, 1},
            {1, 0},
            {0, 0}
        };

        std::vector<glm::vec2> vertices{};
        std::vector<std::vector<uint32_t>> faces{};

        Geometry_Text geometry_text_1{};
        RenderText(font_, "Hello world!", geometry_text_1, primitive_1_width_, primitive_1_height_);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_1.vertices, geometry_text_1.indices, primitive_1_);

        Geometry_Text geometry_text_2{};
        RenderText(font_, "Goodbye world  :(", geometry_text_2, primitive_2_width_, primitive_2_height_);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_2.vertices, geometry_text_2.indices, primitive_2_);
    }

    void LoadTexture(const char* file_name, TextureSampler& texture_sampler) {
        Utility::Image texture;
        Utility::LoadImage(file_name, texture);

        render_engine_.CreateTexture(texture.pixels, texture.texture_width, texture.texture_height, texture_sampler);

        Utility::FreeImage(texture);
    }

    void RenderText(Font& font, const char* text, Geometry_Text& geometry, uint32_t& width, uint32_t& height) {
        std::vector<uint32_t> face = {0, 1, 2, 3};

        width = 0;
        height = 0;

        uint32_t offset = 0;

        for (const char* cur = text; *cur != 0; cur++) {
            Utility::FontCharacter ch = font.characters[*cur];

            uint32_t h = static_cast<uint32_t>(ch.h);

            if (h > height) {
                height = h;
            }

            width += ch.ax;

            float l = static_cast<float>(offset + ch.dx);
            float r = static_cast<float>(offset + ch.dx + ch.w);
            float t = static_cast<float>(ch.dy - ch.h);
            float b = static_cast<float>(ch.dy);

            std::vector<glm::vec2> vertices = {
                {l, t},
                {r, t},
                {r, b},
                {l, b}
            };

            l = static_cast<float>(ch.x) / font.size;
            r = static_cast<float>(ch.x + ch.w) / font.size;
            t = static_cast<float>(ch.y) / font.size;
            b = static_cast<float>(ch.y + ch.h) / font.size;

            std::vector<glm::vec2> texture_coords = {
                {l, b},
                {r, b},
                {r, t},
                {l, t}
            };

            geometry.AddFace(vertices, face, texture_coords);

            offset += ch.ax;
        }
    }

    void RenderTextVertical(Font& font, const char* text, Geometry_Text& geometry, uint32_t& width, uint32_t& height) {
        std::vector<uint32_t> face = {0, 1, 2, 3};

        width = 0;
        height = 0;

        uint32_t offset = 0;

        for (const char* cur = text + strlen(text) - 1; cur >= text; cur--) {
            Utility::FontCharacter ch = font.characters[*cur];

            uint32_t w = static_cast<uint32_t>(ch.ax);

            if (w > width) {
                width = w;
            }

            height += font.height;

            float l = static_cast<float>(ch.dx);
            float r = static_cast<float>(ch.dx + ch.w);
            float t = static_cast<float>(offset + ch.dy - ch.h);
            float b = static_cast<float>(offset + ch.dy);

            std::vector<glm::vec2> vertices = {
                {l, t},
                {r, t},
                {r, b},
                {l, b}
            };

            l = static_cast<float>(ch.x) / font.size;
            r = static_cast<float>(ch.x + ch.w) / font.size;
            t = static_cast<float>(ch.y) / font.size;
            b = static_cast<float>(ch.y + ch.h) / font.size;

            std::vector<glm::vec2> texture_coords = {
                {l, b},
                {r, b},
                {r, t},
                {l, t}
            };

            geometry.AddFace(vertices, face, texture_coords);

            offset += font.height;
        }
    }

    void LoadFont(const char* file_name, uint32_t font_size, Font& font) {
        Utility::FontImage font_image;
        Utility::LoadFontImage(file_name, font_size, font_image, font.size, font.height, font.characters);
        render_engine_.CreateAlphaTexture(font_image.pixels, font_image.width, font_image.height, font.texture);
        Utility::FreeFontImage(font_image);
    }

    void DestroyFont(Font& font) {
        render_engine_.DestroyTexture(font.texture);
        font.characters.clear();
    }
};
