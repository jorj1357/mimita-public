// 09 14 2026
/* purpose
* Implements the headless generic-objective self-test: objective entities as
* entity + components + relationships, generic interaction/state-change events,
* carrier pickup/drop, timer/progress, completion feeding generic match
* mechanisms, a second objective behavior on the same primitives, destroyed-actor
* cleanup, and generic replication registration. No ObjectiveType / BombManager.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/objective-generic-selftest.h"

#include <string>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/actor-health.h"
#include "network/objective-events.h"
#include "network/packets.h"
#include "network/server-gamemode.h"

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

struct ObjectiveStateMirror {
    std::uint32_t kind;
    std::uint32_t state;
    std::uint32_t ownerTeam;
    std::uint32_t flags;
};
struct ProgressMirror {
    float progress;
    float goal;
    float ratePerTick;
    std::uint32_t reserved;
};
struct EventLogMirror {
    std::uint32_t lastState;
    std::uint32_t count;
    std::uint32_t tick;
    std::uint32_t reserved;
};

const std::uint64_t kState = gameHash("ObjectiveState");
const std::uint64_t kProgress = gameHash("ObjectiveProgress");
const std::uint64_t kEventLog = gameHash("ObjectiveEventLog");
const std::uint64_t kAtSite = gameHash("objective.at-site");
const std::uint64_t kCarriedBy = gameHash("objective.carried-by");
const std::uint64_t kInteract = gameHash("objective.interact");
const std::uint64_t kCarryDomain = gameHash("objective.carry");
const std::uint64_t kHoldDomain = gameHash("objective.hold");
const std::uint64_t kOwnership = gameHash("ObjectiveOwnership");

std::uint64_t findObjective(std::uint32_t kind)
{
    for (EntityId e : EntityRegistry::instance().all()) {
        ObjectiveStateMirror s{};
        if (DynamicComponentStore::instance().read(e, kState, &s, sizeof(s)) &&
            s.kind == kind)
            return static_cast<std::uint64_t>(e);
    }
    return 0;
}

bool readState(std::uint64_t objective, ObjectiveStateMirror& out)
{
    return DynamicComponentStore::instance().read(
        static_cast<EntityId>(objective), kState, &out, sizeof(out));
}

std::uint64_t carriedBy(std::uint64_t objective)
{
    std::uint64_t out[1] = {0};
    std::uint64_t val[1] = {0};
    const std::size_t n = RelationshipStore::instance().query(
        kCarriedBy, static_cast<EntityId>(objective), out, val, 1);
    return n > 0 ? out[0] : 0;
}

EntityId makeActor(float x, float y, float z)
{
    const EntityId e = EntityRegistry::instance().createGeneric(EntityRealm::Server);
    actorHealthInit(static_cast<std::uint64_t>(e), 100);
    Ecs::setTransform(e, glm::vec3(x, y, z), glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    return e;
}

void dispatchInteract(std::uint64_t actor, std::uint64_t objective,
                      std::uint64_t site, std::uint32_t tick)
{
    GameObjectiveInteractV1 p{};
    p.actorEntity = actor;
    p.objectiveEntity = objective;
    p.siteEntity = site;
    p.actionHash = gameHash("interact.primary");
    p.tick = tick;
    p.inputFlags = 1;
    LiveBehavior::dispatchGameplayEvent64(kInteract, &p, sizeof(p), tick, actor,
                                          objective);
}

void runDomain(GenericRuntime& runtime, std::uint64_t domain, int ticks,
               std::uint64_t baseTick)
{
    for (int i = 0; i < ticks; ++i) {
        const std::uint64_t t = baseTick + static_cast<std::uint64_t>(i);
        runtime.runDomain(domain, t, 1.0f / 60.0f, LiveBehavior::hostContext(t));
    }
}

std::uint64_t siteOf(std::uint64_t objective)
{
    std::uint64_t out[1] = {0};
    std::uint64_t val[1] = {0};
    const std::size_t n = RelationshipStore::instance().query(
        kAtSite, static_cast<EntityId>(objective), out, val, 1);
    return n > 0 ? out[0] : 0;
}

} // namespace

bool runObjectiveGenericSelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);
    ok &= check(runtime.hasMode(kCarryDomain) && runtime.hasMode(kHoldDomain),
                "runtime objective modes registered (no EXE enum)", report);

    ServerGamemodeState& d = serverGamemodeState();
    d.enabled = true;
    d.matchOver = false;
    serverMatchResetEntity();
    const std::uint64_t match = serverMatchEntity();
    d.phase = DUEL_PHASE_ACTIVE;

    // ── Carry-to-site behavior ────────────────────────────────────────
    runtime.setActiveModeDomain(kCarryDomain);
    const EntityId carrier = makeActor(0.0f, 0.0f, 0.0f);
    runDomain(runtime, kCarryDomain, 1, 1);
    const std::uint64_t carry = findObjective(0 /*carry*/);
    ok &= check(carry != 0, "hot objective entity created after startup", report);
    ok &= check(store.has(static_cast<EntityId>(carry), kProgress),
                "objective timer/progress is generic dynamic state", report);
    ok &= check(store.has(static_cast<EntityId>(match), kOwnership),
                "hot behavior claimed ObjectiveOwnership (cold bomb bypassed)",
                report);

    const std::uint64_t site = siteOf(carry);
    ok &= check(site != 0, "site is a separate entity linked by relationship",
                report);

    // Interaction adds the generic carried-by edge; same objective EntityId.
    dispatchInteract(static_cast<std::uint64_t>(carrier), carry, site, 10);
    ok &= check(carriedBy(carry) == static_cast<std::uint64_t>(carrier),
                "pickup adds objective --carried-by--> actor", report);
    ObjectiveStateMirror s{};
    readState(carry, s);
    ok &= check(s.state == 1, "objective state reflects carried", report);

    // Move the carrier onto the site; progress accrues and completion feeds
    // the generic match mechanism (match.finish -> RESULTS).
    Ecs::setTransform(carrier, glm::vec3(10.0f, 0.0f, 0.0f),
                      glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    d.matchOver = false;
    runDomain(runtime, kCarryDomain, 12, 100);
    readState(carry, s);
    ProgressMirror p{};
    store.read(static_cast<EntityId>(carry), kProgress, &p, sizeof(p));
    ok &= check(p.progress >= p.goal && s.state == 2,
                "interaction progress completes the objective", report);
    ok &= check(d.phase == DUEL_PHASE_RESULTS,
                "objective completion feeds hot match logic (results phase)", report);

    // Emit -> generic dispatch round trip observed as replicated state.
    LiveBehavior::drainEvents(16);
    EventLogMirror log{};
    const bool logOk =
        store.read(static_cast<EntityId>(carry), kEventLog, &log, sizeof(log));
    ok &= check(logOk && log.count >= 1,
                "objective.state-changed observed through generic dispatch", report);

    // Drop removes the relationship (same objective EntityId survives).
    dispatchInteract(static_cast<std::uint64_t>(carrier), carry, site, 200);
    ok &= check(carriedBy(carry) == 0,
                "drop removes objective --carried-by--> actor", report);

    // Destroyed carrier is ignored (generic lifecycle cleanup, no stale edge).
    {
        ObjectiveStateMirror s2 = s;
        s2.state = 1;
        store.write(static_cast<EntityId>(carry), kState, &s2, sizeof(s2));
        RelationshipStore::instance().add(kCarriedBy, static_cast<EntityId>(carry),
                                          carrier, 0);
        EntityRegistry::instance().destroy(carrier);
        runDomain(runtime, kCarryDomain, 1, 300);
        ok &= check(carriedBy(carry) == 0,
                    "destroyed/stale carrier relationship is cleaned up", report);
    }

    // ── Second behavior on the same primitives: hold-area ─────────────
    runtime.setActiveModeDomain(kHoldDomain);
    const EntityId holder = makeActor(10.0f, 0.0f, 0.0f);
    d.matchOver = false;
    d.phase = DUEL_PHASE_ACTIVE;
    runDomain(runtime, kHoldDomain, 12, 400);
    const std::uint64_t hold = findObjective(1 /*hold*/);
    ok &= check(hold != 0, "second objective behavior uses same entity/components",
                report);
    readState(hold, s);
    ok &= check(s.state == 2 && s.ownerTeam == 1,
                "second behavior completes via progress + owner state", report);
    ok &= check(d.phase == DUEL_PHASE_RESULTS || d.matchOver,
                "second behavior feeds the generic match mechanism", report);
    (void)holder;

    // ── Generic replication registration (no objective packet) ────────
    ok &= check(store.has(static_cast<EntityId>(carry), kState) &&
                    store.has(static_cast<EntityId>(hold), kProgress),
                "objective schemas registered for generic replication", report);

    runtime.deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    store.clear();
    return ok;
}
