#include "perf/perf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <fstream>
#include <filesystem>

#include "video/frame-pacer.h"
#include "gui/ui-system.h"
#include "debug/debug-log.h"

extern FramePacer gFramePacer;

PerfState gState;

static uint64_t nowUs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count();
}

PerfState& Perf::state() { return gState; }

void Perf::beginFrame()
{
    gState.allocationsThisFrame = 0;
    gState.current = PerfTimes{};
    gState.npcProfileCount = 0;
}

static void sortContributors(FrameSpikeReport& report)
{
    for (int i = 0; i < report.contributorCount - 1; ++i) {
        for (int j = 0; j < report.contributorCount - 1 - i; ++j) {
            if (report.contributors[j].ms < report.contributors[j + 1].ms) {
                auto tmp = report.contributors[j];
                report.contributors[j] = report.contributors[j + 1];
                report.contributors[j + 1] = tmp;
            }
        }
    }
}

void Perf::writeSpikeReport(const FrameSpikeReport& report)
{
    std::ofstream f("logs/performance_profile.txt", std::ios::app);
    if (!f.is_open()) return;

    f << "\n=====================\n";
    f << "FRAME SPIKE\n";
    f << "=====================\n";
    f << "Frame: " << report.frameNumber << "\n";
    f << "Total: " << report.totalFrameMs << "ms\n";
    f << "Average: " << report.avgFrameMs << "ms\n";
    f << "Largest contributors:\n";
    for (int i = 0; i < report.contributorCount; ++i) {
        f << "  " << report.contributors[i].name << "  "
          << report.contributors[i].ms << "ms\n";
    }
    f << "NPC count: " << report.npcCount << "\n";
    f << "Player count: " << report.playerCount << "\n";
    f << "Effects alive: " << report.effectsAlive << "\n";
    f << "Damage numbers alive: " << report.damageNumbersAlive << "\n";
    f << "Particle count: " << report.particleCount << "\n";
    f << "Draw calls: " << report.drawCalls << "\n";
    f << "Visible meshes: " << report.visibleMeshes << "\n";
    f << "Visible triangles: " << report.visibleTriangles << "\n";
    f << "Chunk cells visited: " << report.chunkCellsVisited << "\n";
    f << "Unique triangles: " << report.uniqueTriangles << "\n";
    f.close();

    printf("\n=====================\n");
    printf("FRAME SPIKE\n");
    printf("=====================\n");
    printf("Frame: %d\n", report.frameNumber);
    printf("Total: %.1fms\n", report.totalFrameMs);
    printf("Largest contributors:\n");
    for (int i = 0; i < report.contributorCount && i < 5; ++i) {
        printf("  %s: %.1fms\n", report.contributors[i].name, report.contributors[i].ms);
    }
}

