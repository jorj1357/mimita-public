#include "map_common.h"
#include "map-loader-mesh.h"

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

void generateTriangleNormals(std::vector<Vertex>& verts, size_t first, size_t count)
{
    for (size_t i = first; i + 2 < first + count; i += 3)
    {
        glm::vec3 a = verts[i + 0].pos;
        glm::vec3 b = verts[i + 1].pos;
        glm::vec3 c = verts[i + 2].pos;
        glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::length(n) < 0.0001f)
            n = {0, 0, 1};
        else
            n = glm::normalize(n);
        verts[i + 0].normal = n;
        verts[i + 1].normal = n;
        verts[i + 2].normal = n;
    }
}
