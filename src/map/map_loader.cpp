// C:\important\go away v5\s\mimita-v5\src\map\map_loader.cpp

/**
 * jan 25 2026
 * this is litearll onl used so that mimita character shows up
 * need to move that logic to another file that loads objs good
 * or whatever
 * idk
 * we cna just use the json loader to turn the mimita character obj into a json
 * then we can apply images at high frame rate for the facial expressions mhm
 * 
 */

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
    printf("OBJ path = %s\n", resolvedPath.c_str());

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(resolvedPath.c_str())) {
        printf("OBJ error: %s\n", reader.Error().c_str());
        return Mesh{};
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    Mesh mesh;
    for (const auto& shape : shapes) {
        for (auto idx : shape.mesh.indices) {
            Vertex v;
            v.pos = {
                attrib.vertices[3*idx.vertex_index+0],
                attrib.vertices[3*idx.vertex_index+1],
                attrib.vertices[3*idx.vertex_index+2]
            };
            mesh.verts.push_back(v);
        }
    }

    printf("OBJ verts = %zu\n", mesh.verts.size());
    return mesh;
}
