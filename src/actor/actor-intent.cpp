// 10 02 2026
// Shared ActorIntent -> InputState adapter. One translation owner so human,
// NPC, scripted, and replay sources all execute through the same path.
#include "actor/actor-intent.h"

#include "input/input-state.h"

#include <cmath>

namespace ActorIntentAdapter {

InputState toInputState(const ActorIntent& intent,
                        bool movementPressed,
                        const glm::vec3& fallbackFacing)
{
    InputState input;
    input.wishMoveXY = intent.move;
    input.movementPressed = movementPressed || glm::length(intent.move) > 0.001f;
    input.jumpHeld = intent.jump;
    input.jumpPressed = intent.jump;  // callers edge-detect if they need a pulse
    input.dashPressed = intent.dash;
    input.groundReturnPressed = false;
    input.downDashPressed = false;
    input.freezeHeld = false;

    // Facing: the adapter carries the look direction through camForward. The
    // caller remains the owner of turn-rate smoothing and body yaw (the NPC
    // brain's buildInputState). aimPoint is intentionally not consumed here so
    // there is exactly one facing owner.
    const glm::vec3 fwd = glm::length(intent.lookDirection) > 0.0001f
        ? glm::normalize(intent.lookDirection) : fallbackFacing;
    input.camForward = fwd;
    return input;
}

} // namespace ActorIntentAdapter
