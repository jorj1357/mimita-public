// C:\important\go away v5\s\mimita-v5\src\map\map_common.h

#pragma once
// make this cross platform happy friendly
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <glad/glad.h>
#include <limits>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Face {
    std::vector<Vertex> verts;
    int texID = 0; // NEW — random texture index
};

// One authored mesh-bearing GLB node. The loader bakes the node transform into
// renderVertices, so the editor can move the object by applying a delta to this
// contiguous vertex range and still reuse the normal map collision builder.
struct MapGLBObject {
    std::string id;                 // stable per map: "node:<index>"
    std::string name;               // authored GLB node name
    int nodeIndex = -1;
    glm::mat4 transform{1.0f};      // current world transform (translation used by editor)
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    size_t vertexFirst = 0;
    size_t vertexCount = 0;
};

struct Mesh {
    std::vector<Face> faces;
    std::vector<Vertex> verts; // keep legacy for VAO creation
    std::vector<MapGLBObject> objects;

    struct Batch {
        int materialIndex = -1;
        std::string materialName = "default";
        GLuint texture = 0;
        size_t first = 0;
        size_t count = 0;
        bool doubleSided = false;   // GLTF material doubleSided flag
        glm::vec3 boundsMin{std::numeric_limits<float>::max()};
        glm::vec3 boundsMax{std::numeric_limits<float>::lowest()};
        bool hasBounds = false;
    };

    std::vector<Batch> batches;
    std::vector<GLuint> ownedTextures;
};

// do NOT define struct chunk here
