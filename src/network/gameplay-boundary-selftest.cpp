// 09 14 2026
/* purpose
* Implements the headless authoritative gameplay.60 boundary self-test.
* Does NOT own simulation, gameplay policy, or the renderer.
*/
#include "network/gameplay-boundary-selftest.h"

#include <string>
#include <unordered_map>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/actor-health.h"
#include "network/actor-state.h"
#include "network/server-context.h"
#include "network/server.h"

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

EntityId makeActor(EntityId& out, float x, float y, std::int32_t team,
                   const char* role)
{
    const EntityId entity = EntityRegistry::instance().createGeneric(EntityRealm::Server);
    actorHealthInit(static_cast<std::uint64_t>(entity), 100);
    actorStateWriteTeam(static_cast<std::uint64_t>(entity), team);
    actorStateWriteRole(static_cast<std::uint64_t>(entity), role);
    Ecs::setTransform(entity, glm::vec3(x, y, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                      0.0f, 0.0f);
    out = entity;
    return entity;
}

} // namespace

bool runGameplayBoundarySelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore::instance().clear();
    RelationshipStore::instance().clear();
    EntityRegistry::instance().destroyAll();
    actorHealthEnsureSchema();
    actorStateEnsureSchemas();

    // Minimal authoritative server context so damage.apply resolves.
    std::unordered_map<std::uint32_t, ServerPlayer> players;
    std::unordered_map<std::uint32_t, ServerNpc> npcs;
    std::uint32_t nextProjectileId = 1;
    std::uint32_t serverTick = 1;
    std::uint64_t totalPacketsOut = 0;
    ServerContextV1 context;
    context.players = &players;
    context.npcs = &npcs;
    context.nextProjectileId = &nextProjectileId;
    context.tick = &serverTick;
    context.totalPacketsOut = &totalPacketsOut;
    setActiveServerContext(&context);

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active on the server boundary", report);

    // Reset component/relationship/entity state AND re-register the package
    // schemas (a store clear wipes package-registered schemas too).
    auto resetHotWorld = [&runtime]() {
        (void)runtime;
        DynamicComponentStore::instance().clear();
        RelationshipStore::instance().clear();
        EntityRegistry::instance().destroyAll();
        HotReloadSystem::instance().unloadGameDLL();
        HotReloadSystem::instance().startup();
        actorHealthEnsureSchema();
        actorStateEnsureSchemas();
    };

    // Runtime monster-like entities on opposing teams, no MonsterType.
    EntityId a = 0;
    EntityId b = 0;
    makeActor(a, 0.0f, 0.0f, 0, "juggernaut");
    makeActor(b, 1.0f, 0.0f, 1, "juggernaut");

    auto healthOf = [](EntityId e) {
        std::int32_t cur = -1;
        actorHealthRead(static_cast<std::uint64_t>(e), &cur, nullptr, nullptr);
        return cur;
    };

    // One authoritative gameplay.60 run.
    GameplayContextV1* host = LiveBehavior::hostContext(100);
    runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f, host);

    std::uint64_t targetA = 0, targetB = 0;
    actorStateGetTarget(static_cast<std::uint64_t>(a), &targetA);
    actorStateGetTarget(static_cast<std::uint64_t>(b), &targetB);
    ok &= check(targetA == static_cast<std::uint64_t>(b) ||
                    targetB == static_cast<std::uint64_t>(a),
                "hot AI chose a target and wrote relationship.targets", report);

    const std::int32_t aAfterFirst = healthOf(a);
    const std::int32_t bAfterFirst = healthOf(b);
    ok &= check((aAfterFirst < 100 || bAfterFirst < 100),
                "hot AI performed an authoritative mutation via damage.apply",
                report);

    // Same tick again: the generic cooldown must prevent a second attack.
    runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f, host);
    ok &= check(healthOf(a) == aAfterFirst && healthOf(b) == bAfterFirst,
                "a second gameplay.60 run in the same tick does not double-attack",
                report);

    // A later tick attacks again (deterministic, cooldown elapsed).
    runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 200, 1.0f / 60.0f, host);
    ok &= check(healthOf(a) < aAfterFirst || healthOf(b) < bAfterFirst,
                "cooldown elapse lets the hot AI attack again", report);

    // Hot AI derived per-entity selection from generic role state.
    ok &= check(DynamicComponentStore::instance().has(a, gameHash("NpcAiSelection")),
                "hot system derived AI selection from generic role/team state",
                report);

    // Destroyed entity is ignored by the boundary.
    EntityRegistry::instance().destroy(b);
    DynamicComponentStore::instance().eraseEntity(b);
    RelationshipStore::instance().eraseEntity(b);
    const std::int32_t aBefore = healthOf(a);
    runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 300, 1.0f / 60.0f, host);
    ok &= check(!EntityRegistry::instance().alive(b) && healthOf(a) <= aBefore + 0,
                "destroyed entity is ignored by gameplay.60", report);

    // Deterministic ordering: two identical runs from identical state produce
    // identical results (no registration-order nondeterminism).
    {
        resetHotWorld();
        EntityId c = 0, d = 0;
        makeActor(c, 0.0f, 0.0f, 0, "sniper");
        makeActor(d, 1.0f, 0.0f, 1, "sniper");
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f,
                          LiveBehavior::hostContext(100));
        const std::int32_t c1 = healthOf(c);
        const std::int32_t d1 = healthOf(d);
        resetHotWorld();
        EntityId e = 0, f = 0;
        makeActor(e, 0.0f, 0.0f, 0, "sniper");
        makeActor(f, 1.0f, 0.0f, 1, "sniper");
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f,
                          LiveBehavior::hostContext(100));
        ok &= check(healthOf(e) == c1 && healthOf(f) == d1,
                    "gameplay.60 result is deterministic across identical runs", report);
    }

    // Generic NPC tool attack: equipped tool entity -> hot primary-use -> rocket.
    {
        resetHotWorld();
        EntityId shooter = 0;
        EntityId victim = 0;
        makeActor(shooter, 0.0f, 0.0f, 0, "rocketeer");
        makeActor(victim, 1.0f, 0.0f, 1, "target");
        const EntityId tool =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorStateEquipTool(static_cast<std::uint64_t>(shooter),
                            static_cast<std::uint64_t>(tool), 5 /* rocket key */);
        std::uint64_t equippedTool = 0, equippedKey = 0;
        const bool equipped = actorStateGetEquippedTool(
            static_cast<std::uint64_t>(shooter), &equippedTool, &equippedKey);
        ok &= check(equipped && equippedTool == static_cast<std::uint64_t>(tool) &&
                        equippedKey == 5,
                    "NPC actor equips a generic tool entity with its runtime key"
                    " [eq=" + std::to_string((int)equipped) + " tool=" +
                        std::to_string(equippedTool) + " want=" +
                        std::to_string((unsigned long long)tool) + " key=" +
                        std::to_string(equippedKey) + "]",
                    report);

        auto hotProjectileCount = []() {
            std::uint64_t buf[64] = {0};
            return DynamicComponentStore::instance().enumerate(
                gameHash("HotProjectileState"), buf, 64);
        };
        GameplayContextV1* ctx = LiveBehavior::hostContext(100);
        const std::uint32_t before = hotProjectileCount();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f, ctx);
        LiveBehavior::drainEvents(64);
        ok &= check(hotProjectileCount() > before,
                    "generic NPC tool action spawned the canonical hot projectile",
                    report);
        const std::uint32_t afterOne = hotProjectileCount();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f, ctx);
        LiveBehavior::drainEvents(64);
        ok &= check(hotProjectileCount() == afterOne,
                    "tool cooldown ensures one attack per decision window", report);
    }

    auto health = [](EntityId e) {
        std::int32_t cur = -1;
        actorHealthRead(static_cast<std::uint64_t>(e), &cur, nullptr, nullptr);
        return cur;
    };

    // Hot hitscan NPC tool via the same generic action path.
    {
        resetHotWorld();
        EntityId s = 0, v = 0;
        makeActor(s, 0.0f, 0.0f, 0, "shooter");
        makeActor(v, 1.0f, 0.0f, 1, "target");
        const EntityId tool = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorStateEquipTool(static_cast<std::uint64_t>(s), static_cast<std::uint64_t>(tool),
                            1 /* revolver key */);
        const std::int32_t before = health(v);
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f,
                          LiveBehavior::hostContext(100));
        LiveBehavior::drainEvents(64);
        ok &= check(health(v) < before, "hot hitscan NPC tool dealt damage", report);
        ok &= check(actorStateActionHandled(static_cast<std::uint64_t>(s), 100),
                    "generic handled gate set for a handled hitscan action", report);
    }

    // Hot melee NPC tool via the same generic action path.
    {
        resetHotWorld();
        EntityId s = 0, v = 0;
        makeActor(s, 0.0f, 0.0f, 0, "shooter");
        makeActor(v, 1.0f, 0.0f, 1, "target");
        const EntityId tool = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorStateEquipTool(static_cast<std::uint64_t>(s), static_cast<std::uint64_t>(tool),
                            4 /* swordsword key */);
        const std::int32_t before = health(v);
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f,
                          LiveBehavior::hostContext(100));
        LiveBehavior::drainEvents(64);
        ok &= check(health(v) < before, "hot melee NPC tool dealt damage", report);
    }

    // Unknown tool key: no hot behavior handles it; the gate stays clear so the
    // compatibility fallback may run (fails safe, no category knowledge).
    {
        resetHotWorld();
        EntityId s = 0, v = 0;
        makeActor(s, 0.0f, 0.0f, 0, "shooter");
        makeActor(v, 1.0f, 0.0f, 1, "target");
        const EntityId tool = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorStateEquipTool(static_cast<std::uint64_t>(s), static_cast<std::uint64_t>(tool),
                            0xDEADBEEFu);
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 100, 1.0f / 60.0f,
                          LiveBehavior::hostContext(100));
        LiveBehavior::drainEvents(64);
        ok &= check(!actorStateActionHandled(static_cast<std::uint64_t>(s), 100),
                    "unknown tool key is unhandled (safe cold fallback)", report);
    }

    runtime.deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    setActiveServerContext(nullptr);
    DynamicComponentStore::instance().clear();
    RelationshipStore::instance().clear();
    EntityRegistry::instance().destroyAll();
    return ok;
}