void Perf::writeProfileToFile()
{
    PerfState& s = gState;
    const PerfTimes& t = s.current;

    static uint64_t lastWriteUs = 0;
    uint64_t now = nowUs();
    if (now - lastWriteUs < 1000000) return;
    lastWriteUs = now;

    std::filesystem::create_directories("logs");
    std::ofstream f("logs/performance_profile.txt", std::ios::trunc);
    if (!f.is_open()) return;

    float totalMs = gFramePacer.frameTimeMs();
    f << "Frame: " << s.frameNumber << "\n";
    f << "Total: " << totalMs << "ms\n\n";

    struct PerfEntry { const char* name; double ms; };
    PerfEntry entries[] = {
        {"Input", t.input},
        {"Setup", t.setup},
        {"Audio", t.audio},
        {"State", t.state},
        {"Replay", t.replay},
        {"Networking", t.networking},
        {"Camera", t.camera},
        {"Combat", t.combat},
        {"UI", t.ui},
        {"Physics", t.physics},
        {"Collision", t.collision},
        {"Movement", t.movement},
        {"SweepSlide", t.sweepSlide},
        {"Depenetration", t.depenetration},
        {"GroundDetection", t.groundDetection},
        {"ChunkQuery", t.chunkQuery},
        {"Broadphase", t.broadphase},
        {"Narrowphase", t.narrowphase},
        {"TriangleTests", t.triangleTests},
        {"CharVsWorld", t.charVsWorld},
        {"CharVsChar", t.charVsChar},
        {"WeaponCollisions", t.weaponCollisions},
        {"NpcUpdate", t.npcUpdate},
        {"NpcThink", t.npcThink},
        {"NpcPathfinding", t.npcPathfinding},
        {"NpcTargetAcq", t.npcTargetAcq},
        {"NpcVision", t.npcVision},
        {"NpcMovement", t.npcMovement},
        {"NpcWeapon", t.npcWeapon},
        {"NpcCombat", t.npcCombat},
        {"NpcCollision", t.npcCollision},
        {"NpcAnimation", t.npcAnimation},
        {"NpcRender", t.npcRender},
        {"NpcSpawn", t.npcSpawn},
        {"NpcAi", t.npcAi},
        {"Weapons", t.weapons},
        {"Projectiles", t.projectiles},
        {"Particles", t.particles},
        {"DamageNumbers", t.damageNumbers},
        {"Blood", t.blood},
        {"Rendering", t.rendering},
        {"ShadowRender", t.shadowRender},
        {"PostFX", t.postFX},
        {"GpuSubmit", t.gpuSubmit},
        {"BufferUploads", t.bufferUploads},
        {"TextureUploads", t.textureUploads},
        {"VboUpdates", t.vboUpdates},
        {"DrawCalls", t.drawCalls},
        {"WorldRender", t.worldRender},
        {"PlayerRender", t.playerRender},
        {"WeaponRender", t.weaponRender},
        {"EffectRender", t.effectRender},
        {"Healthbars", t.healthbars},
        {"Crosshair", t.crosshair},
        {"Killfeed", t.killfeed},
        {"MapTraversal", t.mapTraversal},
        {"WorldCulling", t.worldCulling},
        {"MemoryAlloc", t.memoryAlloc},
        {"StringFormat", t.stringFormat},
        {"FileIO", t.fileIO},
        {"DebugLogging", t.debugLogging},
    };
    int entryCount = sizeof(entries) / sizeof(entries[0]);

    for (int i = 0; i < entryCount - 1; ++i) {
        for (int j = 0; j < entryCount - 1 - i; ++j) {
            if (entries[j].ms < entries[j + 1].ms) {
                PerfEntry tmp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = tmp;
            }
        }
    }

    f << "Sorted by time:\n";
    for (int i = 0; i < entryCount; ++i) {
        if (entries[i].ms > 0.01) {
            f << "  " << entries[i].name << "  " << entries[i].ms << "ms\n";
        }
    }

    f << "\nCounters:\n";
    f << "  NPC count: " << t.npcCount << "\n";
    f << "  Effects alive: " << t.effectsAlive << "\n";
    f << "  Damage numbers alive: " << t.damageNumbersAlive << "\n";
    f << "  Particle count: " << t.particleCount << "\n";
    f << "  Draw calls: " << t.totalDrawCalls << "\n";
    f << "  Visible meshes: " << t.visibleMeshes << "\n";
    f << "  Visible triangles: " << t.visibleTriangles << "\n";
    f << "  Hidden meshes: " << t.hiddenMeshes << "\n";
    f << "  Shader switches: " << t.shaderSwitches << "\n";
    f << "  Texture binds: " << t.textureBinds << "\n";
    f << "  Chunk cells visited: " << t.chunkCellsVisited << "\n";
    f << "  Unique triangles: " << t.uniqueTriangles << "\n";
    f << "  Broadphase queries: " << t.broadphaseQueries << "\n";
    f << "  Repeated queries: " << t.repeatedQueries << "\n";
    f << "  Triangle tests: " << t.triangleTestsCount << "\n";
    f << "  Collision pairs: " << t.collisionPairs << "\n";
    f << "  Frustum culled: " << t.frustumCulled << "\n";
    f << "  Occlusion culled: " << t.occlusionCulled << "\n";

    // Per-NPC profile output
    if (s.npcProfileCount > 0) {
        f << "\nTop 20 slowest NPCs:\n";
        int maxShow = std::min(s.npcProfileCount, 20);
        for (int i = 0; i < maxShow; ++i) {
            const auto& np = s.npcProfiles[i];
            f << "  NPC id=" << np.id << " total=" << np.totalMs << "ms"
              << " think=" << np.thinkMs
              << " path=" << np.pathfindingMs
              << " combat=" << np.combatMs
              << " coll=" << np.collisionMs
              << "\n";
        }
    }

    f.close();
}

