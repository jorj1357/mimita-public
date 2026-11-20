// 2026-11-19
/* purpose
* own render mesh vertices, faces, batches, and mesh-bearing authored GLB nodes
* keep this stable vocabulary independent of map-authoring bases and config
* DOES NOT own map config parsing, chunks, collision, or editor state
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Face {
    std::vector<Vertex> verts;
    int texID = 0;
};

// One authored mesh-bearing GLB node. The loader bakes the node transform into
// renderVertices, so the editor can move the object by applying a delta to this
// contiguous vertex range and still reuse the normal map collision builder.
struct MapGLBObject {
    std::string id;
    std::string name;
    int nodeIndex = -1;
    glm::mat4 transform{1.0f};
    glm::vec3 basePosition{0.0f};
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    std::string materialName;
    bool hidden = false;
    bool collision = true;
    uint32_t collisionSolidId = 0;
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
        std::uint32_t texture = 0;
        size_t first = 0;
        size_t count = 0;
        glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        float opacity = 1.0f;
        bool doubleSided = false;
        glm::vec3 boundsMin{std::numeric_limits<float>::max()};
        glm::vec3 boundsMax{std::numeric_limits<float>::lowest()};
        bool hasBounds = false;
    };

    std::vector<Batch> batches;
    std::vector<std::uint32_t> ownedTextures;
};
