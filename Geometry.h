#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Geometry {
    static void CreateCube(std::vector<glm::vec3>& vertices, std::vector<std::vector<uint32_t>>& faces) {
        const float r = .35f;

        vertices = {
            {-r, -r, -r},
            {-r, -r, r},
            {r, -r, r},
            {r, -r, -r},
            {-r, r, -r},
            {-r, r, r},
            {r, r, r},
            {r, r, -r}
        };

        faces = {
            {0, 3, 2, 1},
            {4, 5, 6, 7},
            {0, 1, 5, 4},
            {2, 3, 7, 6},
            {1, 2, 6, 5},
            {3, 0, 4, 7}
        };
    }
};

namespace Geometry2D {
    static void CreateSquare(float size, std::vector<glm::vec2>& vertices, std::vector<std::vector<uint32_t>>& faces) {
        float r = size / 2.0f;

        vertices = {
            {-r, r},
            {r, r},
            {r, -r},
            {-r, -r}
        };

        faces = {
            {0, 1, 2, 3}
        };
    }
};
