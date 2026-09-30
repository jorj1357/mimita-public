// 2026-09-30
/* purpose
* Define the GL-free "import an authored GLB as a destructible mesh" owner.
* Parses a binary GLB, bakes the node hierarchy, reads POSITION + TEXCOORD_0,
* carries the per-primitive material index, flips inverted winding, recenters to
* the local AABB, and validates the result as a closed manifold before any
* gameplay uses it. Results are cached per resolved path.
* Does NOT upload textures or touch GL, own cut history, or render.
*/

#pragma once

#include <string>

#include <glm/glm.hpp>

#include "impact/boolean-mesh.h"

namespace MimitaImpact {

struct DestructibleMeshLoad
{
    bool success = false;

    // Local-space triangle soup, recentered so the AABB center is the origin.
    BooleanMesh mesh;

    // Local AABB half size after recentering.
    glm::vec3 halfExtents{0.5f};

    // Human-readable reason when `success` is false.
    std::string error;
};

// Loads and validates one GLB. Safe with no GL context; a rejected mesh is
// cached too, and the reason is returned instead of being sent into gameplay.
DestructibleMeshLoad loadDestructibleMeshFromGLB(const std::string& path);

} // namespace MimitaImpact