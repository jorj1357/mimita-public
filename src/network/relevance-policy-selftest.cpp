// 09 15 2026
/* purpose
* Implements the headless generic relevance-policy self-test: the hot
* net.relevance policy classifies candidate entities (near/far/always-relevant),
* uses generic Transform state, works for a runtime-unknown entity, is
* deterministic, and reads generic ReplicationPolicy metadata. Transport stays
* cold. No player/NPC/entity-type replication policy type.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/relevance-policy-selftest.h"

#include <string>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/relevance.h"

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

void setCandidate(GameRelevanceQueryV1& q, std::uint32_t i, std::uint64_t entity,
                  float x, float y, float z, std::uint32_t flags)
{
    q.candidates[i].entity = entity;
    q.candidates[i].position[0] = x;
    q.candidates[i].position[1] = y;
    q.candidates[i].position[2] = z;
    q.candidates[i].flags = flags;
    q.candidates[i].reserved = 0;
}

void dispatch(GameRelevanceQueryV1& q)
{
    q.handled = 0;
    for (std::uint32_t i = 0; i < GAME_MAX_RELEVANCE_CANDIDATES; ++i) {
        q.outInclude[i] = 0;
        q.outTier[i] = 0;
    }
    q.outLowTierEveryNTicks = 0;
    LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_NET_RELEVANCE, &q, sizeof(q),
                                          q.tick, q.viewerEntity, 0);
}

} // namespace

bool runRelevancePolicySelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();
    EntityRegistry::instance().destroyAll();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);

    // Viewer at the origin; near/far/always candidates.
    GameRelevanceQueryV1 q{};
    q.viewerEntity = static_cast<std::uint64_t>(
        EntityRegistry::instance().createGeneric(EntityRealm::Server));
    q.viewerPosition[0] = 0.0f;
    q.viewerPosition[1] = 0.0f;
    q.viewerPosition[2] = 0.0f;
    q.tick = 10;
    q.candidateCount = 3;
    setCandidate(q, 0, 1001, 10.0f, 0.0f, 0.0f, 0u);     // near -> tier 0
    setCandidate(q, 1, 1002, 1000.0f, 0.0f, 0.0f, 0u);   // far -> tier 1
    setCandidate(q, 2, 1003, 1000.0f, 0.0f, 0.0f, 1u);   // always -> tier 0
    dispatch(q);

    ok &= check(q.handled == 1u, "hot net.relevance policy handles the query",
                report);
    ok &= check(q.outInclude[0] && q.outInclude[1] && q.outInclude[2],
                "policy includes near, far, and always candidates", report);
    ok &= check(q.outTier[0] == 0u && q.outTier[2] == 0u,
                "near + always-relevant are high tier (every tick)", report);
    ok &= check(q.outTier[1] == 1u && q.outLowTierEveryNTicks > 1u,
                "far candidate is low tier with a reduced cadence", report);

    // Determinism: identical input -> identical decision.
    {
        GameRelevanceQueryV1 q2 = q;
        dispatch(q2);
        bool same = (q2.handled == q.handled) &&
                    (q2.outLowTierEveryNTicks == q.outLowTierEveryNTicks);
        for (std::uint32_t i = 0; i < 3; ++i)
            if (q2.outTier[i] != q.outTier[i] || q2.outInclude[i] != q.outInclude[i])
                same = false;
        ok &= check(same, "relevance selection is deterministic for identical input",
                    report);
    }

    // Runtime-unknown entity: generic ReplicationPolicy metadata drives the
    // always-relevant decision (no conceptual type in the kernel/hot policy).
    {
        const EntityId runtimeEntity =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        std::uint32_t flags = 1u;  // always-relevant
        store.write(runtimeEntity, GAME_COMPONENT_REPLICATION_POLICY, &flags, sizeof(flags));
        ok &= check(store.has(runtimeEntity, GAME_COMPONENT_REPLICATION_POLICY),
                    "generic ReplicationPolicy schema registered for replication",
                    report);

        GameRelevanceQueryV1 q3{};
        q3.viewerEntity = q.viewerEntity;
        q3.viewerPosition[0] = 0.0f;
        q3.tick = 11;
        q3.candidateCount = 1;
        std::uint32_t readFlags = 0;
        store.read(runtimeEntity, GAME_COMPONENT_REPLICATION_POLICY, &readFlags,
                   sizeof(readFlags));
        setCandidate(q3, 0, static_cast<std::uint64_t>(runtimeEntity), 500.0f, 0.0f, 0.0f,
                    readFlags);
        dispatch(q3);
        ok &= check(q3.handled == 1u && q3.outTier[0] == 0u,
                    "runtime-unknown entity classified via generic policy metadata",
                    report);
    }

    // Destroyed entities never participate (registry no longer owns them).
    {
        const EntityId doomed =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        EntityRegistry::instance().destroy(doomed);
        ok &= check(!EntityRegistry::instance().alive(doomed),
                    "destroyed entity is not a relevance candidate", report);
    }

    runtime.deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    store.clear();
    EntityRegistry::instance().destroyAll();
    return ok;
}
