#pragma once

#include <map>

#include "Utility.h"
#include "RenderEngine.h"
#include "Geometry_Text.h"

class Font {
public:
    uint32_t height_{};
    TextureSampler texture_{};

    Font(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void Initialize(const char* file_name, uint32_t font_size) {
        Utility::FontImage font_image;
        Utility::LoadFontImage(file_name, font_size, font_image, size_, height_, characters_);
        render_engine_.CreateAlphaTexture(font_image.pixels, font_image.width, font_image.height, texture_);
        Utility::FreeFontImage(font_image);
    }

    void Destroy() {
        render_engine_.DestroyTexture(texture_);
        characters_.clear();
    }

    void GetSize(const char* text, uint32_t& width, uint32_t& height) {
        width = 0;
        height = 0;

        for (const char* cur = text; *cur != 0; cur++) {
            Utility::FontCharacter ch = characters_[*cur];

            uint32_t h = static_cast<uint32_t>(ch.h);

            if (h > height) {
                height = h;
            }

            width += ch.ax;
        }
    }

    void RenderText(const char* text, Geometry_Text& geometry, uint32_t& width, uint32_t& height) {
        std::vector<uint32_t> face = {0, 1, 2, 3};

        width = 0;
        height = 0;

        uint32_t offset = 0;

        for (const char* cur = text; *cur != 0; cur++) {
            Utility::FontCharacter ch = characters_[*cur];

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

            l = static_cast<float>(ch.x) / size_;
            r = static_cast<float>(ch.x + ch.w) / size_;
            t = static_cast<float>(ch.y) / size_;
            b = static_cast<float>(ch.y + ch.h) / size_;

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

    void RenderTextVertical(const char* text, Geometry_Text& geometry, uint32_t& width, uint32_t& height) {
        std::vector<uint32_t> face = {0, 1, 2, 3};

        width = 0;
        height = 0;

        uint32_t offset = 0;

        for (const char* cur = text + strlen(text) - 1; cur >= text; cur--) {
            Utility::FontCharacter ch = characters_[*cur];

            uint32_t w = static_cast<uint32_t>(ch.ax);

            if (w > width) {
                width = w;
            }

            height += height_;

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

            l = static_cast<float>(ch.x) / size_;
            r = static_cast<float>(ch.x + ch.w) / size_;
            t = static_cast<float>(ch.y) / size_;
            b = static_cast<float>(ch.y + ch.h) / size_;

            std::vector<glm::vec2> texture_coords = {
                {l, b},
                {r, b},
                {r, t},
                {l, t}
            };

            geometry.AddFace(vertices, face, texture_coords);

            offset += height_;
        }
    }

private:
    RenderEngine& render_engine_;
    float size_{};
    std::map<unsigned char, Utility::FontCharacter> characters_{};
};