void Perf::endFrame()
{
    PerfState& s = gState;

    s.frameNumber++;

    s.gameTime += gFramePacer.dt();

    float currentMs = gFramePacer.frameTimeMs();
    s.avgFrameCount += 1.0;
    s.avgFrameTimeMs += (currentMs - s.avgFrameTimeMs) / std::min(s.avgFrameCount, 100.0);

    if (s.showSpikes && currentMs > s.avgFrameTimeMs * 2.0 && s.avgFrameCount > 10.0)
        detectSpike(currentMs);

    s.frameHistoryCount = gFramePacer.historyCount();
    for (int i = 0; i < s.frameHistoryCount && i < 300; ++i)
        s.frameHistory[i] = gFramePacer.historyMs(i);

    // Auto-spike report when frame exceeds 10ms
    if ((s.showSpikes || s.perfFileLogging) && currentMs > 10.0f)
    {
        FrameSpikeReport& r = s.lastSpikeReport;
        r.frameNumber = s.frameNumber;
        r.totalFrameMs = currentMs;
        r.avgFrameMs = s.avgFrameTimeMs;
        r.npcCount = (int)s.npcCount;
        r.playerCount = (int)s.playerCount;
        r.effectsAlive = s.current.effectsAlive;
        r.damageNumbersAlive = s.current.damageNumbersAlive;
        r.particleCount = s.current.particleCount;
        r.drawCalls = s.current.totalDrawCalls;
        r.visibleMeshes = s.current.visibleMeshes;
        r.visibleTriangles = s.current.visibleTriangles;
        r.chunkCellsVisited = s.current.chunkCellsVisited;
        r.uniqueTriangles = s.current.uniqueTriangles;

        // Collect top contributors
        r.contributorCount = 0;

        struct NamedMs { const char* name; double ms; };
        NamedMs all[] = {
            {"Input", s.current.input},
            {"Setup", s.current.setup},
            {"Audio", s.current.audio},
            {"State", s.current.state},
            {"Replay", s.current.replay},
            {"Networking", s.current.networking},
            {"Camera", s.current.camera},
            {"Combat", s.current.combat},
            {"UI", s.current.ui},
            {"Physics", s.current.physics},
            {"Collision", s.current.collision},
            {"SweepSlide", s.current.sweepSlide},
            {"Depenetration", s.current.depenetration},
            {"ChunkQuery", s.current.chunkQuery},
            {"NpcUpdate", s.current.npcUpdate},
            {"NpcThink", s.current.npcThink},
            {"NpcPathfinding", s.current.npcPathfinding},
            {"NpcCombat", s.current.npcCombat},
            {"NpcCollision", s.current.npcCollision},
            {"Weapons", s.current.weapons},
            {"Projectiles", s.current.projectiles},
            {"Particles", s.current.particles},
            {"Blood", s.current.blood},
            {"Rendering", s.current.rendering},
            {"ShadowRender", s.current.shadowRender},
            {"PostFX", s.current.postFX},
            {"WorldRender", s.current.worldRender},
            {"PlayerRender", s.current.playerRender},
            {"EffectRender", s.current.effectRender},
        };
        int allCount = sizeof(all) / sizeof(all[0]);

        for (int i = 0; i < allCount - 1; ++i)
            for (int j = 0; j < allCount - 1 - i; ++j)
                if (all[j].ms < all[j + 1].ms) {
                    NamedMs tmp = all[j]; all[j] = all[j + 1]; all[j + 1] = tmp;
                }

        for (int i = 0; i < allCount && r.contributorCount < FrameSpikeReport::MAX_CONTRIBUTORS; ++i) {
            if (all[i].ms > 0.5) {
                r.contributors[r.contributorCount].name = all[i].name;
                r.contributors[r.contributorCount].ms = all[i].ms;
                r.contributorCount++;
            }
        }

        writeSpikeReport(r);
    }

    // Write profile file each second if logging enabled
    if (s.perfFileLogging)
        writeProfileToFile();

    if (s.benchmarkRunning && s.benchmarkFrameCount < 3600)
    {
        s.benchmarkFrameTimes[s.benchmarkFrameCount++] = gFramePacer.frameTimeMs();
        double elapsed = (double)(nowUs() - (uint64_t)s.benchmarkStartWall) / 1000000.0;
        if (elapsed >= s.benchmarkDuration)
        {
            s.benchmarkRunning = false;
            char path[256];
            snprintf(path, sizeof(path), "benchmark_report.txt");
            exportReport(path);
            Debug::log(Debug::Category::General, "[PERF] Benchmark completed after %.1f seconds, report: %s",
                       elapsed, path);
        }
    }

    if (s.stressRunning)
    {
        s.stressTimer += gFramePacer.dt();
    }
}

