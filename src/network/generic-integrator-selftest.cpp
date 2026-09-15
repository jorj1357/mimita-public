// 09 15 2026
/* purpose
* Implements the headless generic-integrator self-test: a TYPELESS runtime entity
// (Entity + Transform + Velocity, domain None) is integrated directly from its
// generic components through the same movement pipeline and hot policies
// (speed -> air/ground -> gravity -> jump -> dash -> freeze -> clamp). Proves the
// integrator substrate is class-agnostic (no Player/Npc/actor-kind branch).
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/generic-integrator-selftest.h"

#include <string>

#include <glm/glm.hpp>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/movement-types.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runGenericIntegratorSelfTest(std::string& report)
{
    bool ok = true;
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    // A typeless runtime actor: Entity + Transform + Velocity only.
    const EntityId e = EntityRegistry::instance().createGeneric(EntityRealm::Server);
    Ecs::setTransform(e, glm::vec3(0.0f, 0.0f, 1000.0f),
                      glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    Ecs::setVelocity(e, glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));

    MovementConfig cfg = makeCurrentRuntimeMovementConfig();
    cfg.walkMode = MovementWalkMode::Source;
    cfg.airControlEnabled = true;
    cfg.speedCapEnabled = false;
    cfg.speedLimitEnabled = false;
    cfg.sourceMaxSpeed = 20.0f;
    cfg.groundSpeed = 20.0f;
    cfg.airSpeed = 20.0f;
    cfg.airAcceleration = 12.0f;
    cfg.airMaxWishspeed = 0.0f;
    cfg.airSpeedGainMultiplier = 1.0f;
    cfg.surfaceFriction = 1.0f;
    cfg.sourceFriction = 0.0f;
    cfg.gravityZ = 0.0f;
    cfg.maximumFallSpeed = 0.0f;

    MovementCommand cmd{};
    cmd.moveAxes = glm::vec2(0.70710678f, 0.70710678f);
    cmd.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    cmd.movementDirectionPressed = true;

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 90; ++i) {
        // Read the AUTHORITATIVE generic state directly (no typed actor).
        MovementState st{};
        if (const auto* tf = EntityRegistry::instance().tryGet<TransformComponent>(e)) {
            st.position = tf->position;
            st.yaw = tf->yaw;
        }
        if (const auto* gv = EntityRegistry::instance().tryGet<VelocityComponent>(e)) {
            st.baseVelocity = gv->linear;
            st.externalImpulse = gv->externalImpulse;
        }
        st.sizeScale = 1.0f;
        st.ground.onGround = false;

        // Same movement pipeline the server player uses.
        applyPreCollisionBasicMovement(st, cmd, cfg, dt);
        MovementStepEvents preEvents;
        applySpecialMovementPreCollision(st, cmd, cfg, dt, preEvents);
        MovementCollisionFeedback collision{};
        collision.onGround = false;
        MovementStepResult result = applyPostCollisionMovementWithSpecials(
            st, cmd, cfg, collision, dt, preEvents);

        // Write the AUTHORITATIVE generic state directly.
        Ecs::setTransform(e, result.state.position, glm::vec3(1.0f, 0.0f, 0.0f),
                          result.state.yaw, 0.0f);
        Ecs::setVelocity(e, result.state.baseVelocity, result.state.externalImpulse);
    }

    const auto* gv = EntityRegistry::instance().tryGet<VelocityComponent>(e);
    ok &= check(gv != nullptr && glm::length(gv->linear) > 5.1f,
                "typeless runtime actor integrates through the same substrate",
                report);

    // Deterministic repeat: reset and run twice with identical input.
    auto runOnce = [&]() {
        Ecs::setTransform(e, glm::vec3(0.0f, 0.0f, 1000.0f),
                          glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
        Ecs::setVelocity(e, glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));
        for (int i = 0; i < 90; ++i) {
            MovementState st{};
            if (const auto* tf = EntityRegistry::instance().tryGet<TransformComponent>(e)) {
                st.position = tf->position;
                st.yaw = tf->yaw;
            }
            if (const auto* v = EntityRegistry::instance().tryGet<VelocityComponent>(e)) {
                st.baseVelocity = v->linear;
                st.externalImpulse = v->externalImpulse;
            }
            st.sizeScale = 1.0f;
            st.ground.onGround = false;
            applyPreCollisionBasicMovement(st, cmd, cfg, dt);
            MovementStepEvents preEvents;
            applySpecialMovementPreCollision(st, cmd, cfg, dt, preEvents);
            MovementCollisionFeedback collision{};
            collision.onGround = false;
            MovementStepResult result = applyPostCollisionMovementWithSpecials(
                st, cmd, cfg, collision, dt, preEvents);
            Ecs::setVelocity(e, result.state.baseVelocity, result.state.externalImpulse);
        }
        const auto* v2 = EntityRegistry::instance().tryGet<VelocityComponent>(e);
        return v2 ? v2->linear : glm::vec3(0.0f);
    };
    const glm::vec3 a = runOnce();
    const glm::vec3 b = runOnce();
    ok &= check(glm::length(a - b) < 1e-4f,
                "generic integrator is deterministic for identical input", report);

    HotReloadSystem::instance().unloadGameDLL();
    EntityRegistry::instance().destroyAll();
    return ok;
}
