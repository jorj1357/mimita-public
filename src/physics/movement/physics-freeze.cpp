// 07 21 2026, 16 30
/* purpose
* Adapts the legacy Player freeze entry point to the shared movement freeze state machine.
* Preserves the compatibility function while using the target fresh-press and pow4 timer rules.
* Keeps freeze availability, active state, timer, held edge, and start event synchronized.
* Does NOT own a second freeze curve, collision velocity view, audio, VFX, or packets.
* Does NOT destructively scale stored horizontal impulse or suppress vertical movement.
* Does NOT implement contact-reset generation or prediction history.
*/

#include "physics/movement/physics-freeze.h"

#include "entities/player.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"

void doFreeze(
    Player& p,
    bool freezeHeld,
    float dt
)
{
    MovementCommand command;
    command.lifecycle = MovementLifecycleIdentity{p.spawnGeneration, 0};
    command.freezeHeld = freezeHeld;
    command.freezePressed = freezeHeld && !p.freeze.freezeHeldPrev;

    MovementState state = movementStateFromPlayer(p, command.lifecycle);
    MovementStepEvents events;
    const MovementConfig config = makeCurrentRuntimeMovementConfig();
    updateFreeze(state, command, config, dt, events);
    applyMovementStateToPlayer(state, p);
}
