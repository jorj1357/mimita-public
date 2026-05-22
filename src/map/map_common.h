// C:\important\go away v5\s\mimita-v5\src\map\map_common.h

#pragma once
// make this cross platform happy friendly
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Face {
    std::vector<Vertex> verts;
    int texID = 0; // NEW — random texture index
};

struct Mesh {
    std::vector<Face> faces;
    std::vector<Vertex> verts; // keep legacy for VAO creation
};

// do NOT define struct chunk here
