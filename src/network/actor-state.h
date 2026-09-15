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

} // namespace MimitaNet
