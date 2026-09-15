// 09 14 2026
/* purpose
* Generic authoritative actor/entity health as a dynamic component. Any
* damageable entity (NPC, monster-like runtime entity, future destructive
* object) stores health here; the typed HealthComponent and ServerNpc.health are
* projections. Health replicates through the generic dynamic-component path.
* Does NOT own spawn/destroy policy, networking, or the renderer.
*/
#pragma once

#include <cstdint>

namespace MimitaNet {

struct ActorHealthStateV1 {
    std::int32_t current;
    std::int32_t max;
    std::uint32_t dead;
    std::uint32_t reserved;
};

// Registers the replicated health schema (networkPolicy ALL). Idempotent.
void actorHealthEnsureSchema();

bool actorHealthHas(std::uint64_t entity);
bool actorHealthInit(std::uint64_t entity, std::int32_t maxHp);
bool actorHealthRead(std::uint64_t entity, std::int32_t* current, std::int32_t* maxHp,
                     bool* dead);
// Applies damage to the authoritative component; returns false when the entity
// has no health component. `outAfter`/`outDead` receive the new state.
bool actorHealthApplyDamage(std::uint64_t entity, std::int32_t amount,
                            std::int32_t* outAfter, bool* outDead);
bool actorHealthSet(std::uint64_t entity, std::int32_t current);

} // namespace MimitaNet
