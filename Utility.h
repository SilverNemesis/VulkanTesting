#pragma once

#include <map>
#include <vector>

#include "Geometry_Texture.h"

namespace Utility {
    struct Image {
        int texture_width;
        int texture_height;
        unsigned char* pixels;
    };

    void LoadImage(const char* file_name, Image& texture);

    void FreeImage(Image& texture);

    struct FontCharacter {
        uint16_t x;
        uint16_t y;
        uint8_t ax;
        uint8_t w;
        uint8_t h;
        uint8_t dx;
        uint8_t dy;
    };

    struct FontImage {
        uint32_t width;
        uint32_t height;
        unsigned char* pixels;
    };

    void LoadFontImage(const char* file_name, uint32_t font_size, FontImage& font_image, float& font_image_size, uint32_t& height, std::map<unsigned char, FontCharacter>& character_map);

    void FreeFontImage(FontImage& font);

    void LoadModel(const char* file_name, std::vector<Vertex_Texture>& vertices, std::vector<uint32_t>& indices);

    std::vector<unsigned char> ReadFile(const std::string& file_name);
}
