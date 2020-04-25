#include "Utility.h"

#include <fstream>
#include <stdexcept>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <ft2build.h>
#include FT_FREETYPE_H

void Utility::LoadImage(const char* file_name, Image& texture) {
    int texture_channels;
    texture.pixels = stbi_load(file_name, &texture.texture_width, &texture.texture_height, &texture_channels, STBI_rgb_alpha);

    if (!texture.pixels) {
        throw std::runtime_error("failed to load texture image");
    }
}

void Utility::FreeImage(Image& texture) {
    stbi_image_free(texture.pixels);
}

void Utility::LoadModel(const char* file_name, std::vector<Vertex_Texture>& vertices, std::vector<uint32_t>& indices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file_name)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex_Texture, uint32_t, Vertex_Texture_Hash> unique_vertices = {};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex_Texture vertex = {};

            vertex.pos = {
                attrib.vertices[static_cast<size_t>(index.vertex_index) * 3 + 0],
                attrib.vertices[static_cast<size_t>(index.vertex_index) * 3 + 1],
                attrib.vertices[static_cast<size_t>(index.vertex_index) * 3 + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[static_cast<size_t>(index.texcoord_index) * 2 + 0],
                1.0f - attrib.texcoords[static_cast<size_t>(index.texcoord_index) * 2 + 1]
            };

            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(unique_vertices[vertex]);
        }
    }
}

std::vector<unsigned char> Utility::ReadFile(const std::string& file_name) {
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(std::string{"failed to open file "}+file_name);
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<unsigned char> buffer(file_size);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    file.close();

    return buffer;
}

void Utility::LoadFontImage(const char* file_name, uint32_t font_size, FontImage& font_image, float& font_image_size, std::map<unsigned char, FontCharacter>& character_map) {
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
        font_image.width = size;
        font_image.height = size;
        font_image.pixels = new unsigned char[font_image.width * font_image.height];
        memset(font_image.pixels, 0, font_image.width * font_image.height);

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

            if (x + b.width > font_image.width) {
                x = 0;
                y = my + 1;
            }

            if (y + b.rows > font_image.height) {
                resize = true;
                continue;
            }

            unsigned char* src = b.buffer;
            unsigned char* dst = font_image.pixels + y * font_image.width + x;

            for (uint32_t r = 0; r < b.rows; r++) {
                memcpy(dst, src, b.width);
                dst += font_image.width;
                src += b.width;
            }

            character_map[i] = {(uint16_t)x, (uint16_t)y, (uint8_t)(face->glyph->advance.x >> 6), (uint8_t)b.width, (uint8_t)b.rows, (uint8_t)face->glyph->bitmap_left, (uint8_t)face->glyph->bitmap_top};

            x += b.width + 1;

            if (y + b.rows > my) {
                my = y + b.rows;
            }
        }

        if (!resize) {
            break;
        }

        character_map.clear();
        delete[] font_image.pixels;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    font_image_size = static_cast<float>(size);
}

void Utility::FreeFontImage(FontImage& font) {
    delete[] font.pixels;
}
