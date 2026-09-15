// 09 15 2026
/* purpose
* movement.jump: the ONE hot jump policy (buffer, coyote, ground/air jump,
* air-jump availability, impulse/vertical velocity, state transitions). Called
* by the cold/server movement hook (via the generic event) and by local
* prediction (`movement.main`). Context-free: plain numbers only.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

namespace MimitaHotMovement {

void jumpPolicy(GameJumpPolicyV1& io)
{
    // Timers.
    io.jumpIntentTimerSeconds =
        io.jumpIntentTimerSeconds > io.dt ? io.jumpIntentTimerSeconds - io.dt : 0.0f;
    io.coyoteTimerSeconds =
        io.coyoteTimerSeconds > io.dt ? io.coyoteTimerSeconds - io.dt : 0.0f;
    if (io.grounded)
        io.coyoteTimerSeconds = io.coyoteSeconds;

    if (io.jumpHeld && io.autoBhopEnabled)
        io.jumpIntentTimerSeconds = io.jumpBufferSeconds;

    const bool jumpPressedThisFrame =
        io.jumpPressed != 0u || (io.jumpHeld != 0u && !io.jumpHeldPreviously);
    if (jumpPressedThisFrame)
        io.jumpIntentTimerSeconds = io.jumpBufferSeconds;

    const bool jumpReleased = io.jumpHeld == 0u && io.jumpHeldPreviously != 0u;
    if (jumpReleased) {
        io.airJumpArmed = 1u;
        io.airJumpLocked = 0u;
        io.jumpIntentTimerSeconds = 0.0f;
    }

    io.outJumpHeldPreviously = io.jumpHeld;
    io.outVelocityZ = io.velocityZ;
    io.outGrounded = io.grounded;
    io.outDidGroundJump = 0u;
    io.outDidAirJump = 0u;

    if (io.jumpIntentTimerSeconds <= 0.0f)
        return;

    if (io.grounded != 0u || io.coyoteTimerSeconds > 0.0f) {
        io.outVelocityZ = io.jumpSpeed;
        io.outGrounded = 0u;
        io.coyoteTimerSeconds = 0.0f;
        io.jumpIntentTimerSeconds = 0.0f;
        io.airJumpsLeft = static_cast<std::int32_t>(io.maximumAirJumps);
        io.airJumpLocked = 1u;
        io.airJumpArmed = 0u;
        io.outDidGroundJump = 1u;
        return;
    }

    if (io.airJumpsLeft > 0 && io.airJumpArmed != 0u) {
        io.outVelocityZ = io.jumpSpeed;
        --io.airJumpsLeft;
        io.airJumpArmed = 0u;
        io.airJumpLocked = 1u;
        io.jumpIntentTimerSeconds = 0.0f;
        io.outDidAirJump = 1u;
    }
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onJump(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameJumpPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::jumpPolicy(*p);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_jumpRegistration{
    {GAME_EVENT_MOVEMENT_JUMP, 0, 0, onJump, "movement.jump"}};

#endif
