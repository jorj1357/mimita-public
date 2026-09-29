// 09 29 2026
/* purpose
* Verify the pure local-gameplay readiness rule and lifecycle identity in
* src/network/local-gameplay-readiness.h: single-player is never gated, every
* required authoritative-spawn fact blocks simulation until true, and the
* lifecycle id changes on epoch/spawn-generation transitions.
* Does NOT test MultiplayerContext, Player, packets, physics, or the aim body.
*/

#include <cstdio>
#include <cstdint>

#include "network/local-gameplay-readiness.h"

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-58s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

using MimitaNet::LocalGameplayReadiness;
using MimitaNet::localGameplaySimulationReady;
using MimitaNet::localLifecycleId;

static LocalGameplayReadiness readyView()
{
    LocalGameplayReadiness r;
    r.networked = true;
    r.connected = true;
    r.mapReadyForPlayer = true;
    r.waitingForMapLoad = false;
    r.hasServerPosition = true;
    r.spawnTransformPending = false;
    r.hasSpawnGeneration = true;
    r.hasServerEpoch = true;
    r.outgoingEpochMatches = true;
    r.appliedEpochMatches = true;
    r.modelReady = true;
    return r;
}

static void testSinglePlayerNeverGated()
{
    TEST("single-player is never gated");
    LocalGameplayReadiness r;
    r.networked = false;
    CHECK(localGameplaySimulationReady(r), "offline view must be ready");
    PASS();
}

static void testAllFactsReady()
{
    TEST("all authoritative spawn facts ready");
    CHECK(localGameplaySimulationReady(readyView()), "expected ready");
    PASS();
}

static void testEachFactBlocks()
{
    struct Case { const char* name; void (*mutate)(LocalGameplayReadiness&); };
    const Case cases[] = {
        {"not connected blocks",            [](LocalGameplayReadiness& r){ r.connected = false; }},
        {"map not ready blocks",            [](LocalGameplayReadiness& r){ r.mapReadyForPlayer = false; }},
        {"map still loading blocks",        [](LocalGameplayReadiness& r){ r.waitingForMapLoad = true; }},
        {"no server position blocks",       [](LocalGameplayReadiness& r){ r.hasServerPosition = false; }},
        {"pending spawn transform blocks",  [](LocalGameplayReadiness& r){ r.spawnTransformPending = true; }},
        {"no spawn generation blocks",      [](LocalGameplayReadiness& r){ r.hasSpawnGeneration = false; }},
        {"no server epoch blocks",          [](LocalGameplayReadiness& r){ r.hasServerEpoch = false; }},
        {"outgoing epoch mismatch blocks",  [](LocalGameplayReadiness& r){ r.outgoingEpochMatches = false; }},
        {"applied epoch mismatch blocks",   [](LocalGameplayReadiness& r){ r.appliedEpochMatches = false; }},
        {"model not ready blocks",          [](LocalGameplayReadiness& r){ r.modelReady = false; }},
    };
    for (const Case& c : cases)
    {
        TEST(c.name);
        LocalGameplayReadiness r = readyView();
        c.mutate(r);
        CHECK(!localGameplaySimulationReady(r), "expected blocked: %s", c.name);
        PASS();
    }
}

static void testLifecycleIdentity()
{
    TEST("lifecycle id changes on epoch and spawn generation");
    const uint64_t base = localLifecycleId(1, 7);
    CHECK(base == localLifecycleId(1, 7), "same (epoch, gen) must be stable");
    CHECK(base != localLifecycleId(2, 7), "epoch change must change id");
    CHECK(base != localLifecycleId(1, 8), "spawn generation change must change id");
    // A teleport can reuse the spawn generation while advancing the epoch.
    CHECK(localLifecycleId(3, 7) != localLifecycleId(2, 7),
          "teleport epoch bump must change id");
    PASS();
}

int main()
{
    printf("local gameplay readiness / lifecycle identity\n");
    testSinglePlayerNeverGated();
    testAllFactsReady();
    testEachFactBlocks();
    testLifecycleIdentity();
    printf("\n%d passed, %d failed\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
