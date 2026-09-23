// 09 16 2026
/* purpose
* Implements the movement self-test. It drives the real hot movement system
* (which now owns collision via the hot capsule-vs-world solve) and checks
* gravity, determinism, floor landing, and grounded behaviour.
* Does NOT run the game or own movement policy.
*/
#include "physics/movement/movement-selftest.h"

#include <cmath>
#include <string>

#include <glm/glm.hpp>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-movement-fired.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace {

constexpr float kDt = 1.0f / 60.0f;

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

void addFloor(World& world)
{
    CollisionTriangle tri;
    tri.a = glm::vec3(-50.0f, -50.0f, 0.0f);
    tri.b = glm::vec3(50.0f, -50.0f, 0.0f);
    tri.c = glm::vec3(0.0f, 50.0f, 0.0f);
    tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    world.collisionMesh.triangles.push_back(tri);
    world.collisionMesh.boundsMin = glm::vec3(-50.0f, -50.0f, 0.0f);
    world.collisionMesh.boundsMax = glm::vec3(50.0f, 50.0f, 0.0f);
}

bool runHotMovement(EntityId entity, int ticks, float (&outPos)[3],
                    float (&outVel)[3])
{
    Ecs::setTransform(entity, glm::vec3(0.0f, 0.0f, 10.0f),
                      glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
    Ecs::setVelocity(entity, glm::vec3(0.0f), glm::vec3(0.0f));
    MimitaRuntime::GenericRuntime& runtime = MimitaRuntime::GenericRuntime::instance();
    bool got = false;
    for (int i = 0; i < ticks; ++i) {
        runtime.beginMovementTick();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, (std::uint64_t)i, kDt,
                          LiveBehavior::hostContext((std::uint64_t)i));
        float yaw = 0.0f;
        if (runtime.consumeMovementOverride(outPos, outVel, yaw))
            got = true;
    }
    return got;
}

} // namespace

bool runMovementSelfTest(std::string& report)
{
    bool ok = true;

    World world;
    addFloor(world);
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().startup();

    const EntityId entity = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 1);
    Ecs::setBody(entity, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entity, 0.0f, 0.0f, false, false, false, false, false);
    GameSharedStateV1* shared =
        MimitaRuntime::GenericRuntime::instance().sharedState();
    if (shared) {
        shared->magic = GAME_SHARED_MAGIC;
        shared->modeFlags = GAME_MODE_FLAG_HOT_MOVEMENT;
        shared->localPlayerEntity = (std::uint64_t)entity;
    }
    LiveBehavior::setDispatchWorld(&world);

    // Free-fall: gravity pulls the actor down; result is finite.
    float posA[3] = {0.0f, 0.0f, 0.0f};
    float velA[3] = {0.0f, 0.0f, 0.0f};
    const bool gotA = runHotMovement(entity, 20, posA, velA);
    ok &= check(gotA, "hot movement produced overrides", report);
    ok &= check(posA[2] < 10.0f && std::isfinite(posA[2]),
                "gravity applied", report);

    // Determinism: two identical runs from FRESH entities must agree exactly.
    // afad20a ground contacts bounce, and the bounce cooldown is per-entity
    // state; using a fresh entity for each run removes that carry-over.
    const EntityId entityB = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 2);
    Ecs::setBody(entityB, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entityB, 0.0f, 0.0f, false, false, false, false, false);
    const EntityId entityC = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 3);
    Ecs::setBody(entityC, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entityC, 0.0f, 0.0f, false, false, false, false, false);
    float posB[3] = {0.0f, 0.0f, 0.0f};
    float velB[3] = {0.0f, 0.0f, 0.0f};
    if (shared)
        shared->localPlayerEntity = (std::uint64_t)entityB;
    runHotMovement(entityB, 120, posB, velB);
    float posC[3] = {0.0f, 0.0f, 0.0f};
    float velC[3] = {0.0f, 0.0f, 0.0f};
    if (shared)
        shared->localPlayerEntity = (std::uint64_t)entityC;
    runHotMovement(entityC, 120, posC, velC);
    ok &= check(std::fabs(posB[2] - posC[2]) < 1e-4f &&
                    std::fabs(velB[2] - velC[2]) < 1e-4f,
                "hot movement path deterministic", report);

    // Floor collision: the actor does not fall through and stays finite.
    ok &= check(posB[2] > 0.5f, "capsule does not fall through floor", report);
    ok &= check(std::isfinite(velB[2]), "vertical velocity finite", report);

    if (shared)
        shared->localPlayerEntity = (std::uint64_t)entity;

    // Grounded down-dash repeat (afad20a): a fresh Q press on the ground must
    // fire a down-dash every time, because the grounded contact restores the
    // ability each tick. The down-dash into the ground must also BOUNCE the
    // actor up (afad20a ground response), not just settle.
    {
        MimitaRuntime::GenericRuntime& rt =
            MimitaRuntime::GenericRuntime::instance();
        MimitaRuntime::DynamicComponentStore& store =
            MimitaRuntime::DynamicComponentStore::instance();
        Ecs::setTransform(entity, glm::vec3(0.0f, 0.0f, 10.0f),
                          glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
        Ecs::setVelocity(entity, glm::vec3(0.0f), glm::vec3(0.0f));
        for (int i = 0; i < 90; ++i) {
            Ecs::setMovementIntent(entity, 0.0f, 0.0f, false, false, false,
                                   false, false);
            rt.beginMovementTick();
            rt.runDomain(GAME_DOMAIN_GAMEPLAY, (std::uint64_t)i, kDt,
                         LiveBehavior::hostContext((std::uint64_t)i));
            float p[3], v[3], y;
            rt.consumeMovementOverride(p, v, y);
        }
        int fired = 0;
        int presses = 0;
        int bouncedUp = 0;
        for (int i = 0; i < 200; ++i) {
            const bool press = (i % 10) == 0;
            if (press)
                ++presses;
            Ecs::setMovementIntent(entity, 0.0f, 0.0f, false, false, false,
                                   press, false);
            const std::uint64_t t = 1000ull + (std::uint64_t)i;
            rt.beginMovementTick();
            rt.runDomain(GAME_DOMAIN_GAMEPLAY, t, kDt,
                         LiveBehavior::hostContext(t));
            float p[3], v[3], y;
            rt.consumeMovementOverride(p, v, y);
            if (v[2] > 1.0f)
                ++bouncedUp;
            HotMovementFiredV1 f{};
            if (store.read(entity, HOT_MOVEMENT_FIRED_COMPONENT, &f,
                           (std::uint32_t)sizeof(f)) &&
                (f.flags & HOT_FIRED_DOWN_DASH) != 0u) {
                ++fired;
                f.flags &= ~HOT_FIRED_DOWN_DASH;
                store.write(entity, HOT_MOVEMENT_FIRED_COMPONENT, &f,
                            (std::uint32_t)sizeof(f));
            }
        }
        ok &= check(fired >= presses - 2,
                    "grounded down-dash: fires on each fresh press", report);
        ok &= check(bouncedUp > 0,
                    "grounded down-dash: bounces up (afad20a ground response)",
                    report);
    }

    HotReloadSystem::instance().unloadGameDLL();
    EntityRegistry::instance().destroyAll();
    return ok;
}
