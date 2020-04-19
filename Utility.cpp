#include "Utility.h"

#include <fstream>
#include <stdexcept>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void Utility::LoadImage(const char* fileName, Image& texture) {
    int texture_channels;
    texture.pixels = stbi_load(fileName, &texture.texture_width, &texture.texture_height, &texture_channels, STBI_rgb_alpha);

    if (!texture.pixels) {
        throw std::runtime_error("failed to load texture image");
    }
}

void Utility::FreeImage(Image& texture) {
    stbi_image_free(texture.pixels);
}

void Utility::LoadModel(const char* fileName, std::vector<Vertex_Texture>& vertices, std::vector<uint32_t>& indices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fileName)) {
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
