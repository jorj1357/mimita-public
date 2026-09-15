// 09 14 2026
/* purpose
* Implements the headless generic match-policy self-test: hot participant
* team/role assignment, full phase lifecycle, score/time limits, win, and
* respawn-policy ownership by a runtime mode.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/match-policy-selftest.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/actor-health.h"
#include "network/match-lifecycle.h"
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

struct TdmMatchStateMirror {
    std::uint32_t phase;
    float timer;
    std::int32_t scoreLimit;
    std::int32_t redScore;
    std::int32_t blueScore;
    std::uint32_t round;
    std::uint32_t timeLimitSeconds;
    std::uint64_t roundStartTick;
};

struct ActorTeamStateMirror {
    std::int32_t team;
    std::uint32_t reserved;
};

bool readTdm(std::uint64_t match, TdmMatchStateMirror& out)
{
    return DynamicComponentStore::instance().read(
        static_cast<EntityId>(match), gameHash("TdmMatchState"), &out, sizeof(out));
}

void runTicks(GenericRuntime& runtime, std::uint64_t domain, int count, float dt,
              std::uint64_t baseTick)
{
    for (int i = 0; i < count; ++i)
        runtime.runDomain(domain, baseTick + (std::uint64_t)i, dt,
                          LiveBehavior::hostContext(baseTick + (std::uint64_t)i));
}

} // namespace

bool runMatchPolicySelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);
    ok &= check(runtime.hasMode(gameHash("tdm")),
                "runtime TDM mode registered (no EXE enum)", report);

    ServerGamemodeState& d = serverGamemodeState();
    d.enabled = true;
    d.matchOver = false;
    serverMatchResetEntity();
    const std::uint64_t match = serverMatchEntity();
    ok &= check(match != 0, "match entity available", report);

    const std::uint64_t tdmDomain = runtime.modeDomain(gameHash("tdm"));
    runtime.setActiveModeDomain(tdmDomain);
    d.phase = DUEL_PHASE_WAITING;
    d.participants = {1u, 2u, 3u, 4u};

    // Create generic actor entities (participants) with authoritative health.
    std::vector<EntityId> actors;
    for (int i = 0; i < 4; ++i) {
        const EntityId e = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorHealthInit(static_cast<std::uint64_t>(e), 100);
        actors.push_back(e);
    }

    // Countdown -> active; the hot mode assigns teams/roles on first tick.
    runTicks(runtime, tdmDomain, 200, 0.05f, 1);
    ok &= check(d.phase == DUEL_PHASE_ACTIVE,
                "hot mode transitioned countdown -> active", report);
    ok &= check(store.has(static_cast<EntityId>(match), gameHash("MatchPhaseOwnership")),
                "hot mode claimed generic phase ownership", report);

    // Hot team/role assignment written as generic components.
    bool teamsValid = true;
    bool rolesValid = true;
    for (EntityId e : actors) {
        ActorTeamStateMirror t{};
        if (!store.read(e, gameHash("ActorTeamState"), &t, sizeof(t)) ||
            (t.team != 0 && t.team != 1))
            teamsValid = false;
        if (!store.has(e, gameHash("ActorRoleState")))
            rolesValid = false;
    }
    ok &= check(teamsValid, "hot TDM assigned teams via ActorTeamState", report);
    ok &= check(rolesValid, "hot TDM assigned roles via ActorRoleState", report);

    // Deterministic assignment: sorted entity ids alternate 0/1.
    {
        std::vector<EntityId> sorted = actors;
        std::sort(sorted.begin(), sorted.end());
        bool alternates = true;
        for (std::size_t i = 0; i < sorted.size(); ++i) {
            ActorTeamStateMirror t{};
            store.read(sorted[i], gameHash("ActorTeamState"), &t, sizeof(t));
            if (t.team != static_cast<std::int32_t>(i % 2))
                alternates = false;
        }
        ok &= check(alternates, "hot team assignment is deterministic", report);
    }

    // Score limit finishes the match. The killer's team is read from the generic
    // ActorTeamState (source of truth) through the match capability; the typed
    // matchTeams map is only a projection.
    std::vector<EntityId> ordered = actors;
    std::sort(ordered.begin(), ordered.end());
    const std::uint32_t killerId = entityLegacyId(ordered[0]);   // team 0
    const std::uint32_t victimId = entityLegacyId(ordered[1]);   // team 1
    for (int i = 0; i < 40 && !d.matchOver; ++i) {
        GameActorKilledV1 killed{};
        killed.killerId = killerId;
        killed.victimId = victimId;
        killed.tick = static_cast<std::uint32_t>(i);
        LiveBehavior::dispatchActorKilled(killed, static_cast<std::uint64_t>(i));
    }
    ok &= check(d.matchOver, "hot TDM score limit finished the match", report);
    TdmMatchStateMirror state{};
    readTdm(match, state);
    ok &= check(state.phase == 6 /*results*/,
                "hot TDM entered results phase after finish", report);

    // Results -> intermission.
    runTicks(runtime, tdmDomain, 300, 0.05f, 1000);
    ok &= check(d.phase == DUEL_PHASE_INTERMISSION,
                "hot TDM transitioned results -> intermission", report);

    // Intermission -> next round (round incremented, scores reset, lifecycle
    // continued into countdown/active).
    runTicks(runtime, tdmDomain, 400, 0.05f, 2000);
    readTdm(match, state);
    ok &= check(state.round == 2 && state.redScore == 0 && state.blueScore == 0 &&
                    (d.phase == DUEL_PHASE_COUNTDOWN ||
                     d.phase == DUEL_PHASE_ACTIVE || d.phase == DUEL_PHASE_GO),
                "hot TDM transitioned intermission -> next round", report);

    // Time limit finishes the match (victoryType 1) even with no score limit.
    {
        d.matchOver = false;
        TdmMatchStateMirror s{};
        readTdm(match, s);
        s.phase = 2;  // active
        s.timeLimitSeconds = 1;
        s.roundStartTick = 5000;
        store.write(static_cast<EntityId>(match), gameHash("TdmMatchState"), &s,
                    sizeof(s));
        runtime.runDomain(tdmDomain, 5000 + 61, 0.05f,
                          LiveBehavior::hostContext(5000 + 61));
        ok &= check(d.matchOver && d.victoryType == 1,
                    "hot TDM time limit finished the match", report);
    }

    // Respawn policy owned by the mode.
    {
        GameMatchLifecycleV1 policy{};
        policy.matchEntity = match;
        policy.respawnSeconds = d.respawnSeconds;
        policy.respawnsEnabled = serverMatchRespawnsEnabled() ? 1u : 0u;
        const bool handled = LiveBehavior::dispatchGameplayEvent64(
            GAME_EVENT_MATCH_LIFECYCLE, &policy, sizeof(policy), 1, match, 0);
        ok &= check(handled && policy.outRespawnsEnabled == 1u &&
                        policy.outRespawnSeconds > 0.0f,
                    "hot mode owns the respawn policy decision", report);
    }

    runtime.deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    store.clear();
    return ok;
}
