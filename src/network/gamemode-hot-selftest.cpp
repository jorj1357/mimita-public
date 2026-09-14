// 09 14 2026
/* purpose
* Implements the headless hot-gamemode self-test. Asserts invariants and
* data-driven routing; never pins tuned gameplay constants.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#include "network/gamemode-hot-selftest.h"

#include <cstring>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/packets.h"
#include "network/server-gamemode.h"
#include "network/match-lifecycle.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const std::uint64_t kFfaMode = gameHash("ffa");
const std::uint64_t kHotTestMode = gameHash("gamemode.hot-test");
const std::uint64_t kFfaState = gameHash("FfaMatchState");
const std::uint64_t kHotTestState = gameHash("HotTestMatchState");

struct FfaEntry {
    std::uint32_t playerId;
    std::int32_t score;
    std::int32_t deaths;
};
struct FfaMatchState {
    std::int32_t scoreLimit;
    std::int32_t count;
    FfaEntry entries[32];
};

struct HotTestMatchState {
    std::int32_t scoreLimit;
    std::int32_t score;
    float weirdMultiplier;
};

struct ProbeV1 {
    std::int32_t a;
};
struct ProbeV2 {
    std::int32_t a;
    std::int32_t b;
};

bool migrateProbeV1toV2(const void* oldState, std::size_t oldSize, void* newState,
                        std::size_t newSize)
{
    if (oldSize < sizeof(ProbeV1) || newSize < sizeof(ProbeV2))
        return false;
    ProbeV1 old{};
    std::memcpy(&old, oldState, sizeof(ProbeV1));
    ProbeV2 out{};
    out.a = old.a;
    out.b = 7;
    std::memcpy(newState, &out, sizeof(ProbeV2));
    return true;
}

bool failProbe(const void*, std::size_t, void*, std::size_t)
{
    return false;
}

} // namespace

bool runGamemodeHotSelfTest(std::string& report)
{
    bool ok = true;

    EntityRegistry::instance().destroyAll();
    DynamicComponentStore::instance().clear();

    // Load the real replaceable package so its runtime modes register.
    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);
    ok &= check(runtime.hasMode(kFfaMode), "FFA mode registered at runtime", report);
    ok &= check(runtime.hasMode(kHotTestMode),
                "brand-new hot-test mode registered at runtime", report);
    ok &= check(runtime.modeCount() >= 2, "runtime mode registry enumerable", report);
    ok &= check(runtime.modeDomain(kFfaMode) != 0 &&
                    runtime.modeDomain(kFfaMode) != runtime.modeDomain(kHotTestMode),
                "modes route to distinct domains", report);

    // Kernel-owned match entity + enabled match.
    MimitaNet::ServerGamemodeState& d = MimitaNet::serverGamemodeState();
    d.enabled = true;
    d.matchMode = "ffa";
    d.phase = MimitaNet::DUEL_PHASE_ACTIVE;
    d.resultsSeconds = 5.0f;
    MimitaNet::serverMatchResetEntity();
    const std::uint64_t match = MimitaNet::serverMatchEntity();
    ok &= check(match != 0 && runtime.sharedState() &&
                    runtime.sharedState()->matchEntity == match,
                "kernel match entity published to generic shared state", report);

    // ── FFA active: kills score hot and are handled ──────────────────
    runtime.setActiveModeDomain(runtime.modeDomain(kFfaMode));
    {
        GameActorKilledV1 killed{};
        killed.killerId = 7;
        killed.victimId = 8;
        killed.tick = 1;
        const bool handled = LiveBehavior::dispatchActorKilled(killed, 1);
        ok &= check(handled && killed.handled == 1,
                    "active FFA handler owns actor.killed", report);

        FfaMatchState state{};
        const bool has = DynamicComponentStore::instance().read(match, kFfaState,
                                                                &state, sizeof(state));
        ok &= check(has && state.entries[0].playerId == 7 &&
                        state.entries[0].score == 1 &&
                        state.entries[1].deaths == 1,
                    "hot FFA scoring wrote package dynamic state", report);
    }

    // Score snapshot bridge sees the package state.
    {
        void* provider = runtime.capability(gameHash("match.score.snapshot"));
        ok &= check(provider != nullptr, "mode score-snapshot capability resolved", report);
        if (provider) {
            GameMatchScoreSnapshotV1 snapshot{};
            reinterpret_cast<GameMatchScoreSnapshotFn>(provider)(
                LiveBehavior::hostContext(2), &snapshot);
            ok &= check(snapshot.count == 2 && snapshot.entries[0].ownerId == 7 &&
                            snapshot.entries[0].score == 1,
                        "generic score snapshot mirrors package state", report);
        }
    }

    // ── Cross-mode isolation: FFA handler must not run for hot-test ──
    {
        runtime.setActiveModeDomain(runtime.modeDomain(kHotTestMode));
        const std::uint32_t before = 0;
        FfaMatchState beforeState{};
        DynamicComponentStore::instance().read(match, kFfaState, &beforeState,
                                               sizeof(beforeState));
        const std::int32_t scoreBefore = beforeState.entries[0].score;
        (void)before;

        GameActorKilledV1 killed{};
        killed.killerId = 9;
        killed.victimId = 10;
        killed.tick = 3;
        const bool handled = LiveBehavior::dispatchActorKilled(killed, 3);
        ok &= check(handled, "active hot-test handler owns actor.killed", report);

        FfaMatchState afterState{};
        DynamicComponentStore::instance().read(match, kFfaState, &afterState,
                                               sizeof(afterState));
        ok &= check(afterState.entries[0].score == scoreBefore,
                    "inactive mode handler did not run (domain filtering)", report);

        HotTestMatchState ht{};
        const bool hasHt = DynamicComponentStore::instance().read(
            match, kHotTestState, &ht, sizeof(ht));
        ok &= check(hasHt && ht.score >= 2,
                    "hot-test mode used its own weighted scoring rule", report);
    }

    // ── match.evaluate is owned by the active mode ───────────────────
    {
        GameMatchEvaluateV1 evaluate{};
        evaluate.matchEntity = match;
        evaluate.phase = MimitaNet::DUEL_PHASE_ACTIVE;
        const bool handled = LiveBehavior::dispatchMatchEvaluate(evaluate, 4);
        ok &= check(handled && evaluate.handled == 1,
                    "active mode owns match.evaluate", report);
    }

    // ── Mode schema migration (activation-time) ─────────────────────
    {
        const std::uint64_t probeId = gameHash("GmProbeMatchState");
        DynamicComponentSchema v1;
        v1.typeId = probeId;
        v1.schemaHash = gameHash("GmProbeMatchState.v1");
        v1.version = 1;
        v1.size = sizeof(ProbeV1);
        v1.align = 4;
        std::string error;
        ok &= check(DynamicComponentStore::instance().applySchemaUpdate({v1}, error),
                    "mode schema v1 applied", report);
        ProbeV1 p{41};
        ok &= check(DynamicComponentStore::instance().write(match, probeId, &p,
                                                            sizeof(p)),
                    "mode state written", report);

        DynamicComponentSchema v2 = v1;
        v2.version = 2;
        v2.size = sizeof(ProbeV2);
        v2.schemaHash = gameHash("GmProbeMatchState.v2");
        DynamicComponentStore::instance().registerMigration(
            probeId, 1, 2, &migrateProbeV1toV2);
        error.clear();
        ok &= check(DynamicComponentStore::instance().applySchemaUpdate({v2}, error),
                    "mode schema v2 migration applied", report);
        ProbeV2 migrated{};
        ok &= check(DynamicComponentStore::instance().read(match, probeId, &migrated,
                                                           sizeof(migrated)) &&
                        migrated.a == 41 && migrated.b == 7,
                    "mode state survived schema migration", report);

        // A failing migration rejects the update and keeps last-good state.
        DynamicComponentSchema v3 = v2;
        v3.version = 3;
        v3.size = 12;
        v3.schemaHash = gameHash("GmProbeMatchState.v3");
        DynamicComponentStore::instance().registerMigration(probeId, 2, 3,
                                                            &failProbe);
        error.clear();
        const bool applied3 = DynamicComponentStore::instance().applySchemaUpdate({v3},
                                                                                  error);
        ProbeV2 after{};
        DynamicComponentStore::instance().read(match, probeId, &after, sizeof(after));
        ok &= check(!applied3 && after.a == 41 && after.b == 7,
                    "failed mode migration keeps last-good state", report);
    }

    // ── Real FFA win condition is hot ────────────────────────────────
    {
        runtime.setActiveModeDomain(runtime.modeDomain(kFfaMode));
        // Drive scoring to the default limit, then the mode ends the match.
        FfaMatchState state{};
        DynamicComponentStore::instance().read(match, kFfaState, &state, sizeof(state));
        const std::int32_t limit = state.scoreLimit > 0 ? state.scoreLimit : 20;
        for (int i = 0; i < 64 && state.entries[0].score < limit; ++i) {
            GameActorKilledV1 killed{};
            killed.killerId = 7;
            killed.victimId = 8;
            LiveBehavior::dispatchActorKilled(killed, 10);
            DynamicComponentStore::instance().read(match, kFfaState, &state,
                                                   sizeof(state));
        }
        ok &= check(state.entries[0].score >= limit,
                    "hot FFA scoring reached the score limit", report);
        ok &= check(d.matchOver && d.phase == MimitaNet::DUEL_PHASE_RESULTS,
                    "hot FFA win condition ended the match via match.finish", report);
    }

    // ── Generic match lifecycle policy is owned by the active mode ───
    {
        runtime.setActiveModeDomain(runtime.modeDomain(kFfaMode));
        GameMatchLifecycleV1 policy{};
        policy.matchEntity = match;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MATCH_LIFECYCLE, &policy,
                                              sizeof(policy), 20, match, 0);
        ok &= check(policy.handled == 1 && policy.outIntermissionSeconds == 10.0f &&
                        policy.outRespawnSeconds == 2.5f,
                    "active FFA owns match.lifecycle policy", report);

        runtime.setActiveModeDomain(runtime.modeDomain(kHotTestMode));
        GameMatchLifecycleV1 policy2{};
        policy2.matchEntity = match;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MATCH_LIFECYCLE, &policy2,
                                              sizeof(policy2), 21, match, 0);
        ok &= check(policy2.handled == 0,
                    "inactive mode does not own match.lifecycle", report);

        const std::uint64_t kTdmMode = gameHash("tdm");
        ok &= check(runtime.hasMode(kTdmMode),
                    "TDM mode registered at runtime", report);
        runtime.setActiveModeDomain(runtime.modeDomain(kTdmMode));
        GameMatchLifecycleV1 policy3{};
        policy3.matchEntity = match;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MATCH_LIFECYCLE, &policy3,
                                              sizeof(policy3), 22, match, 0);
        ok &= check(policy3.handled == 1 && policy3.outRespawnSeconds == 5.0f,
                    "TDM owns match.lifecycle through the same path", report);
    }

    GenericRuntime::instance().deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
