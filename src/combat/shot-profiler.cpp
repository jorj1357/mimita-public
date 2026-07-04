#include "shot-profiler.h"
#include <cstdio>

ShotProfiler* gShotProfiler = nullptr;

static constexpr double PRINT_THRESHOLD = 0.15;

static void printLine(const char* label, double ms, int count = 0, bool indent = true)
{
    if (ms < PRINT_THRESHOLD) return;
    printf("%s%-30s %7.2f ms", indent ? "    " : "", label, ms);
    if (count > 0) printf("  (%d calls)", count);
    printf("\n");
}

void ShotProfiler::print() const
{
    // Find top 3 stages
    struct Stage { const char* name; double ms; };
    Stage stages[] = {
        {"Collision", worldCollisionMs + npcCollisionMs + remotePlayerCollisionMs},
        {"HitFX", impactSphereMs + bloodMs + debrisMs + damageNumberMs + hitBurstMs},
        {"Replay Recording", replayRecordMs},
        {"Effect Spawn", effectSpawnMs},
        {"Tracers", tracerMs},
        {"Damage", damageMs},
        {"Audio", audioMs},
        {"Aim", aimMs},
        {"Pellet Generation", pelletGenMs},
    };

    // Sort by ms descending, keep top 3
    Stage top3[3] = {};
    int topCount = 0;
    for (const auto& s : stages) {
        if (s.ms < PRINT_THRESHOLD) continue;
        int insertAt = topCount;
        for (int i = 0; i < topCount; ++i) {
            if (s.ms > top3[i].ms) { insertAt = i; break; }
        }
        if (insertAt < 3) {
            if (topCount < 3) topCount++;
            for (int i = topCount - 1; i > insertAt; --i)
                top3[i] = top3[i - 1];
            top3[insertAt] = s;
        }
    }

    double total = 0.0;
    total += aimMs + pelletGenMs + pelletLoopMs + damageMs + audioMs + animationMs;
    total += worldCollisionMs + npcCollisionMs + remotePlayerCollisionMs;
    total += impactSphereMs + bloodMs + debrisMs + damageNumberMs + hitBurstMs;
    total += effectSpawnMs + replayRecordMs + tracerMs;

    printf("\n==================================================\n");
    printf("SHOT PROFILE\n");
    printf("==================================================\n");
    printf("Weapon: %s  Frame: %d\n", weaponName.c_str(), frameTick);
    printf("Total: %.2f ms  Pellets: %d  Hits (NPC/World): %d/%d  Misses: %d\n",
           total, totalPellets, npcHits, worldHits, misses);
    printf("\n");

    // Always show collision if it was significant
    if (worldCollisionMs + npcCollisionMs + remotePlayerCollisionMs > PRINT_THRESHOLD || total > 2.0) {
        printf("  Collision          %7.2f ms  (%d calls)\n",
               worldCollisionMs + npcCollisionMs + remotePlayerCollisionMs, collisionCalls);
        printLine("World Collision", worldCollisionMs, worldTriangleTests, true);
        printLine("NPC Collision", npcCollisionMs, npcBodyTests, true);
    }

    // Always show HitFX breakdown
    double hitFxTotal = impactSphereMs + bloodMs + debrisMs + damageNumberMs + hitBurstMs;
    if (hitFxTotal > PRINT_THRESHOLD || total > 2.0) {
        printf("  HitFX              %7.2f ms  (%d calls, %d objects)\n",
               hitFxTotal, hitFxCalls, objectsSpawnedByHitFx);
        printLine("Impact Sphere", impactSphereMs, 0, true);
        printLine("Blood FX", bloodMs, 0, true);
        printLine("Debris FX", debrisMs, 0, true);
        printLine("Damage Number", damageNumberMs, 0, true);
        printLine("Hit Burst", hitBurstMs, 0, true);
    }

    if (replayRecordMs > PRINT_THRESHOLD || total > 2.0) {
        printf("  Replay Recording   %7.2f ms  (%d events, %d vector grows)\n",
               replayRecordMs, replayEventsCreated, replayVectorGrows);
    }

    if (effectSpawnMs > PRINT_THRESHOLD || total > 2.0) {
        printf("  Effect Spawn       %7.2f ms  (%d spawned, %d pool scans, %d pool hits)\n",
               effectSpawnMs, effectsSpawned, poolLinearScans, poolHits);
    }

    printLine("Tracers", tracerMs, tracersSpawned);
    printLine("Damage", damageMs);
    printLine("Audio", audioMs);
    printLine("Aim", aimMs);
    printLine("Pellet Generation", pelletGenMs);

    // Memory
    if (totalMallocs > 0 || vectorReallocs > 0) {
        printf("\n  Memory\n");
        printf("    %-28s %7d\n", "mallocs", totalMallocs);
        printf("    %-28s %7d\n", "frees", totalFrees);
        printf("    %-28s %7d\n", "bytes allocated", totalBytes);
        printf("    %-28s %7d\n", "largest alloc (bytes)", largestAllocBytes);
        printf("    %-28s %7d\n", "vector reallocs", vectorReallocs);
        printf("    %-28s %7d\n", "string allocs", stringAllocs);
    }

    // Bottleneck analysis
    if (topCount > 0) {
        printf("\n");
        printf("--------------------------------------------------\n");
        printf("LARGEST STAGE:\n");
        for (int i = 0; i < topCount; ++i) {
            double pct = total > 0.0 ? (top3[i].ms / total) * 100.0 : 0.0;
            printf("  #%d: %-25s %.2f ms (%.0f%%)\n", i + 1, top3[i].name, top3[i].ms, pct);
        }
        printf("\n");

        const char* bottleneck = top3[0].name;
        printf("LIKELY BOTTLENECK: %s\n", bottleneck);
        printf("\n");
        printf("because %s consumed %.0f%% of total shot time.\n", 
               bottleneck, total > 0.0 ? (top3[0].ms / total) * 100.0 : 0.0);
        printf("\n");

        // Recommendations
        bool recommended = false;
        auto rec = [&](const char* r) {
            printf("Recommended optimization: %s\n", r);
            recommended = true;
        };
        if (std::string(bottleneck) == "HitFX") {
            rec("Pool EffectParts / batch blood decals");
        } else if (std::string(bottleneck) == "Replay Recording") {
            rec("Batch replay effects into fewer events");
        } else if (std::string(bottleneck) == "Collision") {
            rec("Share broadphase results across nearby pellets");
        } else if (std::string(bottleneck) == "Effect Spawn") {
            rec("Use freelist for pool slots instead of linear scan");
        } else if (std::string(bottleneck) == "Tracers") {
            rec("Batch tracers into a single replay event");
        } else if (!recommended) {
            rec("Investigate stage for allocation/loop optimization");
        }
    }

    printf("==================================================\n");
    fflush(stdout);
}
