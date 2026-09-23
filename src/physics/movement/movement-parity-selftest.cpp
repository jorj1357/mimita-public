// 09 14 2026
/* purpose
* Implements the hot movement parity/determinism harness.
* Does NOT run the game or own movement policy.
*/
#include "physics/movement/movement-parity-selftest.h"

#include <cmath>
#include <string>

#include <glm/glm.hpp>

#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "hot-reload/generic-runtime.h"
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
}

// Drives the registered hot movement system for `ticks` fixed steps and returns
// the last applied override position/velocity and whether an override occurred.
bool runHotMovement(EntityId entity, int ticks, float (&outPos)[3], float (&outVel)[3])
{
    Ecs::setTransform(entity, glm::vec3(0.0f, 0.0f, 2.0f),
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

bool runMovementParitySelfTest(std::string& report)
{
    bool ok = true;

    World world;
    addFloor(world);
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().startup();

    const EntityId entity = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 1);
    Ecs::setBody(entity, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entity, 0.0f, 0.0f, false, false, false, false, false);
    if (GameSharedStateV1* shared =
            MimitaRuntime::GenericRuntime::instance().sharedState()) {
        shared->magic = GAME_SHARED_MAGIC;
        shared->modeFlags = GAME_MODE_FLAG_HOT_MOVEMENT;  // hot step, not free-fly
        shared->localPlayerEntity = (std::uint64_t)entity;
    }
    LiveBehavior::setDispatchWorld(&world);

    ok &= check(MimitaRuntime::GenericRuntime::instance().systemCount() > 0,
                "hot package active with systems", report);

    // Two identical runs from FRESH entities must agree exactly. afad20a ground
    // contacts bounce, and the bounce cooldown is per-entity state; a fresh
    // entity for each run removes that carry-over.
    const EntityId entityB = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 2);
    Ecs::setBody(entityB, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entityB, 0.0f, 0.0f, false, false, false, false, false);
    const EntityId entityC = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 3);
    Ecs::setBody(entityC, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entityC, 0.0f, 0.0f, false, false, false, false, false);

    GameSharedStateV1* shared =
        MimitaRuntime::GenericRuntime::instance().sharedState();

    float posA[3] = {0.0f, 0.0f, 0.0f};
    float velA[3] = {0.0f, 0.0f, 0.0f};
    float posB[3] = {0.0f, 0.0f, 0.0f};
    float velB[3] = {0.0f, 0.0f, 0.0f};
    if (shared)
        shared->localPlayerEntity = (std::uint64_t)entityB;
    const bool gotA = runHotMovement(entityB, 120, posA, velA);
    if (shared)
        shared->localPlayerEntity = (std::uint64_t)entityC;
    const bool gotB = runHotMovement(entityC, 120, posB, velB);
    ok &= check(gotA && gotB, "hot movement produced overrides", report);
    ok &= check(std::fabs(posA[2] - posB[2]) < 1e-4f &&
                    std::fabs(velA[2] - velB[2]) < 1e-4f,
                "hot movement path deterministic", report);

    // It must land on the floor, not fall through, and rest vertically
    // (afad20a snaps a small grounded vertical velocity out).
    ok &= check(posA[2] > 0.5f, "hot movement lands on floor", report);
    ok &= check(std::fabs(velA[2]) < 0.5f, "hot movement vertical rest", report);

    // Landing height: the capsule center rests at radius + segment half, i.e.
    // tipHalf = 0.9 for this test capsule (radius 0.4, halfHeight 0.9). The old
    // cold capsule primitive is gone; this is a fixed expectation.
    ok &= check(std::fabs(posA[2] - 0.9f) < 0.35f,
                "hot landing near expected capsule rest height", report);
    ok &= check(std::isfinite(posA[2]), "hot result finite", report);

    HotReloadSystem::instance().unloadGameDLL();
    EntityRegistry::instance().destroyAll();
    return ok;
}
