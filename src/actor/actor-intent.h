// 10 02 2026
/* purpose
* Defines the shared ActorIntent boundary between decision sources and the
* shared actor execution path (movement/weapons/interaction).
* Human input, the NPC brain, scripted actors, and replay input all produce an
* ActorIntent; a single adapter converts it to the existing InputState so the
* fixed-60-Hz physics kernel stays the one execution owner.
* Does NOT simulate physics or own any weapon/AI policy.
* Does NOT bypass InputState or physicsMainUpdate.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct InputState;

// A source-agnostic statement of what an actor wants to do this tick. It is a
// boundary value, not policy: whoever fills it (human/NPC/script/replay) owns
// the decision, and the shared adapter owns the translation to execution.
struct ActorIntent {
    glm::vec2 move{0.0f};             // planar wish direction (unit or zero)
    glm::vec3 lookDirection{1.0f, 0.0f, 0.0f};
    glm::vec3 aimPoint{0.0f};         // world point the actor wants to aim at

    bool jump = false;
    bool dash = false;
    bool crouch = false;
    bool attack = false;
    bool reload = false;
    bool interact = false;

    std::string weaponId;             // empty = keep current weapon
    uint32_t targetActorId = 0;       // 0 = no specific target
};

namespace ActorIntentAdapter {

// Convert an ActorIntent into the shared InputState the physics kernel consumes.
// This is the single translation owner; callers must not duplicate it.
// `outFacing` receives the actor's look direction after the adapter resolves
// aimPoint/lookDirection so callers can update their facing model.
InputState toInputState(const ActorIntent& intent,
                        bool movementPressed,
                        const glm::vec3& fallbackFacing);

} // namespace ActorIntentAdapter