void Perf::addTime(const char* name, double ms)
{
    PerfTimes& t = gState.current;
    if (std::strcmp(name, "Input") == 0)                        t.input += ms;
    else if (std::strcmp(name, "Setup") == 0)                   t.setup += ms;
    else if (std::strcmp(name, "Audio") == 0)                   t.audio += ms;
    else if (std::strcmp(name, "State") == 0)                   t.state += ms;
    else if (std::strcmp(name, "Replay") == 0)                  t.replay += ms;
    else if (std::strcmp(name, "Networking") == 0)              t.networking += ms;
    else if (std::strcmp(name, "Camera") == 0)                  t.camera += ms;
    else if (std::strcmp(name, "Combat") == 0)                  t.combat += ms;
    else if (std::strcmp(name, "UI") == 0)                      t.ui += ms;
    else if (std::strcmp(name, "Physics") == 0 ||
             std::strcmp(name, "Simulation") == 0)              t.physics += ms;
    else if (std::strcmp(name, "Collision") == 0)               t.collision += ms;
    else if (std::strcmp(name, "Movement") == 0)                t.movement += ms;
    else if (std::strcmp(name, "SweepSlide") == 0)              t.sweepSlide += ms;
    else if (std::strcmp(name, "Depenetration") == 0)           t.depenetration += ms;
    else if (std::strcmp(name, "GroundDetection") == 0)         t.groundDetection += ms;
    else if (std::strcmp(name, "ChunkQuery") == 0)              t.chunkQuery += ms;
    else if (std::strcmp(name, "Broadphase") == 0)              t.broadphase += ms;
    else if (std::strcmp(name, "Narrowphase") == 0)             t.narrowphase += ms;
    else if (std::strcmp(name, "TriangleTests") == 0)           t.triangleTests += ms;
    else if (std::strcmp(name, "CharVsWorld") == 0)             t.charVsWorld += ms;
    else if (std::strcmp(name, "CharVsChar") == 0)              t.charVsChar += ms;
    else if (std::strcmp(name, "WeaponCollisions") == 0)        t.weaponCollisions += ms;
    else if (std::strcmp(name, "NpcUpdate") == 0)               t.npcUpdate += ms;
    else if (std::strcmp(name, "NpcThink") == 0)                t.npcThink += ms;
    else if (std::strcmp(name, "NpcPathfinding") == 0)          t.npcPathfinding += ms;
    else if (std::strcmp(name, "NpcTargetAcq") == 0)            t.npcTargetAcq += ms;
    else if (std::strcmp(name, "NpcVision") == 0)               t.npcVision += ms;
    else if (std::strcmp(name, "NpcMovement") == 0)             t.npcMovement += ms;
    else if (std::strcmp(name, "NpcWeapon") == 0)               t.npcWeapon += ms;
    else if (std::strcmp(name, "NpcCombat") == 0)               t.npcCombat += ms;
    else if (std::strcmp(name, "NpcCollision") == 0)            t.npcCollision += ms;
    else if (std::strcmp(name, "NpcAnimation") == 0)            t.npcAnimation += ms;
    else if (std::strcmp(name, "NpcRender") == 0)               t.npcRender += ms;
    else if (std::strcmp(name, "NpcSpawn") == 0)                t.npcSpawn += ms;
    else if (std::strcmp(name, "AI") == 0)                      t.npcAi += ms;
    else if (std::strcmp(name, "Weapons") == 0)                 t.weapons += ms;
    else if (std::strcmp(name, "Projectiles") == 0)             t.projectiles += ms;
    else if (std::strcmp(name, "Particles") == 0)               t.particles += ms;
    else if (std::strcmp(name, "DamageNumbers") == 0)           t.damageNumbers += ms;
    else if (std::strcmp(name, "Blood") == 0)                   t.blood += ms;
    else if (std::strcmp(name, "Rendering") == 0)               t.rendering += ms;
    else if (std::strcmp(name, "ShadowRender") == 0)            t.shadowRender += ms;
    else if (std::strcmp(name, "PostFX") == 0)                  t.postFX += ms;
    else if (std::strcmp(name, "GpuSubmit") == 0)               t.gpuSubmit += ms;
    else if (std::strcmp(name, "BufferUploads") == 0)           t.bufferUploads += ms;
    else if (std::strcmp(name, "TextureUploads") == 0)          t.textureUploads += ms;
    else if (std::strcmp(name, "VboUpdates") == 0)              t.vboUpdates += ms;
    else if (std::strcmp(name, "DrawCalls") == 0)               t.drawCalls += ms;
    else if (std::strcmp(name, "WorldRender") == 0)             t.worldRender += ms;
    else if (std::strcmp(name, "PlayerRender") == 0)            t.playerRender += ms;
    else if (std::strcmp(name, "WeaponRender") == 0)            t.weaponRender += ms;
    else if (std::strcmp(name, "EffectRender") == 0)            t.effectRender += ms;
    else if (std::strcmp(name, "Healthbars") == 0)              t.healthbars += ms;
    else if (std::strcmp(name, "Crosshair") == 0)               t.crosshair += ms;
    else if (std::strcmp(name, "Killfeed") == 0)                t.killfeed += ms;
    else if (std::strcmp(name, "MapTraversal") == 0)            t.mapTraversal += ms;
    else if (std::strcmp(name, "WorldCulling") == 0)            t.worldCulling += ms;
    else if (std::strcmp(name, "MemoryAlloc") == 0)             t.memoryAlloc += ms;
    else if (std::strcmp(name, "StringFormat") == 0)            t.stringFormat += ms;
    else if (std::strcmp(name, "FileIO") == 0)                  t.fileIO += ms;
    else if (std::strcmp(name, "DebugLogging") == 0)            t.debugLogging += ms;
    t.total += ms;
}

