// 2026-09-30
/* purpose
* Compute rigid-body mass properties of one closed triangle mesh in its own
* local space: volume, center of mass, and inertia. This is the single owner of
* mesh-derived mass properties so destructible geometry and any future authored
* mesh shape measure the same way.
* Uses the signed tetrahedron (divergence) integrals, so an inward-wound cavity
* wall subtracts exactly like the boolean result that produced it.
* Stores the inertia as a local-axis diagonal; the full tensor and principal
* axes are not modeled yet.
* Does NOT own gameplay mass/density values and does NOT read or write entities.
*/

#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "physics/physics-types.h"

struct MeshMassProperties
{
    bool valid = false;

    // Signed-volume magnitude of the closed mesh, local units^3.
    float volume = 0.0f;

    // Local-space center of mass measured from the mesh origin.
    glm::vec3 centerOfMass{0.0f};

    // Inertia about the center of mass for unit density (mass == volume),
    // expressed on the local axes. Diagonal approximation; see header note.
    glm::vec3 unitInertiaDiagonal{1.0f};
};

// Integrates a closed, outward-wound triangle mesh (cavity walls wound inward).
// Returns valid=false when the mesh is empty, degenerate, or non-finite.
MeshMassProperties computeMeshMassProperties(
    const std::vector<CollisionTriangle>& triangles);