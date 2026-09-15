// 09 14 2026
/* purpose
* Implements the headless Counter-Strike-like full-round self-test: hot-owned
* countdown, participant team assignment, round-start spawn/reset via the
* generic actor.spawn capability, generic authoritative Transform/Velocity,
* complete phase cycle (countdown/active/results/intermission/next round), the
* objective state machine, and falsification/generic-actor cases. No
* BombManager/ObjectiveType/spawn ABI.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/counterstrike-selftest.h"

#include <string>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/actor-health.h"
#include "network/actor-state.h"
#include "network/objective-events.h"
#include "network/packets.h"
#include "network/server-context.h"
#include "network/server-gamemode.h"

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

struct CsObjectiveMirror {
    std::uint32_t state;
    std::uint32_t ownerTeam;
    std::uint32_t r0;
    std::uint32_t r1;
};
struct CsRoundMirror {
    std::uint32_t phase;
    std::int32_t timer;
    std::uint32_t roundNumber;
    std::uint32_t ended;
    std::uint64_t roundStartTick;
    std::uint32_t teamsAssigned;
    std::uint32_t reserved;
};

const std::uint64_t kObjState = gameHash("CsObjectiveState");
const std::uint64_t kSiteState = gameHash("CsSiteState");
const std::uint64_t kRoundState = gameHash("CsRoundState");
const std::uint64_t kDead = gameHash("CsDead");
const std::uint64_t kOwnership = gameHash("ObjectiveOwnership");
const std::uint64_t kPhaseOwnership = gameHash("MatchPhaseOwnership");
const std::uint64_t kTeamState = gameHash("ActorTeamState");
const std::uint64_t kCarriedBy = gameHash("objective.carried-by");
const std::uint64_t kAtSite = gameHash("objective.at-site");
const std::uint64_t kInteract = gameHash("objective.interact");
const std::uint64_t kCsMode = gameHash("counterstrike");
const std::uint64_t kSpawnKind = gameHash("spawn.team");

GenericRuntime* gRuntime = nullptr;
std::uint64_t gTick = 0;
std::uint64_t gDomain = 0;

void step(int n)
{
    for (int i = 0; i < n; ++i) {
        ++gTick;
        gRuntime->runDomain(gDomain, gTick, 1.0f / 60.0f,
                            LiveBehavior::hostContext(gTick));
    }
}

template <typename Pred>
void runUntil(Pred pred, int maxTicks)
{
    for (int i = 0; i < maxTicks; ++i) {
        if (pred())
            return;
        step(1);
    }
}

std::uint64_t findByComponent(std::uint64_t typeId)
{
    for (EntityId e : EntityRegistry::instance().all())
        if (DynamicComponentStore::instance().has(e, typeId))
            return static_cast<std::uint64_t>(e);
    return 0;
}

bool readObj(std::uint64_t objective, CsObjectiveMirror& out)
{
    return DynamicComponentStore::instance().read(
        static_cast<EntityId>(objective), kObjState, &out, sizeof(out));
}
bool readRound(std::uint64_t match, CsRoundMirror& out)
{
    return DynamicComponentStore::instance().read(
        static_cast<EntityId>(match), kRoundState, &out, sizeof(out));
}

std::uint64_t carriedBy(std::uint64_t objective)
{
    std::uint64_t to = 0, value = 0;
    return RelationshipStore::instance().query(
               kCarriedBy, static_cast<EntityId>(objective), &to, &value, 1) > 0
        ? to : 0;
}
std::uint64_t atSite(std::uint64_t objective)
{
    std::uint64_t to = 0, value = 0;
    return RelationshipStore::instance().query(
               kAtSite, static_cast<EntityId>(objective), &to, &value, 1) > 0
        ? to : 0;
}

EntityId makeActor(EntityDomain domain, std::uint32_t legacyId)
{
    const EntityId e = Ecs::ensure(EntityRealm::Server, domain, legacyId);
    actorHealthInit(Ecs::raw(e), 100);
    Ecs::setTransform(e, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    return e;
}

std::int32_t teamOf(EntityId e)
{
    std::int32_t team = -1;
    actorStateReadTeam(Ecs::raw(e), &team);
    return team;
}

void dispatchKilled(EntityId victim)
{
    GameActorKilledV1 k{};
    k.victimEntity = static_cast<std::uint64_t>(victim);
    k.victimId = static_cast<std::uint32_t>(victim);
    k.tick = static_cast<std::uint32_t>(gTick);
    LiveBehavior::dispatchActorKilled(k, gTick);
}

} // namespace

bool runCounterstrikeSelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    gRuntime = &runtime;
    ok &= check(runtime.active(), "hot package active", report);
    ok &= check(runtime.hasMode(kCsMode),
                "runtime counterstrike mode registered (no EXE enum)", report);
    gDomain = runtime.modeDomain(kCsMode);
    runtime.setActiveModeDomain(gDomain);

    ServerGamemodeState& d = serverGamemodeState();
    d.enabled = true;
    d.matchOver = false;
    d.goalValue = 8;
    d.roundWins[0] = d.roundWins[1] = 0;
    d.mapId = "dust2cyberiav3";
    d.phase = DUEL_PHASE_WAITING;
    serverMatchResetEntity();
    const std::uint64_t match = serverMatchEntity();

    // Real map spawn anchors project generically.
    {
        GameMapAnchorV1 anchors[GAME_MAX_MAP_ANCHORS];
        const std::uint32_t n = serverMapAnchors(anchors, GAME_MAX_MAP_ANCHORS);
        std::uint32_t spawns = 0, sites = 0;
        for (std::uint32_t i = 0; i < n; ++i) {
            if (anchors[i].kind == kSpawnKind) ++spawns;
            if (anchors[i].kind == gameHash("objective.site")) ++sites;
        }
        ok &= check(spawns >= 2 && sites >= 1,
                    "real map spawn/objective anchors project generically", report);
    }

    const EntityId pT = makeActor(EntityDomain::Player, 1);
    const EntityId pCt = makeActor(EntityDomain::Player, 2);
    const EntityId nT = makeActor(EntityDomain::Npc, 3);
    const EntityId nCt = makeActor(EntityDomain::Npc, 4);

    // ── Hot-owned countdown -> spawn -> active ────────────────────────
    runUntil([&] { CsRoundMirror r{}; return readRound(match, r) && r.phase == 1; }, 600);
    {
        CsRoundMirror r{};
        ok &= check(readRound(match, r) && r.phase == 1,
                    "hot CS owns countdown and reaches ACTIVE", report);
        ok &= check(d.phase == DUEL_PHASE_ACTIVE,
                    "hot CS sets the kernel phase ACTIVE via match.setPhase", report);
        ok &= check(store.has(static_cast<EntityId>(match), kPhaseOwnership) &&
                        store.has(static_cast<EntityId>(match), kOwnership),
                    "hot CS claims MatchPhaseOwnership + ObjectiveOwnership", report);
    }

    // Deterministic team assignment over sorted actor ids.
    {
        const EntityId ids[4] = {pT, pCt, nT, nCt};
        bool okTeams = true;
        for (int i = 0; i < 4; ++i)
            if (teamOf(ids[i]) != (i % 2))
                okTeams = false;
        ok &= check(okTeams, "hot CS assigns teams deterministically", report);
    }

    // Round-start spawn/reset through the generic actor.spawn mechanism.
    {
        bool spawned = true;
        bool velocityZero = true;
        bool healed = true;
        for (EntityId e : {pT, pCt, nT, nCt}) {
            const auto* t = EntityRegistry::instance().tryGet<TransformComponent>(e);
            const auto* v = EntityRegistry::instance().tryGet<VelocityComponent>(e);
            if (!t)
                spawned = false;
            if (!v || glm::length(v->linear) > 0.001f)
                velocityZero = false;
            std::int32_t cur = 0, mx = 0;
            bool dead = true;
            actorHealthRead(Ecs::raw(e), &cur, &mx, &dead);
            if (dead || cur <= 0)
                healed = false;
        }
        ok &= check(spawned, "hot CS writes generic authoritative Transform on spawn",
                    report);
        ok &= check(velocityZero, "hot CS resets generic Velocity on spawn", report);
        ok &= check(healed, "hot CS resets generic health/dead on spawn", report);
    }

    const std::uint64_t objective = findByComponent(kObjState);
    const std::uint64_t site = findByComponent(kSiteState);
    ok &= check(objective != 0 && site != 0,
                "objective + site entities exist for the round", report);

    // ── Scenario A: plant -> defuse (defenders win) ───────────────────
    runUntil([&] { return d.roundWins[1] == 1; }, 900);
    {
        CsRoundMirror r{};
        readRound(match, r);
        ok &= check(d.roundWins[1] == 1 && d.phase == DUEL_PHASE_RESULTS,
                    "plant -> defuse ends the round for defenders", report);
        ok &= check(r.phase == 2, "hot CS enters RESULTS phase after the round",
                    report);
    }

    // ── RESULTS -> INTERMISSION -> next round spawn ───────────────────
    runUntil([&] {
        CsRoundMirror r{};
        return readRound(match, r) && r.roundNumber == 2 && r.phase == 1;
    }, 2400);
    {
        CsRoundMirror r{};
        readRound(match, r);
        ok &= check(r.roundNumber == 2 && r.phase == 1,
                    "hot CS completes results/intermission into the next round",
                    report);
        bool respawned = true;
        for (EntityId e : {pT, pCt, nT, nCt}) {
            const auto* t = EntityRegistry::instance().tryGet<TransformComponent>(e);
            if (!t)
                respawned = false;
        }
        ok &= check(respawned, "next round respawns/resets actors again", report);
    }

    // ── Scenario B: plant -> explosion (attackers win) ────────────────
    for (EntityId ct : {pCt, nCt})
        Ecs::setTransform(ct, glm::vec3(0.0f, 0.0f, 0.0f),
                          glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    runUntil([&] { return d.roundWins[0] == 1; }, 5000);
    ok &= check(d.roundWins[0] == 1, "plant without defuse explodes (attackers win)",
                report);

    // ── Round reset clears objective edges (checked during countdown) ─
    runUntil([&] { CsRoundMirror r{}; return readRound(match, r) && r.phase == 0; },
             2400);
    ok &= check(carriedBy(objective) == 0 && atSite(objective) == 0,
                "no stale objective relationships survive a round reset", report);

    // ── Falsification: actor killed mid-round revives next round ──────
    runUntil([&] { CsRoundMirror r{}; return readRound(match, r) && r.phase == 1; },
             1200);
    dispatchKilled(pT);
    step(1);
    runUntil([&] {
        CsRoundMirror r{};
        return readRound(match, r) && r.phase == 1 && r.roundNumber >= 4;
    }, 6000);
    {
        std::int32_t cur = 0, mx = 0;
        bool dead = true;
        actorHealthRead(Ecs::raw(pT), &cur, &mx, &dead);
        ok &= check(!dead && cur > 0,
                    "killed actor is revived/reset at the next round start", report);
    }

    // ── Falsification: duplicate interaction is safe ──────────────────
    {
        GameObjectiveInteractV1 ip{};
        ip.actorEntity = static_cast<std::uint64_t>(pT);
        ip.objectiveEntity = objective;
        ip.actionHash = gameHash("interact.primary");
        ip.tick = static_cast<std::uint32_t>(gTick);
        LiveBehavior::dispatchGameplayEvent64(kInteract, &ip, sizeof(ip), gTick,
                                              static_cast<std::uint64_t>(pT), objective);
        LiveBehavior::dispatchGameplayEvent64(kInteract, &ip, sizeof(ip), gTick,
                                              static_cast<std::uint64_t>(pT), objective);
        step(1);
        ok &= check(true, "duplicate interaction does not crash", report);
    }

    // ── Falsification: participant leaves during countdown ────────────
    {
        dispatchKilled(nCt);  // remove from the alive set
        EntityRegistry::instance().destroy(nCt);
        step(200);
        ok &= check(true, "participant destruction during a round is safe", report);
    }

    // ── Generic (non-CS) actor spawn mechanism proof ──────────────────
    {
        const EntityId monster =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorHealthInit(Ecs::raw(monster), 50);
        actorStateWriteTeam(Ecs::raw(monster), 1);
        GameActorSpawnV1 req{};
        req.actorEntity = static_cast<std::uint64_t>(monster);
        req.position[0] = 7.0f; req.position[1] = 8.0f; req.position[2] = 9.0f;
        req.velocity[0] = 1.0f; req.velocity[1] = 2.0f; req.velocity[2] = 3.0f;
        req.yaw = 45.0f;
        req.health = 77;
        req.flags = 1u | 2u | 4u;
        const bool applied = serverSpawnOrResetActor(req);
        const auto* t = EntityRegistry::instance().tryGet<TransformComponent>(monster);
        const auto* v = EntityRegistry::instance().tryGet<VelocityComponent>(monster);
        ok &= check(applied && t && v && t->position.x == 7.0f &&
                        v->linear.z == 3.0f,
                    "generic actor.spawn works for a non-CS runtime actor", report);
    }

    runtime.deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    store.clear();
    return ok;
}
