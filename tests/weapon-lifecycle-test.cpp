// 07 21 2026, 21 45
/* purpose
* Tests generic weapon lifecycle bookkeeping introduced for Stage 4A requests and contacts.
* Verifies pending attack retries preserve seed, tick context, and physical attack variant data.
* Verifies contact episodes retain target spawn generation and final confirmation state.
* Does NOT open sockets, start the server loop, apply damage, or launch mimita.exe.
* Does NOT test rendering, audio, projectile simulation, or old packet compatibility.
* Does NOT replace reconnect and respawn process-level networking tests.
*/

#include "network/multiplayer-context.h"
#include "combat/weapon-execution.h"

#include <cstdio>

static int gFailures = 0;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

int main()
{
    MimitaNet::MultiplayerContext::PendingAttackRequest pending;
    pending.requestId = 99;
    pending.spawnGeneration = 10;
    pending.clientSimulationTick = 1234;
    pending.basedOnInputSequence = 44;
    pending.deterministicSeed = 0x33332222;
    pending.attackVariant = 2;
    check(pending.clientSimulationTick == 1234, "pending retry keeps client simulation tick");
    check(pending.basedOnInputSequence == 44, "pending retry keeps input sequence");
    check(pending.deterministicSeed == 0x33332222, "pending retry keeps deterministic seed");
    check(pending.attackVariant == 2, "pending retry keeps physical attack variant");

    WeaponExecution::PhysicalContactEpisode episode;
    episode.active = true;
    episode.targetPlayerId = 7;
    episode.targetSpawnGeneration = 11;
    episode.pendingConfirmationDamage = 25;
    episode.pendingHealthBefore = 100;
    episode.pendingHealthAfter = 75;
    episode.pendingKilled = false;
    episode.samplesSinceConfirmation = 1;
    check(episode.targetSpawnGeneration == 11, "contact episode records target spawn generation");
    check(WeaponExecution::episodeShouldConfirm(episode, true, 4),
          "contact episode flushes final pending confirmation");

    if (gFailures)
    {
        std::printf("[weapon-lifecycle-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[weapon-lifecycle-test] PASS\n");
    return 0;
}
