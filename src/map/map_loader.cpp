// C:\important\go away v5\s\mimita-v5\src\map\map_loader.cpp

/**
 * jan 5 2026
 * The loader’s job is geometry only.
 * The loader’s job is geometry only.
 * The loader’s job is geometry only.
 * need to make it do that only and textures go elsewhere 
 */

#include "map_common.h"
// now its in /include so should work 
#include "tiny_obj_loader.h"
#include <random>
#include <ctime>
#include "texture_manager.h"
#include "utils/path_utils.h"
// dont do this idk jan 5 2026 
// extern TextureManager TEX;

Mesh loadOBJ(const std::string& path) {
    std::string resolvedPath = resolveAssetPath(path);
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(resolvedPath)) {
        throw std::runtime_error("Failed to load OBJ: " + resolvedPath + " (resolved from " + path + ")");
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    Mesh mesh;

    // texture pool
    std::mt19937 rng(static_cast<unsigned>(time(nullptr)));
    // making erros commentd out jan 5 2026 
    // std::uniform_int_distribution<int> dist(0, TEX.count() - 1);

    for (const auto& shape : shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
            Face f;
            f.texID = 0; // assign later
            // f.texID = dist(rng); // random texture
            for (int j = 0; j < 3; j++) {
                auto idx = shape.mesh.indices[i + j];
                glm::vec3 pos(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                );

                glm::vec3 norm(0);
                if (idx.normal_index >= 0) {
                    norm = glm::vec3(
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    );
                }

                glm::vec2 uv(0.0f);
                if (idx.texcoord_index >= 0 && !attrib.texcoords.empty()) {
                    uv = glm::vec2(
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
                    );
                }

                Vertex v{pos, norm, uv};
                f.verts.push_back(v);
                mesh.verts.push_back(v); // keep global vertex list for VAO
            }
            mesh.faces.push_back(f);
        }
    }
    return mesh;
}
