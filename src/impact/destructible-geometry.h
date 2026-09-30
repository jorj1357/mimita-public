// 2026-09-30
/* purpose
* Define the authoritative destructible-geometry record and its owner.
* A shape is stored as a canonical base mesh (box for now) plus an ordered list
* of boolean cutters; the generated triangles are a rebuildable cache, never the
* source of truth. Rebuilding replays the cut history through the MiMITA boolean
* wrapper (impact/boolean-mesh.h), so Manifold types never reach this layer.
* Meshing is lazy: nothing is generated until the first cut, so an untouched
* object keeps the cheap 12-triangle box the caller authored.
* Each rebuild also caches the mass properties of the remaining material
* (volume, center of mass, diagonal unit inertia) for the physics owner, so
* rigid-body mass is never re-integrated on the fixed tick.
* Does NOT render, send packets, or own rigid-body motion (PhysicalEntitySystem
* still owns collision + transforms).
* Does NOT decide whether a cut is created (ImpactSystem owns that).
*/

#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "impact/boolean-mesh.h"
#include "map/map_common.h"          // Vertex
#include "physics/physics-types.h"   // CollisionTriangle, AABB

namespace MimitaImpact {

// Why a rebuild decided the object should split into independent bodies.
enum class FractureReason : uint8_t
{
    None = 0,
    // A cut disconnected the material into more than one solid piece.
    DisconnectedComponent,
    // One piece is left, but its center of mass no longer projects over its
    // remaining support footprint (too much removed on one side).
    UnbalancedSupport
};

// Per-entity fracture tuning. Values are deliberately permissive by default so
// a normal crate only fractures when it genuinely loses its support.
struct FractureTuning
{
    bool enabled = true;

    // A disconnected piece only detaches when it is at least this fraction of
    // the original volume. Smaller slivers stay welded so a surface nick does
    // not shatter the object into dust.
    float minPieceVolumeFraction = 0.02f;

    // Unbalanced-support heuristic (destructible-world.md 21/24): the remaining
    // piece is effectively supported only on one side once its center of mass
    // has drifted off-center. Fires when:
    //   material was removed (remaining < maxRemainingFraction), and
    //   horizontal center-of-mass offset > comOffsetFraction * half extent.
    // Tuned aggressively on purpose; the first human pass should retune these.
    float maxRemainingFraction = 0.98f;
    float comOffsetFraction = 0.12f;

    // Cap on detached bodies produced by one fracture event (spec budget).
    uint32_t maxFragmentsPerEvent = 12;
};

// A decision returned by the fracture trigger. `primaryPiece` is the index of
// the piece that should keep the original entity; the rest are detached.
struct FractureDecision
{
    bool shouldFracture = false;
    FractureReason reason = FractureReason::None;
    size_t primaryPiece = 0;

    // Fraction 0..1 of removed mass on the unsupported side (diagnostics).
    float imbalance = 0.0f;
};

// One authoritative subtractive cut: a MiMITA boolean cutter plus the gameplay
// metadata needed to identify and reconcile it across prediction and authority.
struct DestructionCut
{
    uint64_t cutId = 0;
    BooleanCutter cutter;
    float damage = 0.0f;
    float energy = 0.0f;
    uint32_t materialId = 0;
    uint32_t sourceEntityId = 0;

    // Stable identity of the predictive source (projectile id / fire serial) so
    // the same shot is never applied twice.
    uint64_t predictionKey = 0;
};

// Authoritative compact destruction state embedded in PhysicalEntity.
struct DestructibleGeometry
{
    bool enabled = false;
    bool broken = false;
    float health = 100.0f;
    float maxHealth = 100.0f;

    // Source box half size (local space, centred at the origin).
    glm::vec3 halfExtents{0.5f};

    // Canonical base mesh (local space, centred at the origin) and the ordered
    // authoritative cut history. Both are the truth; the surface below is cache.
    BooleanMesh baseMesh;
    std::vector<DestructionCut> cuts;

    uint64_t geometryRevision = 0;
    uint64_t nextCutId = 1;
    uint32_t materialId = 0;

    // Cuts accepted into the authoritative history but not yet applied to the
    // surface. ImpactSystem enqueues here so a rapid burst becomes one batched
    // rebuild per flush instead of one O(triangle) rebuild per shot.
    uint32_t pendingCutCount = 0;

    // Incremental-rebuild session owned by the boolean wrapper. Caches the
    // running result so a new cut subtracts only the new cutter; game code
    // treats it as an opaque handle.
    uint64_t booleanSessionId = 0;

    // Diagnostics from the most recent rebuild. `baseVolume` is the authored
    // box volume so mass/moment-of-inertia scale with the material that remains.
    float baseVolume = 0.0f;
    float remainingVolume = 0.0f;
    uint32_t shellCount = 0;
    uint32_t componentCount = 0;
    BooleanError lastError = BooleanError::None;

