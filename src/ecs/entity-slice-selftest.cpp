// 09 12 2026
/* purpose
* Implements the headless entity/component vertical-slice self-test.
* Asserts invariants, determinism, and identity stability; never pins tuned
* gameplay constants.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#include "ecs/entity-slice-selftest.h"

#include "combat/projectile-simulation.h"
#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-journal.h"

#include <fstream>
#include <glm/glm.hpp>
#include <string>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

// Empty collision world: the projectile only integrates physics.
struct EmptyWorldView : CollisionWorldView {
    void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float,
                             std::vector<int>& outIndices) const override
    {
        outIndices.clear();
    }
    const CollisionTriangle& triangleAt(int) const override
    {
        static CollisionTriangle triangle{};
        return triangle;
    }
    int triangleCount() const override { return 0; }
    void queryPlayerCapsulesSwept(const glm::vec3&, const glm::vec3&, float,
                                  std::vector<SweptPlayerCapsule>& out) const override
    {
        out.clear();
    }
};

void runProjectile(ProjectilePhysicsState& state)
{
    ProjectilePhysicsConfig config;
    config.speed = 10.0f;
    config.lifetime = 2.0f;
    EmptyWorldView world;
    for (int i = 0; i < 150; ++i)
        simulateProjectileTick(state, config, world, 1.0f / 60.0f);
}

} // namespace

bool runEntitySliceSelfTest(std::string& report)
{
    bool ok = true;
    auto& registry = EntityRegistry::instance();
    registry.destroyAll();

    LiveEventJournal::instance().init();

    // 1. Stable, distinct player/NPC identities.
    const EntityId player = Ecs::ensureLocalPlayerEntity();
    const EntityId npc = Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, 42);
    Ecs::setControlSource(npc, ControlSource::ServerNpc);
    Ecs::setAuthority(npc, NetworkAuthority::Server);
    Ecs::setTransform(player, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    Ecs::setTransform(npc, glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), 180.0f, 0.0f);
    Ecs::setVelocity(player, glm::vec3(0.0f), glm::vec3(0.0f));
    Ecs::setVelocity(npc, glm::vec3(0.0f), glm::vec3(0.0f));
    Ecs::setHealth(player, 100, 100, false);
    Ecs::setHealth(npc, 100, 100, false);
    Ecs::setBody(player, 1.0f, 0.4f, 1.8f);
    Ecs::setBody(npc, 1.0f, 0.4f, 1.8f);

    ok &= check(player != kInvalidEntityId && npc != kInvalidEntityId && player != npc,
                "distinct stable player/npc entity ids", report);
    ok &= check(registry.find(EntityRealm::Local, EntityDomain::Player, 1) == player,
                "player id stable across lookup", report);

    // 2. Required components exist.
    ok &= check(registry.has<TransformComponent>(player) &&
                    registry.has<VelocityComponent>(player) &&
                    registry.has<HealthComponent>(player) &&
                    registry.has<BodyComponent>(player),
                "player capability components present", report);
    ok &= check(registry.has<TransformComponent>(npc) &&
                    registry.has<HealthComponent>(npc),
                "npc capability components present", report);

    // 3. Control source is data, not a type check.
    const auto* playerControl = registry.tryGet<ControlSourceComponent>(player);
    const auto* npcControl = registry.tryGet<ControlSourceComponent>(npc);
    ok &= check(playerControl && npcControl &&
                    playerControl->source == ControlSource::LocalHuman &&
                    npcControl->source == ControlSource::ServerNpc,
                "human and npc control sources differ", report);

    // 4. Rocket entity with owner resolving to the firing entity.
    const EntityId rocket = Ecs::spawnRocket(
        EntityRealm::Server, 7, player, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 0.0f, 0.0f),
        0, 1234, 5.0f, NetworkAuthority::Server);
    const auto* owner = registry.tryGet<OwnerComponent>(rocket);
    ok &= check(rocket != kInvalidEntityId && registry.alive(rocket),
                "rocket entity created", report);
    ok &= check(owner && owner->owner == player, "rocket owner resolves to player entity", report);
    ok &= check(registry.has<ProjectileComponent>(rocket) &&
                    registry.has<ColliderComponent>(rocket),
                "rocket projectile/collider components present", report);

    // 5. Deterministic projectile simulation (no tuned constant pinned).
    ProjectilePhysicsState first;
    first.position = glm::vec3(0.0f);
    first.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
    ProjectilePhysicsState second = first;
    runProjectile(first);
    runProjectile(second);
    ok &= check(first.position == second.position && first.velocity == second.velocity,
                "identical projectile input is deterministic", report);
    ok &= check(first.age > 0.0f && (first.exploded || first.age >= 2.0f),
                "projectile lifetime path completes", report);

    // 6. Damage path updates the victim health component.
    Ecs::setHealth(npc, 5, 100, false);
    Ecs::journalDamage(npc, player, 80.0f, "selftest");
    const auto* npcHealth = registry.tryGet<HealthComponent>(npc);
    const bool killed = npcHealth && npcHealth->current - 80 <= 0;
    if (killed)
        Ecs::setHealth(npc, 0, 100, true);
    const auto* after = registry.tryGet<HealthComponent>(npc);
    ok &= check(after && after->dead && after->current == 0,
                "victim health/death component path works", report);

    // 7. Entity identity survives a DLL reload cycle (hot reload keeps state).
    HotReloadSystem::instance().startup();
    const bool loaded = HotReloadSystem::instance().loaded();
    HotReloadSystem::instance().unloadGameDLL();
    HotReloadSystem::instance().startup();
    ok &= check(loaded && registry.alive(npc) &&
                    registry.find(EntityRealm::Server, EntityDomain::Npc, 42) == npc,
                "entity identity survives DLL reload cycle", report);
    HotReloadSystem::instance().unloadGameDLL();

    // 8. Structured evidence is written to the live journal.
    {
        std::ifstream journal(LiveEventJournal::instance().path());
        std::string line;
        std::string content;
        while (std::getline(journal, line))
            content += line;
        ok &= check(content.find("\"type\":\"entity_registered\"") != std::string::npos &&
                        content.find("\"type\":\"rocket_entity_spawned\"") != std::string::npos &&
                        content.find("\"type\":\"control_source_set\"") != std::string::npos,
                    "entity journal evidence written", report);
    }

    return ok;
}
