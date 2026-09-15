// 09 14 2026
/* purpose
* Generic objective event facts shared by the kernel, hot objective packages, and
* tests. An "objective" is any entity + components + relationships; the kernel
* never knows what it means (bomb, point, payload). These payloads carry no mode
* or objective-type enum and are dispatched by runtime event hash.
* Does NOT own objective policy, rendering, or the network transport.
*/
#pragma once

#include <cstdint>

namespace MimitaNet {

// Interaction fact: one actor interacted with one objective (and optional site).
// The meaning of the action and the resulting state live in hot objective code.
struct GameObjectiveInteractV1 {
    std::uint64_t actorEntity;
    std::uint64_t objectiveEntity;
    std::uint64_t siteEntity;    // 0 = none
    std::uint64_t actionHash;    // gameHash("interact.primary") etc.
    std::uint32_t tick;
    std::uint32_t inputFlags;    // 1 = pressed, 2 = held, 4 = released
    std::uint32_t handled;       // hot handler sets 1 when it owns the outcome
    std::uint32_t reserved;
};

// State-change fact emitted by a hot objective system after it mutates an
// objective's components/relationships. Observation only: HUD/replay/mode logic
// may consume it, but the components remain the source of truth.
struct GameObjectiveStateV1 {
    std::uint64_t objectiveEntity;
    std::uint64_t actorEntity;   // carrier/owner/interactor, 0 = none
    std::uint64_t siteEntity;    // 0 = none
    std::uint32_t state;         // package-private meaning (0 = idle)
    std::uint32_t tick;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// Node/relationship type used for the carrier edge. It is a plain runtime
// relationship type, not an enum or an ABI field.
static constexpr const char* GAME_OBJECTIVE_CARRIED_BY = "objective.carried-by";

} // namespace MimitaNet
