// C:\important\quiet\n\mimita-public\mimita-public\src\utils\mesh_utils.h

/**
 * dec 3 2025 
 * this file does math using the mesh for hitbox we have 
 * to do collisions stuff i think
 */

#pragma once
#include "../map/map_common.h"
#include <glm/glm.hpp>

inline void computeMeshBounds(const Mesh& mesh, glm::vec3& minOut, glm::vec3& maxOut)
{
    if (mesh.verts.empty())
    {
        minOut = maxOut = glm::vec3(0);
        return;
    }

    glm::vec3 mn = mesh.verts[0].pos;
    glm::vec3 mx = mn;

    for (const auto& v : mesh.verts)
    {
        mn.x = std::min(mn.x, v.pos.x);
        mn.y = std::min(mn.y, v.pos.y);
        mn.z = std::min(mn.z, v.pos.z);

        mx.x = std::max(mx.x, v.pos.x);
        mx.y = std::max(mx.y, v.pos.y);
        mx.z = std::max(mx.z, v.pos.z);
    }

    minOut = mn;
    maxOut = mx;
}
