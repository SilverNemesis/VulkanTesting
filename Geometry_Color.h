#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan.h>

struct Vertex_Color {
    glm::vec3 pos;
    glm::vec3 color;

    bool operator==(const Vertex_Color& other) const {
        return pos == other.pos && color == other.color;
    }

    static VkVertexInputBindingDescription getBindingDescription() {
        static VkVertexInputBindingDescription bindingDescription = {0, sizeof(Vertex_Color), VK_VERTEX_INPUT_RATE_VERTEX};
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_Color, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_Color, color)}
            }};
        return attributeDescriptions;
    }
};

class Geometry_Color {
public:
    std::vector<Vertex_Color> vertices;
    std::vector<uint32_t> indices;

    void AddFaces(std::vector<glm::vec3>& vertices, std::vector<std::vector<uint32_t>>& faces, std::vector<glm::vec3>& colors) {
        size_t maxColor = colors.size();
        size_t color = 0;
        for (auto face : faces) {
            AddFace(vertices, face, colors[color++ % maxColor]);
        }
    }

private:
    void AddFace(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& face, glm::vec3& color) {
        switch (face.size()) {
        case 3:
            AddTriangle(color, vertices[face[0]], vertices[face[1]], vertices[face[2]]);
            break;
        case 4:
            AddSquare(color, vertices[face[0]], vertices[face[1]], vertices[face[2]], vertices[face[3]]);
            break;
        case 5:
            AddPentagon(color, vertices[face[0]], vertices[face[1]], vertices[face[2]], vertices[face[3]], vertices[face[4]]);
            break;
        default:
            throw std::runtime_error(std::string{"faces with "} +std::to_string(face.size()) + std::string{" vertices are not supported"});
        }
    }

    void AddTriangle(glm::vec3 color, glm::vec3& vertex_0, glm::vec3& vertex_1, glm::vec3& vertex_2) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({vertex_0, color});
        vertices.push_back({vertex_1, color});
        vertices.push_back({vertex_2, color});
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    void AddSquare(glm::vec3 color, glm::vec3& vertex_0, glm::vec3& vertex_1, glm::vec3& vertex_2, glm::vec3& vertex_3) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({vertex_0, color});
        vertices.push_back({vertex_1, color});
        vertices.push_back({vertex_2, color});
        vertices.push_back({vertex_3, color});
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    void AddPentagon(glm::vec3 color, glm::vec3& vertex_0, glm::vec3& vertex_1, glm::vec3& vertex_2, glm::vec3& vertex_3, glm::vec3& vertex_4) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({vertex_0, color});
        vertices.push_back({vertex_1, color});
        vertices.push_back({vertex_2, color});
        vertices.push_back({vertex_3, color});
        vertices.push_back({vertex_4, color});
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
