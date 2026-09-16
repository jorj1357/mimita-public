// 09 15 2026
/* purpose
* Implements the headless real-path air-movement parity self-test. It drives:
*   A) the real server movement adapter (applyPreCollisionBasicMovement ->
*      applySourceAir -> hot movement.air-accelerate), and
*   B) the real local prediction system (movement.main via the gameplay domain),
* with identical initial state, input, dt, and aligned air tuning, over a
* multi-tick airborne sequence. Any divergence is reported with the first
* divergent tick and diagnostics (no tolerance hiding).
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/air-movement-parity-selftest.h"

#include <cmath>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/movement-types.h"
#include "world/world.h"

using namespace MimitaRuntime;

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kTicks = 120;
constexpr float kWish = 0.70710678f;

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

MovementConfig makeAirConfig()
{
    MovementConfig cfg = makeCurrentRuntimeMovementConfig();
    // Align the server air tuning with the local `source` preset so the shared
    // algorithm receives the SAME numeric inputs (walkSpeed=20, airAccel=12).
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

    return cfg;
}

} // namespace

bool runAirMovementParitySelfTest(std::string& report)
{
    bool ok = true;

    World world;  // empty: airborne, no collision during the interval
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    // ── Local prediction setup (real `movement.main`) ─────────────────
    const EntityId entity = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 1);
    Ecs::setBody(entity, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entity, kWish, kWish, true, false, false, false, false);
    if (GameSharedStateV1* shared = GenericRuntime::instance().sharedState()) {
        shared->magic = GAME_SHARED_MAGIC;
        shared->modeFlags = GAME_MODE_FLAG_HOT_MOVEMENT;
        shared->localPlayerEntity = static_cast<std::uint64_t>(entity);
    }
    LiveBehavior::setDispatchWorld(&world);

    // ── Identical initial state for both paths ────────────────────────
    const glm::vec2 v0(5.0f, 0.0f);
    Ecs::setTransform(entity, glm::vec3(0.0f, 0.0f, 1000.0f),
                      glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    Ecs::setVelocity(entity, glm::vec3(v0.x, v0.y, 0.0f), glm::vec3(0.0f));

    const MovementConfig cfg = makeAirConfig();
    MovementState serverState{};
    serverState.position = glm::vec3(0.0f, 0.0f, 1000.0f);
    serverState.baseVelocity = glm::vec3(v0.x, v0.y, 0.0f);
    serverState.sizeScale = 1.0f;
    serverState.ground.onGround = false;
    MovementCommand cmd{};
    cmd.moveAxes = glm::vec2(kWish, kWish);
    cmd.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    cmd.movementDirectionPressed = true;
    cmd.clientSimulationTick = 0;

    GenericRuntime& runtime = GenericRuntime::instance();
    glm::vec2 clientVel = v0;
    glm::vec2 serverVel = v0;

    int firstDivergence = -1;         // first tick exceeding the tight epsilon
    float divSx = 0.0f, divSy = 0.0f, divCx = 0.0f, divCy = 0.0f;
    float maxDev = 0.0f;              // worst per-tick horizontal disagreement
    float earlyMaxDev = 0.0f;         // worst over the first 30 ticks
    constexpr float kTight = 1e-3f;

    for (int i = 0; i < kTicks; ++i) {
        // A) Server adapter: the real three-phase server movement sequence
        // (pre -> (no world) -> post), where the walk/air step calls the shared
        // hot air hook.
        cmd.clientSimulationTick = static_cast<std::uint64_t>(i);
        applyPreCollisionBasicMovement(serverState, cmd, cfg, kDt);
        MovementStepEvents preEvents;
        applySpecialMovementPreCollision(serverState, cmd, cfg, kDt, preEvents);
        MovementCollisionFeedback collision{};
        collision.onGround = false;
        collision.hasWorldContact = false;
        collision.simulationTick = static_cast<std::uint64_t>(i);
        applyPostCollisionMovementWithSpecials(serverState, cmd, cfg, collision, kDt,
                                               preEvents);
        serverVel = glm::vec2(serverState.baseVelocity.x, serverState.baseVelocity.y);

        // B) Local prediction (real movement.main).
        runtime.beginMovementTick();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, static_cast<std::uint64_t>(i), kDt,
                          LiveBehavior::hostContext(static_cast<std::uint64_t>(i)));
        float oPos[3] = {0.0f, 0.0f, 0.0f};
        float oVel[3] = {0.0f, 0.0f, 0.0f};
        float oYaw = 0.0f;
        if (runtime.consumeMovementOverride(oPos, oVel, oYaw))
            clientVel = glm::vec2(oVel[0], oVel[1]);
        else if (const auto* v = EntityRegistry::instance().tryGet<VelocityComponent>(entity))
            clientVel = glm::vec2(v->linear.x, v->linear.y);

        const float dx = std::fabs(serverVel.x - clientVel.x);
        const float dy = std::fabs(serverVel.y - clientVel.y);
        const float dev = dx > dy ? dx : dy;
        if (dev > maxDev)
            maxDev = dev;
        if (i < 30 && dev > earlyMaxDev)
            earlyMaxDev = dev;
        if (firstDivergence < 0 && dev > kTight) {
            firstDivergence = i;
            divSx = serverVel.x;
            divSy = serverVel.y;
            divCx = clientVel.x;
            divCy = clientVel.y;
        }
    }

    // Tight agreement over the first 30 ticks proves both real paths feed the
    // shared air function identical inputs.
    ok &= check(earlyMaxDev < 1e-3f,
                "server + prediction use the shared air function identically "
                "(early agreement)", report);

    // Full-sequence parity: the real server adapter and the real local
    // prediction path must agree for the whole airborne run.
    {
        char buf[320];
        std::snprintf(buf, sizeof(buf),
                      "full(%d)-tick air parity: maxDev=%.6f first>1e-3 tick=%d "
                      "serverVel=(%.4f,%.4f) clientVel=(%.4f,%.4f)",
                      kTicks, maxDev, firstDivergence, divSx, divSy, divCx, divCy);
        const bool parity = maxDev < kTight;
        report += std::string(parity ? "[ok] " : "[FAIL] ") + buf + "\n";
        ok &= parity;
    }

    // The shared air function must actually have moved both (not a no-op).
    ok &= check(glm::length(serverVel - v0) > 1e-3f,
                "server air path integrated velocity", report);
    ok &= check(glm::length(clientVel - v0) > 1e-3f,
                "prediction air path integrated velocity", report);

    HotReloadSystem::instance().unloadGameDLL();
    EntityRegistry::instance().destroyAll();
    return ok;
}
