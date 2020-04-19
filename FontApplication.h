#pragma once

#include <map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "Utility.h"
#include "RenderEngine.h"
#include "RenderPipeline.h"
#include "Geometry_Text.h"

struct Font {
    float size;
    TextureSampler texture;
    std::map<unsigned char, Utility::FontCharacter> characters;
};

class FontApplication {
public:
    void Startup(RenderApplication* render_application, int window_width, int window_height) {
        window_width_ = window_width;
        window_height_ = window_height;

        render_engine_.Initialize(render_application);

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/text/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/text/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_text_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), sizeof(glm::vec3), 1, 2, true);
        }

        LoadFont("fonts/Inconsolata/Inconsolata-Regular.ttf", 36, font_1_);
        LoadFont("fonts/katakana/katakana.ttf", 48, font_2_);

        render_pipeline_text_.UpdateDescriptorSets(0, {font_1_.texture});
        render_pipeline_text_.UpdateDescriptorSets(1, {font_2_.texture});

        std::vector<glm::vec2> texture_coordinates = {
            {0, 1},
            {1, 1},
            {1, 0},
            {0, 0}
        };

        std::vector<glm::vec2> vertices{};
        std::vector<std::vector<uint32_t>> faces{};

        const char* text = "Hello world!";

        Geometry_Text geometry_text_1{};
        RenderText(font_1_, text, 0, 200, geometry_text_1);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_1.vertices, geometry_text_1.indices, font_primitive_1_);

        Geometry_Text geometry_text_2{};
        RenderText(font_2_, text, 0, 400, geometry_text_2);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_2.vertices, geometry_text_2.indices, font_primitive_2_);

        UpdateProjection();
    }

    void Shutdown() {
        vkDeviceWaitIdle(render_engine_.device_);
        render_pipeline_text_.Destroy();
        render_engine_.DestroyIndexedPrimitive(font_primitive_1_);
        render_engine_.DestroyIndexedPrimitive(font_primitive_2_);
        DestroyFont(font_1_);
        DestroyFont(font_2_);
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

        RenderPipeline& render_pipeline = render_pipeline_text_;
        const VkDescriptorSet& descriptor_set_1 = render_pipeline.GetDescriptorSet(image_index, 0);
        const VkDescriptorSet& descriptor_set_2 = render_pipeline.GetDescriptorSet(image_index, 1);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);

        VkBuffer vertex_buffers_1[] = {font_primitive_1_.vertex_buffer_};
        VkDeviceSize offsets_1[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
        vkCmdBindIndexBuffer(command_buffer, font_primitive_1_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set_1, 0, nullptr);
        glm::vec3 color_1{1.0, 0.0, 0.0};
        vkCmdPushConstants(command_buffer, render_pipeline.pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(color_1), &color_1);
        vkCmdDrawIndexed(command_buffer, font_primitive_1_.index_count_, 1, 0, 0, 0);

        VkBuffer vertex_buffers_2[] = {font_primitive_2_.vertex_buffer_};
        VkDeviceSize offsets_2[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_2, offsets_2);
        vkCmdBindIndexBuffer(command_buffer, font_primitive_2_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set_2, 0, nullptr);
        glm::vec3 color_2{0.0, 0.0, 1.0};
        vkCmdPushConstants(command_buffer, render_pipeline.pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(color_2), &color_2);
        vkCmdDrawIndexed(command_buffer, font_primitive_2_.index_count_, 1, 0, 0, 0);

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
        render_pipeline_text_.Reset();
    }

    void PipelineRebuild() {
        render_pipeline_text_.Rebuild();
        render_pipeline_text_.UpdateDescriptorSets(0, {font_1_.texture});
        render_pipeline_text_.UpdateDescriptorSets(1, {font_2_.texture});
        UpdateProjection();
    }

private:
    int window_width_;
    int window_height_;

    RenderEngine render_engine_{1};
    RenderPipeline render_pipeline_text_{render_engine_, Vertex_Text::getBindingDescription(), Vertex_Text::getAttributeDescriptions(), 0};

    struct UniformBufferObject {
        glm::mat4 proj;
    };

    UniformBufferObject uniform_buffer_{};

    Font font_1_{};
    Font font_2_{};

    IndexedPrimitive font_primitive_1_{};
    IndexedPrimitive font_primitive_2_{};

    void UpdateProjection() {
        uniform_buffer_.proj = glm::ortho(0.0f, static_cast<float>(window_width_), static_cast<float>(window_height_), 0.0f);
        render_pipeline_text_.UpdateUniformBuffers(&uniform_buffer_);
    }

    void LoadTexture(const char* fileName, TextureSampler& texture_sampler) {
        Utility::Image texture;
        Utility::LoadImage(fileName, texture);

        render_engine_.CreateTexture(texture.pixels, texture.texture_width, texture.texture_height, texture_sampler);

        Utility::FreeImage(texture);
    }

    void RenderText(Font& font, const char* text, int x, int y, Geometry_Text& geometry) {
        std::vector<uint32_t> face = {0, 1, 2, 3};

        for (const char* cur = text; *cur != 0; cur++) {
            Utility::FontCharacter ch = font.characters[*cur];

            float l = static_cast<float>(x + ch.dx);
            float r = static_cast<float>(x + ch.dx + ch.w);
            float t = static_cast<float>(y + ch.dy - ch.h);
            float b = static_cast<float>(y + ch.dy);

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

            x += ch.a;
        }
    }

    int32_t GetTextLength(Font& font, const char* text) {
        int32_t size = 0;
        for (const char* cur = text; *cur != 0; cur++) {
            Utility::FontCharacter ch = font.characters[*cur];
            size += ch.a;
        }
        return size;
    }

    void LoadFont(const char* file_name, uint32_t font_size, Font& font) {
        Utility::FontImage font_image;
        Utility::LoadFontImage(file_name, font_size, font_image, font.size, font.characters);
        render_engine_.CreateAlphaTexture(font_image.pixels, font_image.width, font_image.height, font.texture);
        Utility::FreeFontImage(font_image);
    }

    void DestroyFont(Font& font) {
        render_engine_.DestroyTexture(font.texture);
        font.characters.clear();
    }
};
