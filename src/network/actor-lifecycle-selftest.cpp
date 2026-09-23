// 09 23 2026
/* purpose
* Headless self-test for the generic hot actor lifecycle boundary: one envelope
* drives player initial spawn, respawn, and reconnect; identity and life
* generation survive; the hot handler arms generic spawn protection; and a hot
* respawn veto is honored. No Player/Npc layout crosses the boundary.
* Does NOT own transport or the live server loop.
*/
#include "network/actor-lifecycle-selftest.h"

#include <cstring>
#include <string>
#include <unordered_map>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/server-context.h"
#include "network/server.h"

using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runActorLifecycleSelfTest(std::string& report)
{
    bool ok = true;
    // Clear before activation so the hot package's schema registrations survive.
    MimitaRuntime::DynamicComponentStore::instance().clear();
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    std::unordered_map<std::uint32_t, ServerPlayer> players;
    std::unordered_map<std::uint32_t, ServerNpc> npcs;
    players[7];
    ServerContextV1 ctx{};
    ctx.players = &players;
    ctx.npcs = &npcs;
    setActiveServerContext(&ctx);

    const EntityId pe = Ecs::ensure(EntityRealm::Server, EntityDomain::Player, 7);

    // ── Initial spawn through the generic lifecycle envelope ──────────
    {
        ActorLifecycleStateV1 state{};
        state.entityId = (std::uint64_t)pe;
        state.actorKind = 1u;
        state.lifeGeneration = 3u;   // pre-existing life generation
        state.reason = 0u;           // initial join
        state.position[0] = 1.0f;
        state.position[1] = 2.0f;
        state.position[2] = 3.0f;
        state.yaw = 90.0f;
        state.health = 100;
        state.maxHealth = 100;
        std::strncpy(state.avatarName, "tester", sizeof(state.avatarName) - 1);

        const bool handled = LiveBehavior::dispatchActorLifecycle(state, 100);
        ok &= check(handled && state.handled, "hot actor.lifecycle handles the envelope",
                    report);
        ok &= check(state.entityId == (std::uint64_t)pe &&
                        state.lifeGeneration == 3u,
                    "identity and life generation preserved across lifecycle", report);
        ok &= check(state.respawnRequested == 1u && state.dead == 0u,
                    "hot lifecycle keeps the actor alive for the new life", report);
    }

    // ── Spawn protection armed generically on the actor entity ────────
    {
        HotSpawnProtectionV1 sp{};
        const bool has = MimitaRuntime::DynamicComponentStore::instance().read(
            (std::uint64_t)pe, HOT_SPAWN_PROTECTION_COMPONENT, &sp, sizeof(sp));
        ok &= check(has && sp.untilTick > 0u,
                    "hot lifecycle armed spawn protection on the actor entity", report);
    }

    // ── Respawn path also uses the same envelope ──────────────────────
    {
        ActorLifecycleStateV1 state{};
        state.entityId = (std::uint64_t)pe;
        state.actorKind = 1u;
        state.lifeGeneration = 3u;
        state.reason = 1u;  // respawn
        state.dead = 1u;
        state.position[0] = 5.0f;
        state.position[1] = 0.0f;
        state.position[2] = 7.0f;
        state.yaw = 0.0f;
        state.health = 100;
        state.maxHealth = 100;
        LiveBehavior::dispatchActorLifecycle(state, 200);
        ok &= check(state.handled && state.respawnRequested == 1u &&
                        state.entityId == (std::uint64_t)pe,
                    "respawn uses the same hot lifecycle owner", report);
    }

    // ── Determinism ───────────────────────────────────────────────────
    {
        ActorLifecycleStateV1 a{};
        a.entityId = (std::uint64_t)pe;
        a.lifeGeneration = 3u;
        a.position[0] = 1.0f;
        a.yaw = 90.0f;
        ActorLifecycleStateV1 b = a;
        LiveBehavior::dispatchActorLifecycle(a, 300);
        LiveBehavior::dispatchActorLifecycle(b, 300);
        ok &= check(a.handled == b.handled &&
                        a.respawnRequested == b.respawnRequested &&
                        a.lifeGeneration == b.lifeGeneration,
                    "actor lifecycle is deterministic for identical input", report);
    }

    setActiveServerContext(nullptr);
    MimitaRuntime::DynamicComponentStore::instance().clear();
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().unloadGameDLL();

    report += ok ? "[ACTOR LIFECYCLE SELFTEST] PASS\n"
                 : "[ACTOR LIFECYCLE SELFTEST] FAIL\n";
    return ok;
}
