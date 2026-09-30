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

#include <glm/glm.hpp>

#include "impact/boolean-mesh.h"
#include "impact/impact-event.h"

struct PhysicalEntity;

namespace MimitaImpact {

struct ImpactResult
{
    bool applied = false;
    bool cutCreated = false;
    float cutRadius = 0.0f;
    float damage = 0.0f;
    int chunksRebuilt = 0;
    uint32_t triangleCount = 0;
    uint32_t componentCount = 0;
    float remainingVolume = 0.0f;
    BooleanError error = BooleanError::None;
};

class ImpactSystem
{
public:
    static ImpactSystem& instance();

    // Applies one impact. Returns what happened; safe to call every hit.
    ImpactResult submit(const ImpactEvent& event);

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
