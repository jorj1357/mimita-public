// 09 14 2026
/* purpose
* Generic authoritative actor identity/behavior state as dynamic components and
* relationships: team, role, movement preset, behavior profile, and target.
* Works for players, NPCs, future monsters/bots/minions alike; the typed
* Npc/ServerNpc fields are projections. Replicates through generic replication.
* Does NOT own spawn/destroy, AI policy, or the renderer.
*/
#pragma once

#include <cstdint>

namespace MimitaNet {

struct ActorTeamStateV1 {
    std::int32_t team;   // -1 = teamless
    std::uint32_t reserved;
};

struct ActorRoleStateV1 {
    std::uint64_t roleHash;   // gameHash(roleId); 0 = none
    std::uint32_t roleIndex;  // MatchRoleRegistry index (0 = none)
    std::uint32_t reserved;
};

struct ActorProfileStateV1 {
    std::uint64_t movementPresetHash;  // gameHash(movementPreset); 0 = none
    std::uint64_t behaviorProfileHash; // gameHash(behaviorProfile); 0 = none
    std::uint32_t flags;
    std::uint32_t reserved;
};

void actorStateEnsureSchemas();

// Team.
bool actorStateWriteTeam(std::uint64_t entity, std::int32_t team);
bool actorStateReadTeam(std::uint64_t entity, std::int32_t* team);

// Role (also records the hash -> role id mapping for server resolution).
bool actorStateWriteRole(std::uint64_t entity, const char* roleId);
bool actorStateReadRoleHash(std::uint64_t entity, std::uint64_t* roleHash);
const char* actorStateRoleIdForHash(std::uint64_t roleHash);

// Movement preset + behavior profile.
bool actorStateWriteProfile(std::uint64_t entity, const char* movementPreset,
                            const char* behaviorProfile);
bool actorStateReadProfile(std::uint64_t entity, std::uint64_t* movementHash,
                           std::uint64_t* behaviorHash);

// Target as an entity-to-entity relationship (relationship.targets).
bool actorStateSetTarget(std::uint64_t sourceEntity, std::uint64_t targetEntity);
bool actorStateGetTarget(std::uint64_t sourceEntity, std::uint64_t* targetEntity);
void actorStateClearTarget(std::uint64_t sourceEntity);

// Equipped tool as generic item state: actor --contains/equips-item--> tool
// entity, with a ToolRef component on the tool carrying the runtime tool key.
// Players, NPCs, and monsters use the same path.
bool actorStateEquipTool(std::uint64_t actorEntity, std::uint64_t toolEntity,
                         std::uint64_t toolKey);
bool actorStateGetEquippedTool(std::uint64_t actorEntity, std::uint64_t* toolEntity,
                               std::uint64_t* toolKey);
bool actorStateHasEquippedTool(std::uint64_t actorEntity);
// Remove the equipped-tool edge (unequip). The tool entity persists.
bool actorStateUnequipTool(std::uint64_t actorEntity);
// Standard weapon-slot equip bridge: ensure a persistent generic tool entity for
// `toolKey` and equip it on the actor (one equipped tool per actor). `realm` is
// the EntityRealm the tool entity is created in. Returns the tool EntityId.
std::uint64_t actorStateEquipWeaponKey(std::uint64_t actorEntity,
                                       std::uint64_t toolKey,
                                       std::uint32_t realm);

// Generic actor identity (display name). One cross-system source for nameplates,
// chat, killfeed, scoreboard, and spectator UI; no Player*/Npc* needed. GAME_NET_ALL
// so a joining client learns names generically.
static constexpr std::uint32_t ACTOR_IDENTITY_NAME_MAX = 32;
struct ActorIdentityStateV1 {
    char name[ACTOR_IDENTITY_NAME_MAX];
    std::uint32_t reserved;
};
bool actorStateWriteIdentity(std::uint64_t entity, const char* name);
bool actorStateReadIdentity(std::uint64_t entity, char* outName,
                            std::uint32_t outSize);

// Generic action-handling gate: the hot action router records on the actor that
// it handled a runtime action at a tick. The cold legacy fallback consults this
// (no weapon/type category is known to the kernel).
bool actorStateActionHandled(std::uint64_t actorEntity, std::uint64_t tick);

// ── Generic actor lifecycle/origin state (NPC migration Phase 1-3) ──────
// One generic source for actor lifecycle metadata so `ServerNpc` stops owning
// it. Origin/R… classify the life without an NPC-specific field, and the
// lifecycle component carries the identity generation + death/respawn facts.
// GAME_NET_ALL so a joining client learns them generically. The hot
// `npc.lifecycle` policy is the only decider; these components are its state.
struct ActorOriginStateV1 {
    std::uint32_t origin;         // GameNpcOriginV1 (0 startup,1 manual,2 mode,3 respawn)
    std::uint32_t startingWeaponHash;  // gameHash(weaponId); 0 = cold default
    std::uint64_t startingWeaponNameHash;  // reserved for generic identity
};
bool actorStateWriteOrigin(std::uint64_t entity, std::uint32_t origin,
                           std::uint64_t startingWeaponHash);
bool actorStateReadOrigin(std::uint64_t entity, std::uint32_t* origin,
                          std::uint64_t* startingWeaponHash);

struct ActorLifecycleComponentV1 {
    std::uint32_t lifeGeneration;  // bumped per respawn; never 0
    std::uint32_t dead;
    float respawnTimer;
    std::uint32_t deathReason;     // GameActorDestroyReasonV1 at death
    std::uint32_t reserved;
};
bool actorStateWriteLifecycle(std::uint64_t entity, std::uint32_t lifeGeneration,
                              std::uint32_t dead, float respawnTimer);
bool actorStateReadLifecycle(std::uint64_t entity, std::uint32_t* lifeGeneration,
                             std::uint32_t* dead, float* respawnTimer);

// Generic avatar identity: a hashed resource/avatar identity so avatar names are
// not limited by the legacy 15-byte NPC packet field. The client resolves the
// hash to a name/resource; the server only stores the identity. GAME_NET_ALL.
struct ActorAvatarStateV1 {
    std::uint64_t avatarHash;      // gameHash(avatarName); 0 = none
    std::uint32_t generation;      // avatar generation (per life)
    std::uint32_t reserved;
};
bool actorStateWriteAvatar(std::uint64_t entity, std::uint64_t avatarHash,
                           std::uint32_t generation);
bool actorStateReadAvatar(std::uint64_t entity, std::uint64_t* avatarHash,
                          std::uint32_t* generation);

// Generic actor network state (NPC migration Phase 4): the wire-facing
// projection of the authoritative Transform/Velocity plus the presentation
// bits the compact snapshot used to carry. GAME_NET_ALL. Replicating an actor's
// position/velocity/aim/pose is replication because it possesses this component,
// not because packet code contains an NPC branch.
struct ActorNetStateV1 {
    float position[3];
    float velocity[3];
    float aim[3];
    float yaw;
    std::uint32_t onGround;
    std::int16_t equippedSlot;
    std::uint8_t weaponState;   // NET_WEAPON_STATE_* bits
    std::uint8_t reserved8;
    std::uint32_t reserved;
};
bool actorStateWriteNetState(std::uint64_t entity, const ActorNetStateV1& state);
bool actorStateReadNetState(std::uint64_t entity, ActorNetStateV1* out);

// Generic weapon presentation state for an actor's equipped tool: the compact
// snapshot's equippedSlot + weaponState bits as a generic component, so the hot
// `actor.net-state` projection does not read typed Npc fields. Network policy is
// NONE (a hot input only); `ActorNetState` remains the wire carrier.
struct ActorWeaponStateV1 {
    std::int16_t equippedSlot;
    std::uint8_t weaponState;   // NET_WEAPON_STATE_* bits
    std::uint8_t reserved8;
    std::uint32_t reserved;
};
bool actorStateWriteWeaponState(std::uint64_t entity, std::int16_t equippedSlot,
                                std::uint8_t weaponState);
bool actorStateReadWeaponState(std::uint64_t entity, std::int16_t* equippedSlot,
                               std::uint8_t* weaponState);

} // namespace MimitaNet
