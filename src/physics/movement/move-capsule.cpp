// 09 14 2026
/* purpose
* Implements the kernel capsule-vs-world movement primitive.
* Does NOT own player state, input, or gameplay policy.
*/
#include "physics/movement/move-capsule.h"

#include <glm/gtc/quaternion.hpp>

#include "physics/physical-body.h"
#include "world/world.h"

namespace Physics {

void moveCapsuleStep(MovementStateV1& state, const World* world, float dt)
{
    if (dt <= 0.0f)
        return;

    RigidBody body;
    body.position = glm::vec3(state.position[0], state.position[1], state.position[2]);
    body.orientation = glm::angleAxis(glm::radians(state.yaw), glm::vec3(0.0f, 0.0f, 1.0f));
    body.linearVelocity = glm::vec3(state.velocity[0], state.velocity[1], state.velocity[2]);
    body.capsuleRadius = state.radius > 0.0f ? state.radius : 0.4f;
    body.capsuleHalfHeight = state.halfHeight > 0.0f ? state.halfHeight : 0.5f;
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    setBodyMass(body, 1.0f);

    const glm::vec3 gravity(0.0f, 0.0f, -9.81f);
    integrate(body, gravity, dt);

    bool collided = false;
    if (world) {
        collided = collideWithWorld(body, *world, dt);
        depenetrateWorld(body, *world, 2);
    }

    state.position[0] = body.position.x;
    state.position[1] = body.position.y;
    state.position[2] = body.position.z;
    state.velocity[0] = body.linearVelocity.x;
    state.velocity[1] = body.linearVelocity.y;
    state.velocity[2] = body.linearVelocity.z;
    state.collided = collided ? 1u : 0u;
    // Grounded heuristic: a world contact with near-zero vertical velocity.
    if (collided && body.linearVelocity.z > -0.05f && body.linearVelocity.z < 0.05f)
        state.grounded = 1u;
    else if (!collided)
        state.grounded = 0u;
}

} // namespace Physics
