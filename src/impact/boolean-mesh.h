// 2026-09-30
/* purpose
* Define the MiMITA-owned mesh/cutter vocabulary for true boolean destruction.
* Game code knows only these plain structures; this is the only header it sees.
* The implementation (boolean-mesh.cpp) is the single translation unit that
* includes the vendored Manifold headers, converts between MiMITA meshes and
* Manifold's MeshGL, performs the subtraction, and converts the result back.
* Does NOT own rigid-body motion, cut sizing, rendering, or networking.
* Does NOT expose manifold:: types to the rest of the game.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MimitaImpact {

// One output vertex. The mesh is authored as a triangle soup (3 vertices per
// triangle) so the flat-shaded surface, collision triangles, and render buffer
// all share one representation.
struct BooleanMeshVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec2 uv{0.0f};
    uint32_t materialId = 0;
};

// A closed triangle mesh in the object's local space. `indices` are triangle
// corners into `vertices`, wound CCW when viewed from outside.
struct BooleanMesh
{
    std::vector<BooleanMeshVertex> vertices;
    std::vector<uint32_t> indices;

    bool empty() const { return indices.empty(); }
    size_t triangleCount() const { return indices.size() / 3u; }
};

// Elementary cutter volume. Sphere is the first gameplay slice; Capsule is the
// swept path of a projectile and is built by the wrapper from primitives.
enum class BooleanCutterType : uint8_t
{
    Sphere = 0,
    Capsule = 1
};

struct BooleanCutter
{
    BooleanCutterType type = BooleanCutterType::Sphere;
    glm::vec3 localCenter{0.0f};
    glm::vec3 localDirection{0.0f, 0.0f, 1.0f}; // capsule axis, ignored for sphere
    float radius = 0.0f;
    float length = 0.0f; // capsule cylinder length between the cap centers
};

// Manifold error values mapped into MiMITA-owned categories so callers never
// depend on a Manifold enum.
enum class BooleanError : uint8_t
{
    None = 0,
    InvalidTarget,
    NotManifold,
    MissingPositionProperties,
    PropertiesWrongLength,
    InvalidCutter,
    EmptyResult,
    ResultTooLarge,
    Internal
};

struct BooleanCutResult
{
    bool success = false;

    // False when the subtract removed no material (the cutter was entirely
    // inside already-empty space). `mesh` is still valid, but it is identical
    // to the previous surface. Callers use this to avoid rebuilding collision,
    // render, and mass data for a no-op cut.
    bool changed = true;

    BooleanMesh mesh;
    float remainingVolume = 0.0f;
    uint32_t triangleCount = 0;

    // Connected surface shells as reported by Manifold (an interior cavity is a
    // separate negative-volume shell). Kept for diagnostics.
    uint32_t shellCount = 0;

    // Shells with positive volume: the number of genuinely separate solid
    // pieces. Fracture is NOT enabled in this slice; this is reported only.
    uint32_t componentCount = 0;

    BooleanError error = BooleanError::None;
    std::string message;
};

// Builds the canonical textured box (8 shared vertices, 12 triangles, planar
// XY UVs) centered at the local origin. This is the immutable base mesh that
// the authoritative cut history is applied to.
BooleanMesh buildBooleanBoxMesh(const glm::vec3& halfExtents, uint32_t materialId);

// Builds a box centered at `center` in local space (same conventions as
// buildBooleanBoxMesh). Used with booleanUnion to author compound bases.
BooleanMesh buildBooleanBoxMeshAt(const glm::vec3& center, const glm::vec3& halfExtents,
                                  uint32_t materialId);

// Unions closed meshes into one manifold. Empty result on failure. Used to
// author compound destructible bases (for example a shape with a thin waist).
BooleanMesh booleanUnion(const std::vector<BooleanMesh>& meshes, uint32_t materialId);

// Subtracts one cutter from `base`. `base` must already be the accumulated
// result of the previous cuts; the caller keeps the canonical base plus the cut
// history and can rebuild by replaying. Returns a fully self-describing result;
// on failure `success` is false and `error`/`message` are set, and the returned
// mesh is empty so invalid geometry is never handed to gameplay.
BooleanCutResult booleanSubtract(const BooleanMesh& base, const BooleanCutter& cutter);

// Replays the full ordered cut history against the canonical base and returns
// the accumulated result. This is the authoritative path: never feed a previous
// output back in as `base`, because Manifold output meshes have property seams
// that need merge vectors to re-import. Chaining in-memory avoids that.
BooleanCutResult booleanSubtractAll(const BooleanMesh& base,
                                    const std::vector<BooleanCutter>& cutters);

// Incremental form of the same authoritative replay. The wrapper caches the
// running in-memory result for `sessionId`, so a rebuild whose cut history only
// grew subtracts the new cutters instead of replaying every previous one.
// Behavior is identical to booleanSubtractAll: the result is always the
// canonical base minus the full ordered history. If the history shrank (a cut
// rolled back) or the session is unknown, it transparently rebuilds from the
// base. When two callers share a process (a listen server), calls are
// serialized by the wrapper. Pass 0 to opt out and use booleanSubtractAll.
//
// `simplifyTolerance` > 0 combines near-coplanar triangles after the
// subtraction (bounded triangle growth per hole). `maxTriangles` > 0 rejects a
// cut that would grow the mesh past the cap WITHOUT committing the running
// result (the caller keeps the previous surface and the cached session stays at
// the last valid state, so the next cut is still incremental). Both default to
// off for compatibility.
BooleanCutResult booleanSubtractIncremental(uint64_t sessionId,
                                            const BooleanMesh& base,
                                            const std::vector<BooleanCutter>& cutters,
                                            uint32_t maxTriangles = 0,
                                            double simplifyTolerance = 0.0);

// Drops the cached running result for `sessionId`. Call when a geometry record
// is re-initialized or its entity is removed so the wrapper does not retain it.
void booleanSessionRelease(uint64_t sessionId);

// Returns whether `mesh` would import as a valid closed manifold. Fills
// `reason` when it would not. Intended for tests and pre-flight validation.
// Coincident positions are welded via merge vectors before import, so a
// triangle soup with duplicated seam vertices can still be valid.
BooleanError booleanValidate(const BooleanMesh& mesh, std::string* reason = nullptr);

// One separated solid piece of a boolean result (one positive-volume shell).
struct BooleanPiece
{
    BooleanMesh mesh;          // closed, outward-wound, local space
    float volume = 0.0f;       // signed volume magnitude
    glm::vec3 centroid{0.0f};  // local-space centroid for velocity seeding
};

// Decomposes the canonical base minus the full ordered cut history into its
// separated solid pieces, largest volume first. A shape with a single connected
// solid returns exactly one piece. Interior cavities (negative-volume shells)
// are not returned; only matter. Used to fracture a cut object into independent
// bodies; returns an empty vector on failure.
std::vector<BooleanPiece> booleanDecomposePieces(const BooleanMesh& base,
                                                 const std::vector<BooleanCutter>& cutters);

} // namespace MimitaImpact
