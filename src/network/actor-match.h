// 09 10 2026
/* purpose
* Define the shared match-identity vocabulary for authoritative participants.
* Every human or NPC participant gets one ActorMatchDescriptor owned by the
* active gamemode state; systems reference profile IDs instead of copying configs.
* Human players and NPCs use the same descriptor and the same assignment path.
* Does NOT simulate actors, load configs, or define gameplay behavior.
* Does NOT own teams/scoring (server-gamemode) or the wire packet (packets.h).
*/
#pragma once

#include <cstdint>
#include <string>

namespace MimitaNet {

enum class ActorController : uint8_t
{
    Human = 0,
    Npc = 1
};

enum class ActorState : uint8_t
{
    Alive = 0,
    Dead = 1,
    Respawning = 2,
    Spectating = 3
};

// The single server-owned identity for one match participant. Profile fields
// are IDs that reference config registries; do not duplicate the configs here.
struct ActorMatchDescriptor
{
    ActorController controller = ActorController::Human;
    ActorState state = ActorState::Alive;
    int teamId = -1;  // -1 = no team (FFA / unassigned)

    std::string roleId;
    std::string movementProfileId;
    std::string weaponProfileId;
    std::string behaviorProfileId;  // NPC only; empty for humans
};

// Pure actor lifecycle transition rule (single source of truth).
//   !dead                      -> Alive
//   dead + respawns enabled    -> Dead -> Respawning -> Alive (via repeated calls)
//   dead + no respawns         -> Dead -> Spectating (terminal)
// Kept free of engine state so it is unit-testable without the server.
inline ActorState nextActorState(ActorState current, bool dead, bool respawnsEnabled)
{
    if (!dead)
        return ActorState::Alive;
    if (!respawnsEnabled)
        return (current == ActorState::Alive) ? ActorState::Dead
                                              : ActorState::Spectating;
    if (current == ActorState::Alive)
        return ActorState::Dead;
    if (current == ActorState::Dead || current == ActorState::Spectating)
        return ActorState::Respawning;
    return current;  // Respawning stays until the actor is revived.
}

// Pure kill-heal policy: heal the killer unless the active mode disabled it.
inline bool matchKillHeals(bool gamemodeEnabled, bool killHeals)
{
    return !gamemodeEnabled || killHeals;
}

} // namespace MimitaNet
