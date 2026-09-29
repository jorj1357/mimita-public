// 2026-09-29
/* purpose
* Build the surface of a destructible axis-aligned box directly on its six faces.
* A sphere cut punches an N-gon rim out of every face it crosses; a cut that
* passes clean through an axis is joined by an inward-facing tube between the two
* opposite rims, so the opening is real geometry (walkable), not a visual hole.
* A cut that does not pass through leaves a capped pocket instead.
* An uncut box is exactly 12 triangles and every hole adds a bounded number more,
* so the collision mesh stays cheap no matter how many shots land.
* Does NOT own cut history or decide cut size (destructible-geometry / impact-system).
*/

#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "map/map_common.h"          // Vertex
#include "physics/physics-types.h"   // CollisionTriangle

namespace MimitaImpact {

struct DestructionCutSphere;

// Emits outward-wound triangles for `halfExtents` (box centred at the local
// origin) minus every stored sphere cut. `renderVertices` carries per-face UVs
// so the uncut mesh looks identical to the textured box.
void buildDestructibleBoxSurface(
    const glm::vec3& halfExtents,
    const std::vector<DestructionCutSphere>& cuts,
    std::vector<Vertex>& outRenderVertices,
    std::vector<CollisionTriangle>& outCollisionTriangles);

} // namespace MimitaImpact