Perf::ScopedTimer::ScopedTimer(const char* n)
    : name(n), startUs(nowUs())
{
}

Perf::ScopedTimer::~ScopedTimer()
{
    double ms = (double)(nowUs() - startUs) / 1000.0;
    addTime(name, ms);
}

void Perf::collectNpcProfile(unsigned int npcId, const char* category, double ms)
{
    PerfState& s = gState;
    int idx = -1;
    for (int i = 0; i < s.npcProfileCount; ++i) {
        if (s.npcProfiles[i].id == npcId) { idx = i; break; }
    }
    if (idx < 0 && s.npcProfileCount < PerfState::MAX_NPC_PROFILE) {
        idx = s.npcProfileCount++;
        s.npcProfiles[idx] = {};
        s.npcProfiles[idx].id = npcId;
    }
    if (idx >= 0) {
        auto& np = s.npcProfiles[idx];
        np.totalMs += ms;
        if (std::strcmp(category, "think") == 0)         np.thinkMs += ms;
        else if (std::strcmp(category, "path") == 0)     np.pathfindingMs += ms;
        else if (std::strcmp(category, "target") == 0)   np.targetAcqMs += ms;
        else if (std::strcmp(category, "vision") == 0)   np.visionMs += ms;
        else if (std::strcmp(category, "move") == 0)     np.movementMs += ms;
        else if (std::strcmp(category, "weapon") == 0)   np.weaponMs += ms;
        else if (std::strcmp(category, "combat") == 0)   np.combatMs += ms;
        else if (std::strcmp(category, "collision") == 0) np.collisionMs += ms;
        else if (std::strcmp(category, "anim") == 0)     np.animationMs += ms;
        else if (std::strcmp(category, "render") == 0)   np.renderMs += ms;
    }
}

void Perf::flushNpcProfiles()
{
    PerfState& s = gState;
    if (s.npcProfileCount < 2) return;
    // Sort by total descending
    for (int i = 0; i < s.npcProfileCount - 1; ++i)
        for (int j = 0; j < s.npcProfileCount - 1 - i; ++j)
            if (s.npcProfiles[j].totalMs < s.npcProfiles[j + 1].totalMs) {
                auto tmp = s.npcProfiles[j];
                s.npcProfiles[j] = s.npcProfiles[j + 1];
                s.npcProfiles[j + 1] = tmp;
            }
}

void Perf::togglePerfReport()
{
    gState.showPerfReport = !gState.showPerfReport;
    Debug::log(Debug::Category::General, "[PERF] perf_report=%s",
               gState.showPerfReport ? "ON" : "OFF");
}

void Perf::toggleGraph()
{
    gState.showGraph = !gState.showGraph;
    Debug::log(Debug::Category::General, "[PERF] perf_graph=%s",
               gState.showGraph ? "ON" : "OFF");
}

