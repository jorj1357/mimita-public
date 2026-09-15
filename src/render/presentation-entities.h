// 09 14 2026
/* purpose
* Client-side generic presentation-entity bridge for projectiles. Materializes
* and updates a generic client entity (Transform + Velocity +
* PresentationState) for each live network/predicted projectile so the hot
* `hot.presentation-mesh` system draws it through render.mesh. Identity and
* lifetime are tied to the projectile id; the hot system owns the draw. This is
* the migration off typed projectile rendering.
* Does NOT own prediction/interpolation math or networking; it mirrors the
* already-interpolated render state into generic presentation state.
*/
#pragma once

#include <cstdint>

class Player;

namespace PresentationEntities {

// Data mapping from a legacy network weapon id to logical resources. Returns
// false when the weapon is not migrated (typed renderer remains authoritative).
bool resourcesForWeapon(std::uint32_t networkWeaponId, std::uint64_t& meshId,
                        std::uint64_t& textureId, float& scale);

// Materialize or update the generic presentation entity for a projectile.
// Returns the entity id (0 when unmigrated).
std::uint64_t ensure(std::uint32_t projectileId, const float position[3],
                     const float velocity[3], std::uint64_t meshId,
                     std::uint64_t textureId, float scale);

// True when a generic presentation entity exists for this projectile, so the
// typed renderer must not also draw it.
bool has(std::uint32_t projectileId);

// Local prediction: materialize ONE provisional generic entity for a prediction
// key (Transform/Velocity/PresentationState + a generic PredictionLink) and
// register it with the generic PredictionRegistry. When the authoritative
// entity for the key appears, the registry retires this provisional.
std::uint64_t ensurePredicted(std::uint64_t predictionKey,
                              std::uint32_t projectileId,
                              const float position[3], const float velocity[3],
                              std::uint64_t meshId, std::uint64_t textureId,
                              float scale, std::uint64_t tick);

// Associate any client entity carrying a generic PredictionLink with its
// prediction key (authoritative entity -> canonical, provisional retires).
// Safe with authority-first, duplicates, and stale keys.
void associateByLink(std::uint64_t tick);

// ── Client actor presentation bridge ──────────────────────────────────
// Project a real client NPC replica into a generic presentation entity
// (Transform + PresentationState). The typed NPC renderer yields when the
// generic path owns presentation (see actorMeshReady()). Authoritative/network
// state stays the existing NPC snapshot/interpolation; Transform here is a
// projection, not a second simulation.
std::uint64_t ensureActor(std::uint32_t actorId, const float position[3],
                          const float look[3], std::uint64_t meshResourceId,
                          std::uint64_t textureResourceId, float scale,
                          const float color[4]);
void beginActorSync();
void endActorSync();

// ── Local player bridge (THE_PLAYER) ─────────────────────────────────
// Projects the local player onto its canonical generic entity and applies the
// hot-generated skeleton pose back onto its body parts. This is the same
// EntityId-keyed architecture as remote actors; the typed Player is only the
// visible body owner, not the animation/pose policy owner.
void projectLocalPlayer(::Player& player);
void applyHotPoseToPlayer(::Player& player);

// ── Generic actor overlay state ──────────────────────────────────────
// Canonical actor EntityId for a replicated actor id (idempotent ensure).
std::uint64_t actorEntityFor(std::uint32_t actorId, bool isPlayer);
// Project Transform + Health + generic ActorIdentityState for a replicated actor
// so the hot overlay path can own its nameplate/health without a second actor
// identity. Body draw ownership is unchanged (no PresentationState written here).
void projectActorOverlayState(std::uint32_t actorId, bool isPlayer,
                              const ::Player& player);
// True when the actor mesh resource is loaded and can be drawn, so the typed
// renderer must yield ownership.
bool actorMeshReady();

// Frame-scoped mark/sweep: entities not touched between begin/end are retired,
// so a destroyed/exploded/stale projectile's visual never lingers.
void beginSync();
void endSync();

// Drop every tracked presentation entity (session teardown).
void clear();

// Client projection for REPLICATED authoritative projectiles. Enumerates
// entities carrying the generic HotProjectileState dynamic component and writes
// the typed Transform/Velocity the hot presentation system reads, so the real
// replicated EntityId presents itself (join-in-progress included). Never creates
// an entity; skips entities the client has not adopted.
void projectReplicatedProjectiles();

} // namespace PresentationEntities
