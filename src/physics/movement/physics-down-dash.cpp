// 07 21 2026, 16 30
/* purpose
* Adapts the legacy Player down-dash entry point to the shared movement helper.
* Preserves the Q-triggered compatibility function while using target base Z assignment.
* Keeps down-dash availability and one-tick event state synchronized through MovementState.
* Does NOT own a second down-dash formula, collision handling, VFX, audio, or packets.
* Does NOT block grounded down dash, freeze vertical movement, or slope collision behavior.
* Does NOT implement Ground Return or contact-reset generation.
*/

#include "physics/movement/physics-down-dash.h"

#include "entities/player.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"

void doDownDash(
    Player& p,
    bool downDashPressed,
    float dt
) {
    (void)dt;
    if (!downDashPressed)
        return;

    MovementCommand command;
    command.lifecycle = MovementLifecycleIdentity{p.spawnGeneration, 0};
    command.downDashPressed = true;

    MovementState state = movementStateFromPlayer(p, command.lifecycle);
    MovementStepEvents events;
    const MovementConfig config = makeCurrentRuntimeMovementConfig();
    tryActivateDownDash(state, command, config, events);
    applyMovementStateToPlayer(state, p);
}