void Perf::toggleNpcPerf()
{
    gState.showNpcPerf = !gState.showNpcPerf;
    Debug::log(Debug::Category::General, "[PERF] perf_npc=%s",
               gState.showNpcPerf ? "ON" : "OFF");
}

void Perf::toggleMemory()
{
    gState.showMemory = !gState.showMemory;
    Debug::log(Debug::Category::General, "[PERF] perf_memory=%s",
               gState.showMemory ? "ON" : "OFF");
}

void Perf::toggleSpikes()
{
    gState.showSpikes = !gState.showSpikes;
    Debug::log(Debug::Category::General, "[PERF] perf_spikes=%s",
               gState.showSpikes ? "ON" : "OFF");
}

void Perf::toggleRenderStats()
{
    gState.renderStats = !gState.renderStats;
    Debug::log(Debug::Category::General, "[PERF] render_stats=%s",
               gState.renderStats ? "ON" : "OFF");
}

void Perf::toggleAllocAudit()
{
    gState.allocAudit = !gState.allocAudit;
    gState.totalAllocations = 0;
    Debug::log(Debug::Category::General, "[PERF] allocation_audit=%s",
               gState.allocAudit ? "ON" : "OFF");
}

void Perf::toggleAudioAudit()
{
    gState.audioAudit = !gState.audioAudit;
    Debug::log(Debug::Category::General, "[PERF] audio_audit=%s",
               gState.audioAudit ? "ON" : "OFF");
}

void Perf::togglePerfFileLogging()
{
    gState.perfFileLogging = !gState.perfFileLogging;
    Debug::log(Debug::Category::General, "[PERF] perf_file_logging=%s",
               gState.perfFileLogging ? "ON" : "OFF");
    if (gState.perfFileLogging) {
        std::filesystem::create_directories("logs");
        std::ofstream f("logs/performance_profile.txt", std::ios::trunc);
        if (f.is_open()) {
            f << "=== PERFORMANCE PROFILE LOG ===\n";
            f << "Frame: Total, Input, Setup, Audio, State, Replay, Networking, Camera, Combat, UI, "
                 "Physics, Collision, Movement, SweepSlide, Depenetration, GroundDetection, ChunkQuery, "
                 "Broadphase, Narrowphase, TriangleTests, CharVsWorld, CharVsChar, WeaponCollisions, "
                 "NpcUpdate, NpcThink, NpcPathfinding, NpcTargetAcq, NpcVision, NpcMovement, NpcWeapon, "
                 "NpcCombat, NpcCollision, NpcAnimation, NpcRender, NpcSpawn, NpcAi, "
                 "Weapons, Projectiles, Particles, DamageNumbers, Blood, "
                 "Rendering, ShadowRender, PostFX, GpuSubmit, BufferUploads, TextureUploads, VboUpdates, "
                 "DrawCalls, WorldRender, PlayerRender, WeaponRender, EffectRender, "
                 "Healthbars, Crosshair, Killfeed, MapTraversal, WorldCulling, "
                 "MemoryAlloc, StringFormat, FileIO, DebugLogging, "
                 "NPCs, Effects, DrawCalls, VisibleMeshes, Triangles, ChunkCells\n";
            f.close();
        }
    }
}

void Perf::setPreset(int p)
{
    if (p < 0 || p > 3) p = 0;
    gState.preset = p;
    applyPreset(p);
    static const char* names[] = {"Default", "Low", "Medium", "High"};
    Debug::log(Debug::Category::General, "[PERF] preset=%s", names[p]);
}

void Perf::applyPreset(int p)
{
    (void)p;
}

void Perf::startBenchmark(double seconds)
{
    gState.benchmarkRunning = true;
    gState.benchmarkDuration = seconds;
    gState.benchmarkFrameCount = 0;
    gState.benchmarkStartWall = (double)nowUs();
    Debug::log(Debug::Category::General, "[PERF] Benchmark started for %.1f seconds", seconds);
}

void Perf::startStress(int npcTarget)
{
    gState.stressRunning = true;
    gState.stressTotalNpcs = npcTarget;
    gState.stressTimer = 0.0;
    Debug::log(Debug::Category::General, "[PERF] Stress test started, target NPCs: %d", npcTarget);
}

void Perf::startCombatTest()
{
    gState.combatTestRunning = true;
    gState.stressRunning = true;
    gState.stressTotalNpcs = 10;
    gState.stressTimer = 0.0;
    Debug::log(Debug::Category::General, "[PERF] Combat stress test started");
}
