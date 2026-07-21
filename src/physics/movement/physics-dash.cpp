// 07 21 2026, 16 30
/* purpose
* Adapts legacy Player dash entry points to the shared movement dash helper.
* Preserves compatibility call signatures while using the target additive 50 XY dash formula.
* Keeps dash state, events, and momentum protection synchronized through MovementState conversion.
* Does NOT own a second dash formula, dash VFX, packet state, or input polling.
* Does NOT decide client/server authority, collision results, or weapon behavior.
* Does NOT keep the obsolete 100-force external-impulse dash as an active implementation.
*/

#include "physics/movement/physics-dash.h"

#include "debug/debug-log.h"
#include "entities/player.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"

#define DASH_LOG(...) Debug::logThrottled(Debug::Category::Physics, "dash", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

static MovementCommand dashCommandForPlayer(Player& p,
                                            const glm::vec2& wishMoveXY,
                                            bool dashPressed,
                                            bool movementPressed,
                                            float movementHeldDuration,
                                            const glm::vec3& camForward)
{
    MovementCommand command;
    command.lifecycle = MovementLifecycleIdentity{p.spawnGeneration, 0};
    command.moveAxes = movementClampUnitOrZero(wishMoveXY);
    command.horizontalCameraForward = camForward;
    command.lookYaw = p.yaw;
    command.dashPressed = dashPressed;
    command.movementDirectionPressed =
        movementPressed || movementHasMoveInput(command.moveAxes);
    command.movementHeldDurationSeconds = movementHeldDuration;
    return command;
}

static void applyDashAdapter(Player& p, const MovementCommand& command)
{
    MovementState state = movementStateFromPlayer(p, command.lifecycle);
    MovementStepEvents events;
    const MovementConfig config = makeCurrentRuntimeMovementConfig();
    tryActivateDash(state, command, config, events);
    applyMovementStateToPlayer(state, p);

    if (events.didDash) {
        DASH_LOG("[DASH] shared dir=(%.2f %.2f) vel=(%.2f %.2f)\n",
                 movementDashDirection(command).x,
                 movementDashDirection(command).y,
                 p.vel.x,
                 p.vel.y);
    }
}

void doAirDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpTriggered,
    bool movementPressed,
    bool airborne,
    int movementTicks,
    float movementHeldDuration,
    float dt,
    const glm::vec3& camForward
) {
    (void)movementTicks;
    (void)dt;
    if (!jumpTriggered || !airborne)
        return;

    applyDashAdapter(
        p,
        dashCommandForPlayer(p,
                             wishMoveXY,
                             true,
                             movementPressed,
                             movementHeldDuration,
                             camForward));
}

void doDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool dashPressed,
    const glm::vec3& camForward,
    float dt
) {
    (void)dt;
    if (!dashPressed)
        return;

    applyDashAdapter(
        p,
        dashCommandForPlayer(p,
                             wishMoveXY,
                             true,
                             movementHasMoveInput(wishMoveXY),
                             0.0f,
                             camForward));
}