    // Mass properties of the current cut surface, integrated once per rebuild
    // (never per tick). `unitInertiaDiagonal` is about the center of mass at
    // unit density, on the local axes. `massFromMesh` marks an authored mesh
    // base, where these values are valid before the first cut too.
    glm::vec3 massCenterOfMass{0.0f};
    glm::vec3 unitInertiaDiagonal{1.0f};
    bool massFromMesh = false;

    // Cached surface consumed by collision + rendering. Empty until the first
    // cut; the caller keeps its authored box mesh until then.
    std::vector<Vertex> renderVertices;
    std::vector<CollisionTriangle> collisionTriangles;

    // Diagnostics from the most recent rebuild's fracture check.
    FractureReason lastFractureReason = FractureReason::None;
    float lastImbalance = 0.0f;
};

// ── Signed-distance helpers (solid < 0, empty > 0). Test/diagnostic only. ──
float boxDistance(glm::vec3 p, glm::vec3 halfSize);
float sphereDistance(glm::vec3 p, glm::vec3 center, float radius);

// Original base box minus every stored cut (sphere or capsule).
float destructibleCrateDistance(const DestructibleGeometry& geometry,
                                glm::vec3 localPoint);

// Evaluates whether the current surface should fracture. Read-only; does not
// mutate the record or spawn bodies.
FractureDecision evaluateFracture(const DestructibleGeometry& geometry,
                                  const FractureTuning& tuning);

// ── Owner ───────────────────────────────────────────────────────────────
// Stores the base mesh + cut history and rebuilds the surface lazily by
// replaying the history through the boolean wrapper.
class DestructibleGeometrySystem
{
public:
    static DestructibleGeometrySystem& instance();

    // Resets the record for a fresh box (using geometry.materialId). Emits no
    // geometry: the caller's box mesh remains authoritative until addCut.
    void initialize(DestructibleGeometry& geometry, glm::vec3 halfExtents);

    // Resets the record to an authored closed mesh (for example imported from a
    // GLB). `halfExtents` is the local AABB half size. Unlike the box path this
    // immediately fills render/collision triangles and mesh-derived mass
    // properties, because there is no authored fallback mesh to keep.
    void initializeFromMesh(DestructibleGeometry& geometry, BooleanMesh baseMesh,
                            glm::vec3 halfExtents);

    // Drops the wrapper's cached running result for this record. Call when the
    // owning entity is removed so the wrapper does not retain geometry.
    void release(DestructibleGeometry& geometry);

    // Stores one cut and rebuilds by replaying the history. Returns 1 when the
    // surface was (re)generated, 0 when the record is disabled or the boolean
    // failed (in which case the cut is rolled back and lastError is set).
    int addCut(DestructibleGeometry& geometry, const DestructionCut& cut);

    // Appends one cut to the authoritative history WITHOUT rebuilding. The
    // surface is generated on the next flushQueuedCuts, batched with any other
    // queued cuts. Returns the stored cut id (0 when disabled).
    uint64_t enqueueCut(DestructibleGeometry& geometry, const DestructionCut& cut);

    // Applies every queued cut in one batched rebuild. `maxCutsThisFlush` > 0
    // caps how many queued cuts are applied this call (the rest wait for the
    // next flush) so a burst cannot blow one frame. Returns:
    //   1  surface changed,
    //   0  nothing queued / no material removed / failed (lastError set),
    //  -1  cut(s) were skipped because the triangle budget was exceeded.
    int flushQueuedCuts(DestructibleGeometry& geometry, uint32_t maxCutsThisFlush = 0);

    // Rebuilds the surface from the stored cuts (used after bulk changes).
    int rebuildAll(DestructibleGeometry& geometry);

    // Safety cap for the generated surface. Cuts that would exceed it are
    // rejected and logged rather than allowed to stall the game.
    size_t maxTrianglesPerEntity = 120000;

    // Fracture trigger tuning (hot-tunable in code for now).
    FractureTuning fractureTuning;

    // Separates the current surface into its independent solid pieces, largest
    // first. Only call after evaluateFracture reports shouldFracture; returns an
    // empty vector on failure so the caller can keep the object whole.
    std::vector<BooleanPiece> decomposePieces(
        const DestructibleGeometry& geometry) const;

private:
    DestructibleGeometrySystem() = default;

    // Applies up to `maxNewCuts` not-yet-applied cutters (0 = all) in one
    // batched rebuild. Returns true when the surface changed.
    bool rebuild(DestructibleGeometry& geometry, uint32_t maxNewCuts = 0);
};

} // namespace MimitaImpact
