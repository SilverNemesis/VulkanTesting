#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan.h>

struct Vertex_Texture {
    glm::vec3 pos;
    glm::vec2 texCoord;

    bool operator==(const Vertex_Texture& other) const {
        return pos == other.pos && texCoord == other.texCoord;
    }

    static VkVertexInputBindingDescription getBindingDescription() {
        static VkVertexInputBindingDescription bindingDescription = {0, sizeof(Vertex_Texture), VK_VERTEX_INPUT_RATE_VERTEX};
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_Texture, pos)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex_Texture, texCoord)}
            }};
        return attributeDescriptions;
    }
};

struct Vertex_Texture_Hash {
    size_t operator()(Vertex_Texture const& vertex) const {
        return std::hash<glm::vec3>()(vertex.pos) ^ std::hash<glm::vec2>()(vertex.texCoord);
    }
};

class Geometry_Texture {
public:
    std::vector<Vertex_Texture> vertices;
    std::vector<uint32_t> indices;

    void AddFaces(std::vector<glm::vec3>& vertices, std::vector<std::vector<uint32_t>>& faces, std::vector<glm::vec2>& texture_coords) {
        for (auto face : faces) {
            AddFace(vertices, face, texture_coords);
        }
    }

private:
    void AddFace(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& face, std::vector<glm::vec2>& texture_coords) {
        switch (face.size()) {
        case 3:
            AddTriangle(texture_coords, vertices[face[0]], vertices[face[1]], vertices[face[2]]);
            break;
        case 4:
            AddSquare(texture_coords, vertices[face[0]], vertices[face[1]], vertices[face[2]], vertices[face[3]]);
            break;
        case 5:
            AddPentagon(texture_coords, vertices[face[0]], vertices[face[1]], vertices[face[2]], vertices[face[3]], vertices[face[4]]);
            break;
        default:
            throw std::runtime_error(std::string{"faces with "} +std::to_string(face.size()) + std::string{" vertices are not supported"});
        }
    }

    void AddTriangle(std::vector<glm::vec2>& texture_coords, glm::vec3& vertex_0, glm::vec3& vertex_1, glm::vec3& vertex_2) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({vertex_0, texture_coords[0]});
        vertices.push_back({vertex_1, texture_coords[1]});
        vertices.push_back({vertex_2, texture_coords[2]});
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    void AddSquare(std::vector<glm::vec2>& texture_coords, glm::vec3& vertex_0, glm::vec3& vertex_1, glm::vec3& vertex_2, glm::vec3& vertex_3) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({vertex_0, texture_coords[0]});
        vertices.push_back({vertex_1, texture_coords[1]});
        vertices.push_back({vertex_2, texture_coords[2]});
        vertices.push_back({vertex_3, texture_coords[3]});
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    void AddPentagon(std::vector<glm::vec2>& texture_coords, glm::vec3& vertex_0, glm::vec3& vertex_1, glm::vec3& vertex_2, glm::vec3& vertex_3, glm::vec3& vertex_4) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({vertex_0, texture_coords[0]});
        vertices.push_back({vertex_1, texture_coords[1]});
        vertices.push_back({vertex_2, texture_coords[2]});
        vertices.push_back({vertex_3, texture_coords[3]});
        vertices.push_back({vertex_4, texture_coords[4]});
        indices.push_back(base + 0);
        indices.push_back(base + 3);
        indices.push_back(base + 4);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
};
