// 09 15 2026
/* purpose
* Implements the headless server spatial-authority self-test: the generic
* Transform/Velocity components are the authoritative spatial store for the
* migrated actor path, and typed ServerPlayer/ServerNpc fields are projections
* refreshed through the generic bridge. Proves spawn/teleport writes and
* movement reads share the same components. No player/NPC-specific ABI.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/server-spatial-authority-selftest.h"

#include <string>
#include <unordered_map>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/hot-reload-system.h"
#include "network/server-context.h"
#include "network/server.h"

using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const TransformComponent* xf(EntityId e)
{
    return EntityRegistry::instance().tryGet<TransformComponent>(e);
}
const VelocityComponent* vc(EntityId e)
{
    return EntityRegistry::instance().tryGet<VelocityComponent>(e);
}

bool nearV(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(a - b) < 0.001f;
}

} // namespace

bool runServerSpatialAuthoritySelfTest(std::string& report)
{
    bool ok = true;
    MimitaRuntime::DynamicComponentStore::instance().clear();
    EntityRegistry::instance().destroyAll();

    std::unordered_map<std::uint32_t, ServerPlayer> players;
    std::unordered_map<std::uint32_t, ServerNpc> npcs;
    players[7];
    npcs[9];

    ServerContextV1 ctx{};
    ctx.players = &players;
    ctx.npcs = &npcs;
    setActiveServerContext(&ctx);

    const EntityId pe = Ecs::ensure(EntityRealm::Server, EntityDomain::Player, 7);
    const EntityId ne = Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, 9);

    // ── Generic -> typed projection (authority refresh) ───────────────
    Ecs::setTransform(pe, glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                      90.0f, 0.0f);
    Ecs::setVelocity(pe, glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(0.1f, 0.0f, 0.0f));
    ok &= check(serverProjectActorSpatialFromGeneric(static_cast<std::uint64_t>(pe)),
                "player generic->typed projection runs", report);
    ok &= check(nearV(players[7].pos, glm::vec3(1.0f, 2.0f, 3.0f)) &&
                    nearV(players[7].vel, glm::vec3(4.0f, 5.0f, 6.0f)) &&
                    players[7].yaw == 90.0f,
                "typed player spatial fields mirror generic authority", report);

    // ── Typed -> generic projection (movement write back) ─────────────
    players[7].pos = glm::vec3(7.0f, 8.0f, 9.0f);
    players[7].vel = glm::vec3(1.0f, 1.0f, 1.0f);
    players[7].yaw = 45.0f;
    ok &= check(serverProjectActorSpatialToGeneric(static_cast<std::uint64_t>(pe)),
                "player typed->generic projection runs", report);
    ok &= check(xf(pe) && nearV(xf(pe)->position, glm::vec3(7.0f, 8.0f, 9.0f)) &&
                    xf(pe)->yaw == 45.0f && vc(pe) &&
                    nearV(vc(pe)->linear, glm::vec3(1.0f, 1.0f, 1.0f)),
                "generic Transform/Velocity receive the movement result", report);

    // ── Spawn/teleport and movement use the same components ───────────
    // Simulate an external writer (actor.spawn / teleport) writing generic, then
    // movement refreshing typed from it (no handoff to a second state).
    GameActorSpawnV1 spawn{};
    spawn.actorEntity = static_cast<std::uint64_t>(pe);
    spawn.position[0] = 20.0f; spawn.position[1] = 21.0f; spawn.position[2] = 22.0f;
    spawn.velocity[0] = 0.0f; spawn.velocity[1] = 0.0f; spawn.velocity[2] = 0.0f;
    spawn.yaw = 180.0f;
    spawn.health = 100;
    spawn.flags = 1u | 2u | 4u;
    serverSpawnOrResetActor(spawn);
    serverProjectActorSpatialFromGeneric(static_cast<std::uint64_t>(pe));
    ok &= check(nearV(players[7].pos, glm::vec3(20.0f, 21.0f, 22.0f)) &&
                    players[7].yaw == 180.0f,
                "actor.spawn and movement share the same generic state", report);

    // ── NPC generic -> typed and typed -> generic ─────────────────────
    Ecs::setTransform(ne, glm::vec3(10.0f, 11.0f, 12.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                      30.0f, 0.0f);
    Ecs::setVelocity(ne, glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f));
    serverProjectActorSpatialFromGeneric(static_cast<std::uint64_t>(ne));
    ok &= check(nearV(npcs[9].pos, glm::vec3(10.0f, 11.0f, 12.0f)) &&
                    nearV(npcs[9].vel, glm::vec3(2.0f, 0.0f, 0.0f)),
                "typed NPC spatial fields mirror generic authority", report);
    npcs[9].pos = glm::vec3(13.0f, 14.0f, 15.0f);
    serverProjectActorSpatialToGeneric(static_cast<std::uint64_t>(ne));
    ok &= check(xf(ne) && nearV(xf(ne)->position, glm::vec3(13.0f, 14.0f, 15.0f)),
                "generic Transform receives the NPC movement result", report);

    // ── Destroyed entity is not projected ─────────────────────────────
    EntityRegistry::instance().destroy(pe);
    ok &= check(!serverProjectActorSpatialToGeneric(static_cast<std::uint64_t>(pe)),
                "destroyed actor is not projected to generic state", report);

    // ── Deterministic repeat ──────────────────────────────────────────
    {
        Ecs::setTransform(ne, glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                          0.0f, 0.0f);
        serverProjectActorSpatialFromGeneric(static_cast<std::uint64_t>(ne));
        npcs[9].pos = glm::vec3(5.0f, 5.0f, 5.0f);
        serverProjectActorSpatialToGeneric(static_cast<std::uint64_t>(ne));
        const glm::vec3 first = xf(ne) ? xf(ne)->position : glm::vec3(0.0f);
        serverProjectActorSpatialToGeneric(static_cast<std::uint64_t>(ne));
        const glm::vec3 second = xf(ne) ? xf(ne)->position : glm::vec3(0.0f);
        ok &= check(nearV(first, second),
                    "spatial projection is deterministic for identical input", report);
    }

    setActiveServerContext(nullptr);
    MimitaRuntime::DynamicComponentStore::instance().clear();
    EntityRegistry::instance().destroyAll();
    return ok;
}
