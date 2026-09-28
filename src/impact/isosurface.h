// 2026-09-28
/* purpose
* Convert the destructible signed-distance field into triangles.
* Uses a table-free isosurface extractor (naive surface nets): one vertex per
* active cell, quads across sign-changing grid edges. It is watertight,
* deterministic, allocation-bounded, and needs no 256-entry lookup table, and
* it lives in the same isosurface family the plan allowed ("marching cubes or
* dual contouring") without hand-transcribing error-prone tables.
* Does NOT own cut history or chunk scheduling (destructible-geometry owns it).
*/

#pragma once

#include <cstddef>

#include <glm/glm.hpp>

#include "impact/destructible-geometry.h"

namespace MimitaImpact {

// Meshes one chunk of the object. Sample points come from a shared integer grid
// so neighbouring chunks agree on shared edges:
//   sample(ix,iy,iz) = gridOrigin + (baseIndex + (ix,iy,iz)) * cellSize
// `baseIndex` is the chunk's minimum owned cell index in the whole grid; the
// extractor additionally reads one cell of halo on the negative sides so quads
// on chunk borders are emitted exactly once (owned by this chunk).
// Triangles stop at `triangleBudget`; the mesh is flagged budgetExceeded after.
GeneratedDestructionMesh meshDestructibleChunk(
    const DestructibleGeometry& geometry,
    glm::ivec3 baseIndex,
    glm::ivec3 cellsPerAxis,
    float cellSize,
    const glm::vec3& gridOrigin,
    size_t triangleBudget);

} // namespace MimitaImpact
