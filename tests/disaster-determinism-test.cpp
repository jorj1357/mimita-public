// Pure test: disaster selection, per-actor assignment, and timeout rules.
//
// Build (from repo root):
//   g++ -std=c++17 -O2 -Iinclude -Isrc tests/disaster-determinism-test.cpp \
//       src/gamemode/disaster-runtime.cpp src/gamemode/action-graph.cpp \
//       src/gamemode/capability-registry.cpp -o build/disaster-determinism-test.exe
//   build/disaster-determinism-test.exe

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "gamemode/action-graph.h"
#include "gamemode/capability-registry.h"
#include "gamemode/disaster-runtime.h"

using namespace MimitaGamemode;

static int gChecks = 0;
static int gFailures = 0;

static void check(bool condition, const char* what)
{
    ++gChecks;
    if (!condition) {
        ++gFailures;
        std::printf("  FAIL: %s\n", what);
    }
}

static ModePack makePack()
{
    ModePack pack;
    pack.id = "survive_disasters";
    pack.capabilities = {
        Cap::kLifecycleIntermissionCountdown,
        Cap::kParticipantsAllActors,
        Cap::kInventoryRandomPerActor,
        Cap::kWinLastActorAlive,
        Cap::kPresentationDisasterBanner,
    };
    for (int i = 0; i < 3; ++i) {
        DisasterDefinition d;
        d.id = "disaster_" + std::to_string(i);
        d.name = "Disaster " + std::to_string(i);
        d.durationSeconds = 60.0f + (float)i;
        d.weaponPool = {"revolver", "shotgun", "rocket_launcher", "spyknife"};
        d.winPolicy = "last_actor_alive";
        pack.disasters.push_back(d);
    }
    return pack;
}

int main()
{
    const ModePack pack = makePack();

    // ── Same manifest + seed => same disaster. ──────────────────────
    const DisasterDefinition* d1 = disasterSelect(pack, 20261004);
    const DisasterDefinition* d2 = disasterSelect(pack, 20261004);
    check(d1 && d2 && d1->id == d2->id, "same manifest+seed selects same disaster");

    // ── Reordering the declared disasters does not change selection. ─
    ModePack reordered = pack;
    std::reverse(reordered.disasters.begin(), reordered.disasters.end());
    const DisasterDefinition* d3 = disasterSelect(reordered, 20261004);
    check(d3 && d1 && d1->id == d3->id, "manifest reorder keeps selection stable");

    // ── Different seed can change selection (not required to, but the
    // selection is at least a function of the seed). ──────────────────
    check(disasterSelect(pack, 1) != nullptr, "selection returns a disaster for any seed");

    // ── Per-actor assignment determinism. ───────────────────────────
    DisasterState a;
    disasterConfigure(a, pack, 555);
    disasterBegin(a, {10, 20, 30, 40}, 100);
    check(a.weaponByActor.size() == 4, "every actor assigned a weapon");

    DisasterState b;
    disasterConfigure(b, pack, 555);
    disasterBegin(b, {10, 20, 30, 40}, 100);
    bool sameAssignment = true;
    for (uint32_t id : {10u, 20u, 30u, 40u})
        sameAssignment &= *disasterWeaponForActor(a, id) == *disasterWeaponForActor(b, id);
    check(sameAssignment, "same seed gives identical per-actor weapons");

    // ── Actor ordering does not change an actor's weapon. ───────────
    DisasterState c;
    disasterConfigure(c, pack, 555);
    disasterBegin(c, {40, 30, 20, 10}, 100);
    bool stableByActor = true;
    for (uint32_t id : {10u, 20u, 30u, 40u})
        stableByActor &= *disasterWeaponForActor(a, id) == *disasterWeaponForActor(c, id);
    check(stableByActor, "actor order does not change assignment");

    // ── Assigned weapons come from the declared pool. ───────────────
    bool inPool = true;
    for (const auto& kv : a.weaponByActor)
        inPool &= std::find(a.weaponPool.begin(), a.weaponPool.end(), kv.second) != a.weaponPool.end();
    check(inPool, "assigned weapons come from the pool");

    // ── Duration boundary. ──────────────────────────────────────────
    check(!disasterDurationElapsed(a, 99), "duration not elapsed before start");
    check(!disasterDurationElapsed(a, 100 + a.durationTicks - 1),
          "duration not elapsed one tick early");
    check(disasterDurationElapsed(a, 100 + a.durationTicks),
          "duration elapsed exactly at boundary");

    // ── Timeout winner is order-independent and deterministic. ──────
    const uint32_t w1 = disasterPickTimeoutWinner(555, {10, 20, 30});
    const uint32_t w2 = disasterPickTimeoutWinner(555, {30, 10, 20});
    check(w1 == w2 && w1 != 0, "timeout winner order-independent");
    check(disasterPickTimeoutWinner(555, {}) == 0, "no survivors => no winner");

    // ── Action graph schedules the declared capabilities. ───────────
    const ActionGraph graph = ActionGraph::build(pack);
    check(graph.hasCapability(Cap::kInventoryRandomPerActor),
          "graph resolves random per-actor inventory");
    check(graph.hasCapability(Cap::kWinLastActorAlive),
          "graph resolves last-actor-alive win");
    const auto enter = graph.onPhaseEnter((uint8_t)ActionPhase::Intermission);
    check(std::find(enter.begin(), enter.end(), Cap::kLifecycleIntermissionCountdown) != enter.end(),
          "intermission countdown is a phase-enter action");
    // Interval action fires on the phase-start tick and every tick after for a
    // 1-tick interval.
    const auto due = graph.dueAt((uint8_t)ActionPhase::Active, 500, 500);
    check(std::find(due.begin(), due.end(), Cap::kWinLastActorAlive) != due.end(),
          "win check is due at the active phase start tick");

    std::printf("disaster-determinism-test: %d checks, %d failures\n", gChecks, gFailures);
    std::printf("%s\n", gFailures == 0 ? "PASS" : "FAIL");
    return gFailures == 0 ? 0 : 1;
}
