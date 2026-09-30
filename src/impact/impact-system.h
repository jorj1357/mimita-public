// 2026-09-28
/* purpose
* Own the single generalized impact entry point. ImpactSystem turns a
* deterministic ImpactEvent into a target response: energy, material lookup,
* health damage, and a destructible-geometry cut.
* Does NOT own projectile flight, damage authority for actors, rendering,
* networking, or persistence.
* Does NOT accept crate-specific events; unsupported source/target pairs are
* safely ignored with a rate-limited diagnostic.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "impact/boolean-mesh.h"
#include "impact/impact-event.h"

struct PhysicalEntity;

namespace MimitaImpact {

struct ImpactResult
{
    bool applied = false;
    bool cutCreated = false;
    // True when the cut was accepted into the history but its surface has not
    // been rebuilt yet (queued for the next flush budget window).
    bool pending = false;
    float cutRadius = 0.0f;
    float damage = 0.0f;
    int chunksRebuilt = 0;
    uint32_t triangleCount = 0;
    uint32_t componentCount = 0;
    float remainingVolume = 0.0f;
    BooleanError error = BooleanError::None;

    // Fracture outcome. `fractured` is true when the hit split the object into
    // independent bodies; `fragmentCount` is how many new entities were spawned
    // (the hit entity keeps the largest piece).
    bool fractured = false;
    uint32_t fragmentCount = 0;
    uint32_t fragmentEntityIds[16] = {0};
};

class ImpactSystem
{
public:
    static ImpactSystem& instance();

    // Per-tick flush budget. At most this many queued cuts are applied to one
    // entity per tick, and the whole flush stops once it has spent this many
    // milliseconds. Tune from config later; kept here so the fixed tick can
    // call flushPendingCuts with a self-documenting budget.
    static constexpr uint32_t kMaxCutsPerEntityPerTick = 8;
    static constexpr float kCutBudgetMsPerTick = 2.0f;

    // Applies one impact. Returns what happened; safe to call every hit.
    ImpactResult submit(const ImpactEvent& event);

    // Applies queued cuts to every destructible entity, batched: one rebuild
    // per entity regardless of how many cuts are queued. `maxCutsPerEntity` > 0
    // caps how many queued cuts are applied this call so a burst cannot blow the
    // frame (the rest wait for the next flush). `budgetMs` > 0 stops early once
    // the measured wall time exceeds it. Call once per fixed tick.
    void flushPendingCuts(uint32_t maxCutsPerEntity = 0, float budgetMs = 0.0f);

    // Builds the destructible record for a freshly spawned entity and writes the
    // generated triangles back into the entity collision mesh. `materialId` is a
    // MimitaImpact material id (0 falls back to the default material).
    void initializeEntity(PhysicalEntity& entity, uint32_t materialId,
                          glm::vec3 halfExtents);

    // Builds the destructible record from an authored closed mesh (for example
    // imported from a GLB). Unlike the box path the entity needs no authored
    // fallback surface: render/collision triangles and mesh-derived mass
    // properties are filled immediately.
    void initializeEntityFromMesh(PhysicalEntity& entity, BooleanMesh baseMesh,
                                  glm::vec3 halfExtents, uint32_t materialId);

    // Splits a cut object into independent rigid bodies when its last rebuild
    // disconnected or unbalanced it. The hit entity keeps the largest piece;
    // other pieces become new Dynamic entities with deterministic network ids
    // (so a client reproducing a replicated cut derives the same pieces without
    // an extra packet). `serverDriven` marks the children as non-authoritative
    // mirrors on a client. Returns the spawned entity ids. Safe to call when no
    // fracture is warranted (returns empty).
    std::vector<uint32_t> applyFracture(PhysicalEntity& entity, ImpactResult& result,
                                        bool serverDriven);

    // ── Deterministic impact math (public for tests) ────────────────────
    static float kineticEnergy(float mass, float speed);
    static float impactAngleFactor(const glm::vec3& projectileDirection,
                                   const glm::vec3& surfaceNormal);
    static float calculateCutRadius(const ImpactEvent& impact,
                                    const MaterialDefinition& material);

private:
    ImpactSystem() = default;

    uint64_t mNextEventId = 1;
    uint32_t mIgnoredLogCounter = 0;
    uint32_t mCutLogCounter = 0;
};

// Deterministic self-test for the destructible-impact slice. Returns true when
// every check passes and (optionally) writes a per-check report.
bool destructibleSelfTest(std::string* outSummary);

} // namespace MimitaImpact
