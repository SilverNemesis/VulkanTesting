#pragma once

#include <vector>

#include "Geometry_Texture.h"

namespace Utility {
    struct Image {
        int texture_width;
        int texture_height;
        unsigned char* pixels;
    };

    void LoadImage(const char* fileName, Image& texture);

    void FreeImage(Image& texture);

    void LoadModel(const char* fileName, std::vector<Vertex_Texture>& vertices, std::vector<uint32_t>& indices);

    std::vector<unsigned char> ReadFile(const std::string& file_name);
}
