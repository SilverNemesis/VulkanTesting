#pragma once

#include <map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "Utility.h"
#include "RenderEngine.h"
#include "RenderPipeline.h"
#include "Geometry_Text.h"

struct Font {
    float size;

    TextureSampler font_texture_;

    struct Character {
        uint16_t x;
        uint16_t y;
        uint8_t a;
        uint8_t w;
        uint8_t h;
        uint8_t dx;
        uint8_t dy;
    };

    std::map<unsigned char, Character> characters_;
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
            render_pipeline_text_.Initialize(vertex_shader_module, fragment_shader_module, 0, sizeof(glm::vec3), 1, 2, true);
        }

        LoadFont("fonts/Inconsolata/Inconsolata-Regular.ttf", 36, font_1_);
        LoadFont("fonts/katakana/katakana.ttf", 48, font_2_);

        render_pipeline_text_.UpdateDescriptorSets(0, {font_1_.font_texture_});
        render_pipeline_text_.UpdateDescriptorSets(1, {font_2_.font_texture_});

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
        RenderText(font_1_, text, -GetTextLength(font_1_, text) - 10, 0, geometry_text_1);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_1.vertices, geometry_text_1.indices, font_primitive_1_);

        Geometry_Text geometry_text_2{};
        RenderText(font_2_, text, 10, 0, geometry_text_2);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_2.vertices, geometry_text_2.indices, font_primitive_2_);
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
        render_pipeline_text_.UpdateDescriptorSets(0, {font_1_.font_texture_});
        render_pipeline_text_.UpdateDescriptorSets(1, {font_2_.font_texture_});
    }

private:
    int window_width_;
    int window_height_;

    RenderEngine render_engine_{1};
    RenderPipeline render_pipeline_text_{render_engine_, Vertex_Text::getBindingDescription(), Vertex_Text::getAttributeDescriptions(), 0};

    Font font_1_{};
    Font font_2_{};

    IndexedPrimitive font_primitive_1_{};
    IndexedPrimitive font_primitive_2_{};

    void LoadTexture(const char* fileName, TextureSampler& texture_sampler) {
        Utility::Image texture;
        Utility::LoadImage(fileName, texture);

        render_engine_.CreateTexture(texture.pixels, texture.texture_width, texture.texture_height, texture_sampler);

        Utility::FreeImage(texture);
    }

    void RenderText(Font& font, const char* text, int x, int y, Geometry_Text& geometry) {
        float xscale = 1.0f / window_width_;
        float yscale = 1.0f / window_height_;

        std::vector<uint32_t> face = {3, 2, 1, 0};

        for (const char* cur = text; *cur != 0; cur++) {
            Font::Character ch = font.characters_[*cur];

            float l = static_cast<float>(x + ch.dx) * xscale;
            float r = static_cast<float>(x + ch.dx + ch.w) * xscale;
            float t = -static_cast<float>(y - ch.h + ch.dy) * yscale;
            float b = -static_cast<float>(y + ch.dy) * yscale;

            std::vector<glm::vec2> vertices = {
                {l, b},
                {r, b},
                {r, t},
                {l, t}
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
            Font::Character ch = font.characters_[*cur];
            size += ch.a;
        }
        return size;
    }

    void LoadFont(const char* file_name, uint32_t font_size, Font& font) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            throw std::runtime_error("unable to initialize font library");
        }

        FT_Face face;
        if (FT_New_Face(ft, file_name, 0, &face)) {
            throw std::runtime_error("unable to load font");
        }

        FT_Set_Pixel_Sizes(face, 0, font_size);

        uint32_t size;

        for (size = 128; size < 4096; size *= 2) {
            uint32_t width = size;
            uint32_t height = size;
            unsigned char* pixels = new unsigned char[width * height];
            memset(pixels, 0, width * height);

            bool resize = false;

            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t my = 0;

            for (int i = 0; i < 256; i++) {
                int index = FT_Get_Char_Index(face, i);

                if (index == 0) {
                    continue;
                }

                if (FT_Load_Glyph(face, index, FT_LOAD_RENDER)) {
                    throw std::runtime_error("failed to load glyph");
                }

                FT_Bitmap b = face->glyph->bitmap;

                if (x + b.width > width) {
                    x = 0;
                    y = my + 1;
                }

                if (y + b.rows > height) {
                    resize = true;
                    continue;
                }

                unsigned char* src = b.buffer;
                unsigned char* dst = pixels + y * width + x;

                for (uint32_t r = 0; r < b.rows; r++) {
                    memcpy(dst, src, b.width);
                    dst += width;
                    src += b.width;
                }

                font.characters_[i] = {(uint16_t)x, (uint16_t)y, (uint8_t)(face->glyph->advance.x >> 6), (uint8_t)b.width, (uint8_t)b.rows, (uint8_t)face->glyph->bitmap_left, (uint8_t)face->glyph->bitmap_top};

                x += b.width + 1;

                if (y + b.rows > my) {
                    my = y + b.rows;
                }
            }

            if (!resize) {
                render_engine_.CreateAlphaTexture(pixels, width, height, font.font_texture_);
                delete[] pixels;
                break;
            }

            font.characters_.clear();
            delete[] pixels;
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        font.size = static_cast<float>(size);
    }

    void DestroyFont(Font& font) {
        render_engine_.DestroyTexture(font.font_texture_);
    }
};
