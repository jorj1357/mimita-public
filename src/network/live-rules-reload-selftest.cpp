// 09 23 2026
/* purpose
* Proves the hot-reload guarantee for match/gamemode rules: a hot generation
* swap (unload + reload of the module DLL) mid-match does not reset the match.
* Phase, scores, round, participants, and generic match state survive; the
* reloaded mode still owns the lifecycle. Mirrors the "THE LAWS CHANGED / THE
* UNIVERSE DID NOT RESTART" invariant for gamemode policy.
* Does NOT own transport or the live server loop.
*/
#include "network/live-rules-reload-selftest.h"

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

bool runLiveRulesReloadSelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);

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

    std::vector<EntityId> actors;
    for (int i = 0; i < 4; ++i) {
        const EntityId e = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        actorHealthInit(static_cast<std::uint64_t>(e), 100);
        actors.push_back(e);
    }

    // Reach an active match with a non-zero score and round.
    runTicks(runtime, tdmDomain, 200, 0.05f, 1);
    ok &= check(d.phase == DUEL_PHASE_ACTIVE, "match reached active", report);

    std::vector<EntityId> ordered = actors;
    std::sort(ordered.begin(), ordered.end());
    const std::uint32_t killerId = entityLegacyId(ordered[0]);
    const std::uint32_t victimId = entityLegacyId(ordered[1]);
    for (int i = 0; i < 3; ++i) {
        GameActorKilledV1 killed{};
        killed.killerId = killerId;
        killed.victimId = victimId;
        killed.tick = static_cast<std::uint32_t>(i);
        LiveBehavior::dispatchActorKilled(killed, static_cast<std::uint64_t>(i));
    }

    TdmMatchStateMirror before{};
    readTdm(match, before);
    const std::uint32_t phaseBefore = d.phase;
    const std::uint32_t roundBefore = before.round;
    const std::int32_t redBefore = before.redScore;
    const std::int32_t blueBefore = before.blueScore;
    const std::vector<std::uint32_t> participantsBefore = d.participants;
    ok &= check((redBefore + blueBefore) > 0, "match has a live score before reload",
                report);

    // ── Live generation swap: unload + reload the module DLL ──────────
    HotReloadSystem::instance().unloadGameDLL();
    HotReloadSystem::instance().startup();
    ok &= check(runtime.active(), "hot package re-activated after reload", report);

    // The reloaded mode re-registers and re-claims the same domain/mode.
    const std::uint64_t domainAfter = runtime.modeDomain(gameHash("tdm"));
    runtime.setActiveModeDomain(domainAfter);
    ok &= check(domainAfter != 0 && runtime.hasMode(gameHash("tdm")),
                "reloaded generation re-registered the TDM mode", report);

    // ── The match state survived the swap ─────────────────────────────
    TdmMatchStateMirror after{};
    readTdm(match, after);
    ok &= check(store.has(static_cast<EntityId>(match), gameHash("MatchPhaseOwnership")),
                "phase ownership survived the reload", report);
    ok &= check(after.round == roundBefore && after.redScore == redBefore &&
                    after.blueScore == blueBefore && d.phase == phaseBefore,
                "phase/scores/round unchanged across the reload", report);
    ok &= check(d.participants == participantsBefore,
                "participants unchanged across the reload", report);

    // ── The reloaded rules still drive the match forward ──────────────
    runTicks(runtime, domainAfter, 60, 0.05f, 3000);
    ok &= check(d.phase == phaseBefore ||
                    d.phase == DUEL_PHASE_ACTIVE || d.phase == DUEL_PHASE_GO,
                "reloaded rules continue the match (no reset to waiting)", report);

    runtime.deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    store.clear();
    return ok;
}
