// C:\important\go away v5\s\mimita-v5\src\map\map_common.h

#pragma once
// make this cross platform happy friendly
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <glad/glad.h>

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

    struct Batch {
        int materialIndex = -1;
        std::string materialName = "default";
        GLuint texture = 0;
        size_t first = 0;
        size_t count = 0;
    };

    std::vector<Batch> batches;
};

// do NOT define struct chunk here
