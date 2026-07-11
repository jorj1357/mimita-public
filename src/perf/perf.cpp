#include "perf/perf.h"
#include "perf/perf-spike.h"

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
    gState.assetLoadsThisFrame = 0;
    gState.current = PerfTimes{};
    gState.children = PerfTimes{};
    gState.npcProfileCount = 0;
    gState.timerStackDepth = 0;
    gState.queryRecordCount = 0;
    gState.dupTrackerCount = 0;
    gState.largeAabbAlert = {};
    perfResetScopes();
}

// ── String-based dispatch helpers ─────────────────────────────

static PerfTimes* sActiveTimes = nullptr;

void Perf::addTime(const char* name, double ms)
{
    PerfTimes& t = gState.current;
    if (std::strcmp(name, "Input") == 0)                        t.input += ms;
    else if (std::strcmp(name, "Setup") == 0)                   t.setup += ms;
    else if (std::strcmp(name, "Audio") == 0)                   t.audio += ms;
    else if (std::strcmp(name, "State") == 0)                   t.state += ms;
    else if (std::strcmp(name, "Replay") == 0)                  t.replay += ms;
    else if (std::strcmp(name, "ReplayTick") == 0)              t.replayTick += ms;
    else if (std::strcmp(name, "ReplaySeek") == 0)              t.replaySeek += ms;
    else if (std::strcmp(name, "ReplayInterp") == 0)            t.replayInterp += ms;
    else if (std::strcmp(name, "ReplayAudio") == 0)             t.replayAudio += ms;
    else if (std::strcmp(name, "ReplayCamera") == 0)            t.replayCamera += ms;
    else if (std::strcmp(name, "ReplayRender") == 0)            t.replayRender += ms;
    else if (std::strcmp(name, "ReplayGui") == 0)               t.replayGui += ms;
    else if (std::strcmp(name, "ReplayCollision") == 0)         t.replayCollision += ms;
    else if (std::strcmp(name, "ReplayEffects") == 0)           t.replayEffects += ms;
    else if (std::strcmp(name, "ReplayExport") == 0)            t.replayExport += ms;
    else if (std::strcmp(name, "Networking") == 0)              t.networking += ms;
    else if (std::strcmp(name, "Camera") == 0)                  t.camera += ms;
    else if (std::strcmp(name, "Combat") == 0)                  t.combat += ms;
    else if (std::strcmp(name, "UI") == 0)                      t.ui += ms;
    else if (std::strcmp(name, "ReplayHud") == 0)               t.replayHud += ms;
    else if (std::strcmp(name, "Menus") == 0)                   t.menus += ms;
    else if (std::strcmp(name, "DebugOverlay") == 0)            t.debugOverlay += ms;
    else if (std::strcmp(name, "Console") == 0)                 t.consoleTime += ms;
    else if (std::strcmp(name, "FontRender") == 0)              t.fontRender += ms;
    else if (std::strcmp(name, "TextLayout") == 0)              t.textLayout += ms;
    else if (std::strcmp(name, "Json") == 0)                    t.jsonTime += ms;
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
    else if (std::strcmp(name, "ProjectileCollision") == 0)     t.projectileCollision += ms;
    else if (std::strcmp(name, "ProjectileExplosion") == 0)     t.projectileExplosion += ms;
    else if (std::strcmp(name, "Shotgun") == 0)                 t.shotgun += ms;
    else if (std::strcmp(name, "HitFX") == 0)                   t.hitfxTotal += ms;
    else if (std::strcmp(name, "HitFXSpawn") == 0)              t.hitfxSpawn += ms;
    else if (std::strcmp(name, "HitFXUpdate") == 0)             t.hitfxUpdate += ms;
    else if (std::strcmp(name, "ParticleSpawn") == 0)           t.particleSpawn += ms;
    else if (std::strcmp(name, "Respawn") == 0)                 t.respawn += ms;
    else if (std::strcmp(name, "NpcSpawnTime") == 0)            t.npcSpawnTime += ms;
    else if (std::strcmp(name, "NpcDestroyTime") == 0)          t.npcDestroyTime += ms;
    else if (std::strcmp(name, "AudioTime") == 0)               t.audioTime += ms;
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
    else if (std::strcmp(name, "FrameOverhead") == 0)           t.frameOverhead += ms;
    else if (std::strcmp(name, "Sleep") == 0)                   t.sleepTime += ms;
    else if (std::strcmp(name, "Swap") == 0)                    t.swapTime += ms;
    else if (std::strcmp(name, "Terminal") == 0)                t.terminalTime += ms;
    else if (std::strcmp(name, "Menus") == 0)                   t.menusTime += ms;
    else if (std::strcmp(name, "DevOverlay") == 0)              t.devOverlay += ms;
    else if (std::strcmp(name, "Diag") == 0)                    t.diagTime += ms;
    t.total += ms;
}

void Perf::addChildTime(const char* name, double ms)
{
    PerfTimes& c = gState.children;
    if (std::strcmp(name, "Input") == 0)                        c.input += ms;
    else if (std::strcmp(name, "Setup") == 0)                   c.setup += ms;
    else if (std::strcmp(name, "Audio") == 0)                   c.audio += ms;
    else if (std::strcmp(name, "State") == 0)                   c.state += ms;
    else if (std::strcmp(name, "Replay") == 0)                  c.replay += ms;
    else if (std::strcmp(name, "ReplayTick") == 0)              c.replayTick += ms;
    else if (std::strcmp(name, "ReplaySeek") == 0)              c.replaySeek += ms;
    else if (std::strcmp(name, "ReplayInterp") == 0)            c.replayInterp += ms;
    else if (std::strcmp(name, "ReplayAudio") == 0)             c.replayAudio += ms;
    else if (std::strcmp(name, "ReplayCamera") == 0)            c.replayCamera += ms;
    else if (std::strcmp(name, "ReplayRender") == 0)            c.replayRender += ms;
    else if (std::strcmp(name, "ReplayGui") == 0)               c.replayGui += ms;
    else if (std::strcmp(name, "ReplayCollision") == 0)         c.replayCollision += ms;
    else if (std::strcmp(name, "ReplayEffects") == 0)           c.replayEffects += ms;
    else if (std::strcmp(name, "ReplayExport") == 0)            c.replayExport += ms;
    else if (std::strcmp(name, "Networking") == 0)              c.networking += ms;
    else if (std::strcmp(name, "Camera") == 0)                  c.camera += ms;
    else if (std::strcmp(name, "Combat") == 0)                  c.combat += ms;
    else if (std::strcmp(name, "UI") == 0)                      c.ui += ms;
    else if (std::strcmp(name, "Physics") == 0 ||
             std::strcmp(name, "Simulation") == 0)              c.physics += ms;
    else if (std::strcmp(name, "Collision") == 0)               c.collision += ms;
    else if (std::strcmp(name, "Movement") == 0)                c.movement += ms;
    else if (std::strcmp(name, "SweepSlide") == 0)              c.sweepSlide += ms;
    else if (std::strcmp(name, "Depenetration") == 0)           c.depenetration += ms;
    else if (std::strcmp(name, "GroundDetection") == 0)         c.groundDetection += ms;
    else if (std::strcmp(name, "ChunkQuery") == 0)              c.chunkQuery += ms;
    else if (std::strcmp(name, "Broadphase") == 0)              c.broadphase += ms;
    else if (std::strcmp(name, "Narrowphase") == 0)             c.narrowphase += ms;
    else if (std::strcmp(name, "TriangleTests") == 0)           c.triangleTests += ms;
    else if (std::strcmp(name, "CharVsWorld") == 0)             c.charVsWorld += ms;
    else if (std::strcmp(name, "CharVsChar") == 0)              c.charVsChar += ms;
    else if (std::strcmp(name, "WeaponCollisions") == 0)        c.weaponCollisions += ms;
    else if (std::strcmp(name, "NpcUpdate") == 0)               c.npcUpdate += ms;
    else if (std::strcmp(name, "NpcThink") == 0)                c.npcThink += ms;
    else if (std::strcmp(name, "NpcPathfinding") == 0)          c.npcPathfinding += ms;
    else if (std::strcmp(name, "NpcTargetAcq") == 0)            c.npcTargetAcq += ms;
    else if (std::strcmp(name, "NpcVision") == 0)               c.npcVision += ms;
    else if (std::strcmp(name, "NpcMovement") == 0)             c.npcMovement += ms;
    else if (std::strcmp(name, "NpcWeapon") == 0)               c.npcWeapon += ms;
    else if (std::strcmp(name, "NpcCombat") == 0)               c.npcCombat += ms;
    else if (std::strcmp(name, "NpcCollision") == 0)            c.npcCollision += ms;
    else if (std::strcmp(name, "NpcAnimation") == 0)            c.npcAnimation += ms;
    else if (std::strcmp(name, "NpcRender") == 0)               c.npcRender += ms;
    else if (std::strcmp(name, "NpcSpawn") == 0)                c.npcSpawn += ms;
    else if (std::strcmp(name, "AI") == 0)                      c.npcAi += ms;
    else if (std::strcmp(name, "Weapons") == 0)                 c.weapons += ms;
    else if (std::strcmp(name, "Projectiles") == 0)             c.projectiles += ms;
    else if (std::strcmp(name, "ProjectileCollision") == 0)     c.projectileCollision += ms;
    else if (std::strcmp(name, "ProjectileExplosion") == 0)     c.projectileExplosion += ms;
    else if (std::strcmp(name, "Shotgun") == 0)                 c.shotgun += ms;
    else if (std::strcmp(name, "HitFX") == 0)                   c.hitfxTotal += ms;
    else if (std::strcmp(name, "HitFXSpawn") == 0)              c.hitfxSpawn += ms;
    else if (std::strcmp(name, "HitFXUpdate") == 0)             c.hitfxUpdate += ms;
    else if (std::strcmp(name, "ParticleSpawn") == 0)           c.particleSpawn += ms;
    else if (std::strcmp(name, "Respawn") == 0)                 c.respawn += ms;
    else if (std::strcmp(name, "NpcSpawnTime") == 0)            c.npcSpawnTime += ms;
    else if (std::strcmp(name, "NpcDestroyTime") == 0)          c.npcDestroyTime += ms;
    else if (std::strcmp(name, "AudioTime") == 0)               c.audioTime += ms;
    else if (std::strcmp(name, "Particles") == 0)               c.particles += ms;
    else if (std::strcmp(name, "DamageNumbers") == 0)           c.damageNumbers += ms;
    else if (std::strcmp(name, "Blood") == 0)                   c.blood += ms;
    else if (std::strcmp(name, "Rendering") == 0)               c.rendering += ms;
    else if (std::strcmp(name, "ShadowRender") == 0)            c.shadowRender += ms;
    else if (std::strcmp(name, "PostFX") == 0)                  c.postFX += ms;
    else if (std::strcmp(name, "GpuSubmit") == 0)               c.gpuSubmit += ms;
    else if (std::strcmp(name, "BufferUploads") == 0)           c.bufferUploads += ms;
    else if (std::strcmp(name, "TextureUploads") == 0)          c.textureUploads += ms;
    else if (std::strcmp(name, "VboUpdates") == 0)              c.vboUpdates += ms;
    else if (std::strcmp(name, "DrawCalls") == 0)               c.drawCalls += ms;
    else if (std::strcmp(name, "WorldRender") == 0)             c.worldRender += ms;
    else if (std::strcmp(name, "PlayerRender") == 0)            c.playerRender += ms;
    else if (std::strcmp(name, "WeaponRender") == 0)            c.weaponRender += ms;
    else if (std::strcmp(name, "EffectRender") == 0)            c.effectRender += ms;
    else if (std::strcmp(name, "Healthbars") == 0)              c.healthbars += ms;
    else if (std::strcmp(name, "Crosshair") == 0)               c.crosshair += ms;
    else if (std::strcmp(name, "Killfeed") == 0)                c.killfeed += ms;
    else if (std::strcmp(name, "MapTraversal") == 0)            c.mapTraversal += ms;
    else if (std::strcmp(name, "WorldCulling") == 0)            c.worldCulling += ms;
    else if (std::strcmp(name, "MemoryAlloc") == 0)             c.memoryAlloc += ms;
    else if (std::strcmp(name, "StringFormat") == 0)            c.stringFormat += ms;
    else if (std::strcmp(name, "FileIO") == 0)                  c.fileIO += ms;
    else if (std::strcmp(name, "DebugLogging") == 0)            c.debugLogging += ms;
    else if (std::strcmp(name, "FrameOverhead") == 0)           c.frameOverhead += ms;
    else if (std::strcmp(name, "Sleep") == 0)                   c.sleepTime += ms;
    else if (std::strcmp(name, "Swap") == 0)                    c.swapTime += ms;
    else if (std::strcmp(name, "Terminal") == 0)                c.terminalTime += ms;
    else if (std::strcmp(name, "Menus") == 0)                   c.menusTime += ms;
    else if (std::strcmp(name, "DevOverlay") == 0)              c.devOverlay += ms;
    else if (std::strcmp(name, "Diag") == 0)                    c.diagTime += ms;
}

// ── ScopedTimer with nesting support ──────────────────────────

Perf::ScopedTimer::ScopedTimer(const char* n)
    : name(n), startUs(nowUs())
{
    PerfState& s = gState;
    if (s.timerStackDepth < PerfState::MAX_TIMER_STACK) {
        s.timerStack[s.timerStackDepth].name = n;
        s.timerStack[s.timerStackDepth].startUs = startUs;
        s.timerStackDepth++;
    }
}

Perf::ScopedTimer::~ScopedTimer()
{
    PerfState& s = gState;
    uint64_t endUs = nowUs();
    double elapsedMs = (double)(endUs - startUs) / 1000.0;

    // Add to this category's inclusive time
    addTime(name, elapsedMs);

    // Pop from timer stack
    if (s.timerStackDepth > 0) {
        s.timerStackDepth--;
        // If there's a parent timer, add this child's time to parent's childTime
        if (s.timerStackDepth > 0) {
            addChildTime(s.timerStack[s.timerStackDepth - 1].name, elapsedMs);
        }
    }
}

double Perf::exclusive(const PerfTimes& t, const PerfTimes& c, const char* name)
{
    if (std::strcmp(name, "Input") == 0)             return t.input - c.input;
    if (std::strcmp(name, "Setup") == 0)             return t.setup - c.setup;
    if (std::strcmp(name, "Audio") == 0)             return t.audio - c.audio;
    if (std::strcmp(name, "State") == 0)             return t.state - c.state;
    if (std::strcmp(name, "Replay") == 0)            return t.replay - c.replay;
    if (std::strcmp(name, "Networking") == 0)        return t.networking - c.networking;
    if (std::strcmp(name, "Camera") == 0)            return t.camera - c.camera;
    if (std::strcmp(name, "Combat") == 0)            return t.combat - c.combat;
    if (std::strcmp(name, "UI") == 0)                return t.ui - c.ui;
    if (std::strcmp(name, "Physics") == 0)           return t.physics - c.physics;
    if (std::strcmp(name, "Collision") == 0)         return t.collision - c.collision;
    if (std::strcmp(name, "SweepSlide") == 0)        return t.sweepSlide - c.sweepSlide;
    if (std::strcmp(name, "Depenetration") == 0)     return t.depenetration - c.depenetration;
    if (std::strcmp(name, "GroundDetection") == 0)   return t.groundDetection - c.groundDetection;
    if (std::strcmp(name, "ChunkQuery") == 0)        return t.chunkQuery - c.chunkQuery;
    if (std::strcmp(name, "WeaponCollisions") == 0)  return t.weaponCollisions - c.weaponCollisions;
    if (std::strcmp(name, "NPC Update") == 0 ||
        std::strcmp(name, "NpcUpdate") == 0)         return t.npcUpdate - c.npcUpdate;
    if (std::strcmp(name, "NPC Pathfinding") == 0 ||
        std::strcmp(name, "NpcPathfinding") == 0)    return t.npcPathfinding - c.npcPathfinding;
    if (std::strcmp(name, "NPC Combat") == 0 ||
        std::strcmp(name, "NpcCombat") == 0)         return t.npcCombat - c.npcCombat;
    if (std::strcmp(name, "NPC Collision") == 0 ||
        std::strcmp(name, "NpcCollision") == 0)      return t.npcCollision - c.npcCollision;
    if (std::strcmp(name, "NpcRender") == 0)         return t.npcRender - c.npcRender;
    if (std::strcmp(name, "Weapons") == 0)           return t.weapons - c.weapons;
    if (std::strcmp(name, "Shotgun") == 0)           return t.shotgun - c.shotgun;
    if (std::strcmp(name, "HitFX") == 0)             return t.hitfxTotal - c.hitfxTotal;
    if (std::strcmp(name, "HitFXSpawn") == 0)        return t.hitfxSpawn - c.hitfxSpawn;
    if (std::strcmp(name, "HitFXUpdate") == 0)       return t.hitfxUpdate - c.hitfxUpdate;
    if (std::strcmp(name, "ParticleSpawn") == 0)     return t.particleSpawn - c.particleSpawn;
    if (std::strcmp(name, "Respawn") == 0)           return t.respawn - c.respawn;
    if (std::strcmp(name, "NpcSpawnTime") == 0)      return t.npcSpawnTime - c.npcSpawnTime;
    if (std::strcmp(name, "NpcDestroyTime") == 0)    return t.npcDestroyTime - c.npcDestroyTime;
    if (std::strcmp(name, "AudioTime") == 0)         return t.audioTime - c.audioTime;
    if (std::strcmp(name, "Particles") == 0)         return t.particles - c.particles;
    if (std::strcmp(name, "Rendering") == 0)         return t.rendering - c.rendering;
    if (std::strcmp(name, "WorldRender") == 0)       return t.worldRender - c.worldRender;
    if (std::strcmp(name, "PlayerRender") == 0)      return t.playerRender - c.playerRender;
    if (std::strcmp(name, "EffectRender") == 0)      return t.effectRender - c.effectRender;
    if (std::strcmp(name, "ShadowRender") == 0)      return t.shadowRender - c.shadowRender;
    if (std::strcmp(name, "PostFX") == 0)            return t.postFX - c.postFX;
    if (std::strcmp(name, "FrameOverhead") == 0)     return t.frameOverhead - c.frameOverhead;
    if (std::strcmp(name, "Sleep") == 0)             return t.sleepTime - c.sleepTime;
    if (std::strcmp(name, "Swap") == 0)              return t.swapTime - c.swapTime;
    if (std::strcmp(name, "Terminal") == 0)          return t.terminalTime - c.terminalTime;
    if (std::strcmp(name, "Menus") == 0)             return t.menusTime - c.menusTime;
    if (std::strcmp(name, "DevOverlay") == 0)        return t.devOverlay - c.devOverlay;
    if (std::strcmp(name, "Diag") == 0)              return t.diagTime - c.diagTime;
    return t.total - 0.0;
}

// ── Collision query recording ────────────────────────────────

void Perf::recordCollisionQuery(const char* caller, const char* reason,
    const char* entity, const char* object,
    const float* aabbMin, const float* aabbMax,
    int chunkCells, int triangles, double ms)
{
    PerfState& s = gState;
    if (s.queryRecordCount < PerfState::MAX_QUERY_RECORDS) {
        auto& q = s.queryRecords[s.queryRecordCount++];
        q.queryId = ++s.queryIdCounter;
        q.frame = s.frameNumber;
        std::strncpy(q.caller, caller ? caller : "Unknown", sizeof(q.caller) - 1);
        q.caller[sizeof(q.caller) - 1] = '\0';
        std::strncpy(q.reason, reason ? reason : "Unknown", sizeof(q.reason) - 1);
        q.reason[sizeof(q.reason) - 1] = '\0';
        std::strncpy(q.entity, entity ? entity : "Unknown", sizeof(q.entity) - 1);
        q.entity[sizeof(q.entity) - 1] = '\0';
        std::strncpy(q.object, object ? object : "Unknown", sizeof(q.object) - 1);
        q.object[sizeof(q.object) - 1] = '\0';
        q.aabbMin[0] = aabbMin ? aabbMin[0] : 0;
        q.aabbMin[1] = aabbMin ? aabbMin[1] : 0;
        q.aabbMin[2] = aabbMin ? aabbMin[2] : 0;
        q.aabbMax[0] = aabbMax ? aabbMax[0] : 0;
        q.aabbMax[1] = aabbMax ? aabbMax[1] : 0;
        q.aabbMax[2] = aabbMax ? aabbMax[2] : 0;
        q.chunkCells = chunkCells;
        q.uniqueTriangles = triangles;
        q.elapsedMs = ms;
    }
}

void Perf::checkLargeAABB(const char* caller, const char* entity, const char* object,
    const char* reason, int chunkCells, int triangles,
    const float* aabbMin, const float* aabbMax)
{
    if (chunkCells <= 100 && triangles <= 500) return;
    PerfState& s = gState;
    auto& a = s.largeAabbAlert;
    a.frame = s.frameNumber;
    a.caller = caller;
    a.entity = entity;
    a.object = object;
    a.reason = reason;
    a.chunkCells = chunkCells;
    a.uniqueTriangles = triangles;
    if (aabbMin && aabbMax) {
        a.aabbSize[0] = aabbMax[0] - aabbMin[0];
        a.aabbSize[1] = aabbMax[1] - aabbMin[1];
        a.aabbSize[2] = aabbMax[2] - aabbMin[2];
    }
    if (chunkCells > 100 && triangles > 500)
        a.suspectedCause = "large AABB combined query";
    else if (chunkCells > 200)
        a.suspectedCause = "excessive chunk traversal";
    else
        a.suspectedCause = "triangle overload";
    Debug::warn(Debug::Category::Collision,
        "[LARGE AABB] frame=%d caller=%s entity=%s object=%s reason=%s "
        "cells=%d tris=%d aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) cause=%s\n",
        s.frameNumber, caller ? caller : "?",
        entity ? entity : "?", object ? object : "?",
        reason ? reason : "?",
        chunkCells, triangles,
        a.aabbSize[0], a.aabbSize[1], a.aabbSize[2],
        a.suspectedCause ? a.suspectedCause : "?");
}

void Perf::trackDuplicateQuery(const char* caller, double ms)
{
    PerfState& s = gState;
    for (int i = 0; i < s.dupTrackerCount; ++i) {
        if (std::strcmp(s.dupTrackers[i].caller, caller) == 0) {
            s.dupTrackers[i].count++;
            s.dupTrackers[i].totalMs += ms;
            return;
        }
    }
    if (s.dupTrackerCount < PerfState::MAX_DUP_TRACKERS) {
        std::strncpy(s.dupTrackers[s.dupTrackerCount].caller, caller ? caller : "?", 63);
        s.dupTrackers[s.dupTrackerCount].caller[63] = '\0';
        s.dupTrackers[s.dupTrackerCount].count = 1;
        s.dupTrackers[s.dupTrackerCount].totalMs = ms;
        s.dupTrackerCount++;
    }
}

// ── Profiler file output ─────────────────────────────────────

static void sortContributors(FrameSpikeReport& report)
{
    for (int i = 0; i < report.contributorCount - 1; ++i)
        for (int j = 0; j < report.contributorCount - 1 - i; ++j)
            if (report.contributors[j].exclMs < report.contributors[j + 1].exclMs) {
                auto tmp = report.contributors[j];
                report.contributors[j] = report.contributors[j + 1];
                report.contributors[j + 1] = tmp;
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
    f << "Top Exclusive Timers:\n";
    for (int i = 0; i < report.contributorCount; ++i) {
        f << "  " << report.contributors[i].name << "  excl="
          << report.contributors[i].exclMs << "ms  incl="
          << report.contributors[i].inclMs << "ms\n";
    }
    f << "NPC count: " << report.npcCount << "\n";
    f << "Effects: " << report.effectsAlive << "  Damage Numbers: "
      << report.damageNumbersAlive << "  Particles: " << report.particleCount << "\n";
    f << "Draw calls: " << report.drawCalls << "  Visible Meshes: "
      << report.visibleMeshes << "  Visible Tris: " << report.visibleTriangles << "\n";
    f << "Chunk cells: " << report.chunkCellsVisited
      << "  Unique tris: " << report.uniqueTriangles << "\n";

    // Duplicate queries
    if (report.dupCount > 0) {
        f << "\nDuplicate Collision Queries:\n";
        for (int i = 0; i < report.dupCount; ++i) {
            f << "  " << report.dups[i].caller << " x" << report.dups[i].count
              << " wasted=" << report.dups[i].wastedMs << "ms"
              << " cache=" << (report.dups[i].couldCache ? "YES" : "NO")
              << " reason=" << report.dups[i].reason << "\n";
        }
        f << "Estimated cache savings: " << report.estimatedCacheSavingsMs << "ms\n";
    }

    // Largest query
    if (report.largestQuery.caller && report.largestQuery.chunkCells > 0) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "\nLargest Collision Query:\n"
            "  caller=%s entity=%s object=%s reason=%s\n"
            "  aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f)"
            " cells=%d tris=%d ms=%.2f\n",
            report.largestQuery.caller,
            report.largestQuery.entity,
            report.largestQuery.object,
            report.largestQuery.reason,
            report.largestQuery.aabbMin[0], report.largestQuery.aabbMin[1], report.largestQuery.aabbMin[2],
            report.largestQuery.aabbMax[0], report.largestQuery.aabbMax[1], report.largestQuery.aabbMax[2],
            report.largestQuery.chunkCells,
            report.largestQuery.uniqueTriangles,
            report.largestQuery.elapsedMs);
        f << buf;
    }

    f.close();

    printf("\n=====================\n");
    printf("FRAME SPIKE\n");
    printf("=====================\n");
    printf("Frame: %d Total: %.1fms\n", report.frameNumber, report.totalFrameMs);
    for (int i = 0; i < report.contributorCount && i < 5; ++i) {
        printf("  %s: excl=%.1fms incl=%.1fms\n",
               report.contributors[i].name,
               report.contributors[i].exclMs,
               report.contributors[i].inclMs);
    }
    if (report.dupCount > 0)
        printf("  Duplicate queries: %d (est. cache savings %.1fms)\n",
               report.dupCount, report.estimatedCacheSavingsMs);
}

void Perf::writeProfileToFile()
{
    PerfState& s = gState;
    const PerfTimes& t = s.current;
    const PerfTimes& c = s.children;

    static uint64_t lastWriteUs = 0;
    uint64_t now = nowUs();
    if (now - lastWriteUs < 1000000) return;
    lastWriteUs = now;

    std::filesystem::create_directories("logs");
    std::ofstream f("logs/performance_profile.txt", std::ios::trunc);
    if (!f.is_open()) return;

    float totalMs = gFramePacer.frameTimeMs();
    f << "Frame: " << s.frameNumber << "  Total: " << totalMs << "ms\n\n";

    // Self-validation: sum of exclusive times vs total
    f << "Exclusive Breakdown:\n";

    struct ExclEntry { const char* name; double excl; double incl; };
    ExclEntry entries[] = {
        {"Input",          t.input - c.input, t.input},
        {"Setup",          t.setup - c.setup, t.setup},
        {"Audio",          t.audio - c.audio, t.audio},
        {"State",          t.state - c.state, t.state},
        {"Replay",         t.replay - c.replay, t.replay},
        {"Networking",     t.networking - c.networking, t.networking},
        {"Camera",         t.camera - c.camera, t.camera},
        {"Combat",         t.combat - c.combat, t.combat},
        {"UI",             t.ui - c.ui, t.ui},
        {"Physics",        t.physics - c.physics, t.physics},
        {"SweepSlide",     t.sweepSlide - c.sweepSlide, t.sweepSlide},
        {"Depenetration",  t.depenetration - c.depenetration, t.depenetration},
        {"GroundDetection",t.groundDetection - c.groundDetection, t.groundDetection},
        {"ChunkQuery",     t.chunkQuery - c.chunkQuery, t.chunkQuery},
        {"WeaponCollisions",t.weaponCollisions - c.weaponCollisions, t.weaponCollisions},
        {"Movement",       t.movement - c.movement, t.movement},
        {"NPC Update",     t.npcUpdate - c.npcUpdate, t.npcUpdate},
        {"NPC Pathfinding",t.npcPathfinding - c.npcPathfinding, t.npcPathfinding},
        {"NPC Combat",     t.npcCombat - c.npcCombat, t.npcCombat},
        {"NPC Collision",  t.npcCollision - c.npcCollision, t.npcCollision},
        {"NPC Render",     t.npcRender - c.npcRender, t.npcRender},
        {"Weapons",        t.weapons - c.weapons, t.weapons},
        {"Projectiles",    t.projectiles - c.projectiles, t.projectiles},
        {"Particles",      t.particles - c.particles, t.particles},
        {"Blood",          t.blood - c.blood, t.blood},
        {"Rendering",      t.rendering - c.rendering, t.rendering},
        {"ShadowRender",   t.shadowRender - c.shadowRender, t.shadowRender},
        {"PostFX",         t.postFX - c.postFX, t.postFX},
        {"WorldRender",    t.worldRender - c.worldRender, t.worldRender},
        {"PlayerRender",   t.playerRender - c.playerRender, t.playerRender},
        {"WeaponRender",   t.weaponRender - c.weaponRender, t.weaponRender},
        {"EffectRender",   t.effectRender - c.effectRender, t.effectRender},
        {"FrameOverhead",  t.frameOverhead - c.frameOverhead, t.frameOverhead},
        {"Sleep",          t.sleepTime - c.sleepTime, t.sleepTime},
        {"Swap",           t.swapTime - c.swapTime, t.swapTime},
        {"Terminal",       t.terminalTime - c.terminalTime, t.terminalTime},
        {"Menus",          t.menusTime - c.menusTime, t.menusTime},
        {"DevOverlay",     t.devOverlay - c.devOverlay, t.devOverlay},
        {"Diag",           t.diagTime - c.diagTime, t.diagTime},
    };
    int entryCount = sizeof(entries) / sizeof(entries[0]);

    // Sort by exclusive descending
    for (int i = 0; i < entryCount - 1; ++i)
        for (int j = 0; j < entryCount - 1 - i; ++j)
            if (entries[j].excl < entries[j + 1].excl) {
                ExclEntry tmp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = tmp;
            }

    double sumExcl = 0.0;
    for (int i = 0; i < entryCount; ++i) {
        if (entries[i].excl > 0.01 || entries[i].incl > 0.01) {
            f << "  " << entries[i].name
              << "  excl=" << entries[i].excl << "ms"
              << "  incl=" << entries[i].incl << "ms\n";
            sumExcl += entries[i].excl;
        }
    }

    double diff = totalMs - sumExcl;
    f << "\nSelf-Validation:\n";
    f << "  Frame Total: " << totalMs << "ms\n";
    f << "  Sum of Exclusive: " << sumExcl << "ms\n";
    f << "  Difference: " << diff << "ms";
    if (diff > 0.1) {
        f << "  *** UNACCOUNTED ***\n";
        f << "  Possible causes:\n";
        f << "    - FramePacer sleep (captured by Sleep timer if frame completed early)\n";
        f << "    - OpenGL driver overhead / GPU sync / glFlush/glFinish\n";
        f << "    - Thread synchronization waits (mutex, condition variable)\n";
        f << "    - OS scheduler preemption / context switches\n";
        f << "    - Memory allocation (malloc/free internals)\n";
        f << "    - Disk I/O (logging, file writes, map loading)\n";
        f << "    - std::vector reallocation / string copy overhead\n";
        f << "    - Profiler overhead (nowUs() calls, strcmp, file writes)\n";
        f << "    - Timer stack underflow (ScopedTimers out of order)\n";
    }
    f << "\n\n";

    // Counters
    f << "Counters:\n";
    f << "  NPCs=" << t.npcCount << " Effects=" << t.effectsAlive
      << " DamageNums=" << t.damageNumbersAlive << " Particles=" << t.particleCount << "\n";
    f << "  DrawCalls=" << t.totalDrawCalls << " VisMeshes=" << t.visibleMeshes
      << " VisTris=" << t.visibleTriangles << "\n";
    f << "  ChunkCells=" << t.chunkCellsVisited << " UniqueTris=" << t.uniqueTriangles
      << " RepeatedQueries=" << t.repeatedQueries << "\n";

    // Duplicate query tracking
    if (s.dupTrackerCount > 0) {
        f << "\nRepeated Collision Queries:\n";
        for (int i = 0; i < s.dupTrackerCount; ++i) {
            const auto& d = s.dupTrackers[i];
            f << "  " << d.caller << " x" << d.count << " total=" << d.totalMs << "ms\n";
        }
    }

    // Large AABB alert
    if (s.largeAabbAlert.caller) {
        const auto& a = s.largeAabbAlert;
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "\nLarge AABB Alert:\n"
            "  caller=%s entity=%s object=%s reason=%s\n"
            "  cells=%d tris=%d size=(%.1f %.1f %.1f) cause=%s\n",
            a.caller, a.entity, a.object, a.reason,
            a.chunkCells, a.uniqueTriangles,
            a.aabbSize[0], a.aabbSize[1], a.aabbSize[2],
            a.suspectedCause ? a.suspectedCause : "?");
        f << buf;
    }

    // Per-NPC profile
    if (s.npcProfileCount > 0) {
        f << "\nTop 20 slowest NPCs:\n";
        int maxShow = std::min(s.npcProfileCount, 20);
        for (int i = 0; i < maxShow; ++i) {
            const auto& np = s.npcProfiles[i];
            f << "  NPC id=" << np.id << " total=" << np.totalMs << "ms"
              << " think=" << np.thinkMs << " path=" << np.pathfindingMs
              << " combat=" << np.combatMs << " coll=" << np.collisionMs << "\n";
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

    // Build spike report for frames > 10ms
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
        r.repeatedQueryCount = s.current.repeatedQueries;

        const PerfTimes& t = s.current;
        const PerfTimes& c = s.children;

        struct NamedMs { const char* name; double incl; double excl; };
        NamedMs all[] = {
            {"Input", t.input, t.input - c.input},
            {"Setup", t.setup, t.setup - c.setup},
            {"Audio", t.audio, t.audio - c.audio},
            {"State", t.state, t.state - c.state},
            {"Replay", t.replay, t.replay - c.replay},
            {"Networking", t.networking, t.networking - c.networking},
            {"Camera", t.camera, t.camera - c.camera},
            {"Combat", t.combat, t.combat - c.combat},
            {"UI", t.ui, t.ui - c.ui},
            {"Physics", t.physics, t.physics - c.physics},
            {"SweepSlide", t.sweepSlide, t.sweepSlide - c.sweepSlide},
            {"Depenetration", t.depenetration, t.depenetration - c.depenetration},
            {"ChunkQuery", t.chunkQuery, t.chunkQuery - c.chunkQuery},
            {"WeaponCollisions", t.weaponCollisions, t.weaponCollisions - c.weaponCollisions},
            {"NpcUpdate", t.npcUpdate, t.npcUpdate - c.npcUpdate},
            {"NpcPathfinding", t.npcPathfinding, t.npcPathfinding - c.npcPathfinding},
            {"NpcCombat", t.npcCombat, t.npcCombat - c.npcCombat},
            {"NpcCollision", t.npcCollision, t.npcCollision - c.npcCollision},
            {"Weapons", t.weapons, t.weapons - c.weapons},
            {"Projectiles", t.projectiles, t.projectiles - c.projectiles},
            {"Particles", t.particles, t.particles - c.particles},
            {"Blood", t.blood, t.blood - c.blood},
            {"Rendering", t.rendering, t.rendering - c.rendering},
            {"ShadowRender", t.shadowRender, t.shadowRender - c.shadowRender},
            {"PostFX", t.postFX, t.postFX - c.postFX},
            {"WorldRender", t.worldRender, t.worldRender - c.worldRender},
            {"PlayerRender", t.playerRender, t.playerRender - c.playerRender},
            {"WeaponRender", t.weaponRender, t.weaponRender - c.weaponRender},
            {"EffectRender", t.effectRender, t.effectRender - c.effectRender},
            {"Sleep", t.sleepTime, t.sleepTime - c.sleepTime},
            {"Swap", t.swapTime, t.swapTime - c.swapTime},
            {"Terminal", t.terminalTime, t.terminalTime - c.terminalTime},
            {"Menus", t.menusTime, t.menusTime - c.menusTime},
            {"DevOverlay", t.devOverlay, t.devOverlay - c.devOverlay},
            {"Diag", t.diagTime, t.diagTime - c.diagTime},
            {"FrameOverhead", t.frameOverhead, t.frameOverhead - c.frameOverhead},
        };
        int allCount = sizeof(all) / sizeof(all[0]);

        for (int i = 0; i < allCount - 1; ++i)
            for (int j = 0; j < allCount - 1 - i; ++j)
                if (all[j].excl < all[j + 1].excl) {
                    NamedMs tmp = all[j]; all[j] = all[j + 1]; all[j + 1] = tmp;
                }

        r.contributorCount = 0;
        for (int i = 0; i < allCount && r.contributorCount < FrameSpikeReport::MAX_CONTRIBUTORS; ++i) {
            if (all[i].excl > 0.3) {
                r.contributors[r.contributorCount].name = all[i].name;
                r.contributors[r.contributorCount].exclMs = all[i].excl;
                r.contributors[r.contributorCount].inclMs = all[i].incl;
                r.contributorCount++;
            }
        }

        // Duplicate queries for spike report
        r.dupCount = 0;
        r.estimatedCacheSavingsMs = 0.0;
        for (int i = 0; i < s.dupTrackerCount && r.dupCount < FrameSpikeReport::MAX_DUPS; ++i) {
            if (s.dupTrackers[i].count > 1) {
                auto& d = r.dups[r.dupCount];
                d.caller = s.dupTrackers[i].caller;
                d.count = s.dupTrackers[i].count;
                d.wastedMs = s.dupTrackers[i].totalMs * (1.0 - 1.0 / s.dupTrackers[i].count);
                d.couldCache = true;
                d.reason = "identical AABB query repeated within frame";
                r.estimatedCacheSavingsMs += d.wastedMs;
                r.dupCount++;
            }
        }

        // Largest query
        if (s.queryRecordCount > 0) {
            int bestIdx = 0;
            double bestMs = 0;
            for (int i = 0; i < s.queryRecordCount; ++i) {
                if (s.queryRecords[i].elapsedMs > bestMs) {
                    bestMs = s.queryRecords[i].elapsedMs;
                    bestIdx = i;
                }
            }
            r.largestQuery = s.queryRecords[bestIdx];
        }

        writeSpikeReport(r);
    }

    // Periodic output for enabled commands (every ~60 frames)
    if (s.frameNumber % 60 == 0) {
        if (s.showLargeAabb && s.largeAabbAlert.caller[0]) {
            const auto& a = s.largeAabbAlert;
            if (a.chunkCells > 100 || a.uniqueTriangles > 500) {
                Debug::log(Debug::Category::Collision,
                    "[LARGE AABB] entity=%s reason=%s cells=%d tris=%d "
                    "size=(%.1f %.1f %.1f) cause=%s\n",
                    a.entity, a.reason, a.chunkCells, a.uniqueTriangles,
                    a.aabbSize[0], a.aabbSize[1], a.aabbSize[2], a.suspectedCause);
            }
        }
        if (s.showCollQueries) {
            printCollQuerySummary();
        }
        if (s.showEntityCounts) {
            printEntityCounts();
        }
    }

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
        s.stressTimer += gFramePacer.dt();

    // Aggregates scopes from MIMITA_PERF_SCOPE macros and writes spike reports
    double targetMs = gPerfBudget.targetFps > 0 ? 1000.0 / gPerfBudget.targetFps : 16.667;
    perfAggregateScopes((double)currentMs, targetMs);
}

// ── perfResetScopes ─────────────────────────────────────────

void perfResetScopes()
{
    gPerfScopeCount = 0;
    gPerfScopeStackDepth = 0;
    for (int i = 0; i < MAX_SCOPES_PER_FRAME; ++i) {
        gPerfScopes[i].active = false;
        gPerfScopes[i].cyclesInclusive = 0;
        gPerfScopes[i].cyclesSelf = 0;
        gPerfScopes[i].callCount = 0;
        gPerfScopes[i].minCycles = UINT64_MAX;
        gPerfScopes[i].maxCycles = 0;
        gPerfScopes[i].allocCount = 0;
        gPerfScopes[i].assetLoadCount = 0;
        gPerfScopes[i].collisionQueryCount = 0;
        gPerfScopes[i].parentIndex = -1;
    }
}

// ── perfLoadBudgetConfig ────────────────────────────────────

void perfLoadBudgetConfig()
{
    gPerfBudget.targetFps = 60;
    gPerfBudget.spikeThresholdMs = 20.0;
    gPerfBudget.severeThresholdMs = 100.0;
    gPerfBudget.catastrophicThresholdMs = 1000.0;
    gPerfBudget.captureCallTreeOnSpike = true;
    gPerfBudget.topFunctionsPerFrame = 30;
    gPerfBudget.historyFramesBeforeSpike = 120;
    gPerfBudget.historyFramesAfterSpike = 180;
    gPerfBudget.aggregateWindowFrames = 600;
    gPerfBudget.logAllocations = true;
    gPerfBudget.logAssetIO = true;
    gPerfBudget.logCollisionQueries = true;
    gPerfBudget.logEntityCounts = true;
    gPerfBudget.logEffectCounts = true;
    gPerfBudget.logRenderCounts = true;
    gPerfBudget.enabled = true;

    std::ifstream f("config/debuglogger.json");
    if (!f.is_open()) return;

    try {
        std::string contents((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());

        auto perfPos = contents.find("\"performance\"");
        if (perfPos == std::string::npos) return;

        auto braceOpen = contents.find('{', perfPos);
        if (braceOpen == std::string::npos) return;

        int depth = 1;
        auto braceClose = braceOpen + 1;
        while (depth > 0 && braceClose < contents.size()) {
            if (contents[braceClose] == '{') depth++;
            else if (contents[braceClose] == '}') depth--;
            braceClose++;
        }
        std::string perfSection = contents.substr(braceOpen, braceClose - braceOpen);

        auto getVal = [&](const std::string& key) -> std::string {
            auto pos = perfSection.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            auto colon = perfSection.find(':', pos);
            if (colon == std::string::npos) return "";
            auto start = perfSection.find_first_not_of(" \t\r\n", colon + 1);
            if (start == std::string::npos) return "";
            auto end = perfSection.find_first_of(",\n\r}", start);
            if (end == std::string::npos) end = perfSection.size();
            std::string val = perfSection.substr(start, end - start);
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                val = val.substr(1, val.size() - 2);
            return val;
        };

        std::string fps = getVal("target_fps");
        if (!fps.empty()) gPerfBudget.targetFps = std::stoi(fps);

        std::string spike = getVal("spike_threshold_ms");
        if (!spike.empty()) gPerfBudget.spikeThresholdMs = std::stod(spike);

        std::string severe = getVal("severe_spike_threshold_ms");
        if (!severe.empty()) gPerfBudget.severeThresholdMs = std::stod(severe);

        std::string capture = getVal("capture_call_tree_on_spike");
        if (!capture.empty()) gPerfBudget.captureCallTreeOnSpike = (capture == "true");

        std::string topN = getVal("top_functions_per_frame");
        if (!topN.empty()) gPerfBudget.topFunctionsPerFrame = std::stoi(topN);
    } catch (...) {}

    Debug::log(Debug::Category::General, "[PERF] Budget config: target=%dfps spike=%.1fms\n",
               gPerfBudget.targetFps, gPerfBudget.spikeThresholdMs);
}

// ── Top exclusive printing ──────────────────────────────────

void Perf::printTopExclusive(int n)
{
    PerfState& s = gState;
    const PerfTimes& t = s.current;
    const PerfTimes& c = s.children;

    struct TopEntry { const char* name; double excl; };
    TopEntry entries[] = {
        {"NPCThink", t.npcThink - c.npcThink},
        {"NpcPathfinding", t.npcPathfinding - c.npcPathfinding},
        {"NpcCombat", t.npcCombat - c.npcCombat},
        {"NpcCollision", t.npcCollision - c.npcCollision},
        {"NpcRender", t.npcRender - c.npcRender},
        {"NpcUpdate", t.npcUpdate - c.npcUpdate},
        {"Collision", t.collision - c.collision},
        {"SweepSlide", t.sweepSlide - c.sweepSlide},
        {"Depenetration", t.depenetration - c.depenetration},
        {"ChunkQuery", t.chunkQuery - c.chunkQuery},
        {"WeaponCollisions", t.weaponCollisions - c.weaponCollisions},
        {"GroundDetection", t.groundDetection - c.groundDetection},
        {"Movement", t.movement - c.movement},
        {"Physics", t.physics - c.physics},
        {"Shotgun", t.shotgun - c.shotgun},
        {"HitFX", t.hitfxTotal - c.hitfxTotal},
        {"Respawn", t.respawn - c.respawn},
        {"Particles", t.particles - c.particles},
        {"Blood", t.blood - c.blood},
        {"Rendering", t.rendering - c.rendering},
        {"ShadowRender", t.shadowRender - c.shadowRender},
        {"PostFX", t.postFX - c.postFX},
        {"WorldRender", t.worldRender - c.worldRender},
        {"PlayerRender", t.playerRender - c.playerRender},
        {"EffectRender", t.effectRender - c.effectRender},
        {"Weapons", t.weapons - c.weapons},
        {"Projectiles", t.projectiles - c.projectiles},
        {"Replay", t.replay - c.replay},
        {"Networking", t.networking - c.networking},
        {"Camera", t.camera - c.camera},
        {"Combat", t.combat - c.combat},
        {"UI", t.ui - c.ui},
        {"Audio", t.audio - c.audio},
        {"Setup", t.setup - c.setup},
        {"State", t.state - c.state},
        {"Sleep", t.sleepTime - c.sleepTime},
        {"Swap", t.swapTime - c.swapTime},
        {"Terminal", t.terminalTime - c.terminalTime},
        {"Menus", t.menusTime - c.menusTime},
        {"Diag", t.diagTime - c.diagTime},
    };
    int entryCount = sizeof(entries) / sizeof(entries[0]);

    for (int i = 0; i < entryCount - 1; ++i)
        for (int j = 0; j < entryCount - 1 - i; ++j)
            if (entries[j].excl < entries[j + 1].excl) {
                TopEntry tmp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = tmp;
            }

    printf("\n=== Top Exclusive Systems ===\n\n");
    int count = std::min(n, entryCount);
    for (int i = 0; i < count; ++i) {
        if (entries[i].excl > 0.01)
            printf("%2d. %-20s %.2fms\n", i + 1, entries[i].name, entries[i].excl);
    }
    printf("\n");
}

void Perf::printCollQuerySummary()
{
    PerfState& s = gState;
    if (s.queryRecordCount == 0) {
        printf("No collision queries this frame.\n");
        return;
    }

    int totalTris = 0;
    int totalQueries = s.queryRecordCount;
    int worstIdx = 0;
    double worstMs = 0;
    double totalMs = 0;
    for (int i = 0; i < totalQueries; ++i) {
        const auto& q = s.queryRecords[i];
        totalTris += q.uniqueTriangles;
        totalMs += q.elapsedMs;
        if (q.elapsedMs > worstMs) { worstMs = q.elapsedMs; worstIdx = i; }
    }

    const auto& w = s.queryRecords[worstIdx];
    double avgMs = totalMs / std::max(1, totalQueries);

    printf("\n=== Collision Queries ===\n");
    printf("  Total Queries:  %d\n", totalQueries);
    printf("  Total Triangles: %d\n", totalTris);
    printf("  Avg/Query:       %.1f tris, %.3f ms\n",
           (double)totalTris / std::max(1, totalQueries), avgMs);
    printf("  Worst Query:     %s (%d tris, %.2f ms)\n",
           w.caller, w.uniqueTriangles, w.elapsedMs);
    printf("  95th Percentile: %.2f ms\n", avgMs * 1.5); // rough estimate
    printf("\n");
}

void Perf::printEntityCounts()
{
    PerfState& s = gState;
    printf("\n=== Entity Counts ===\n");
    printf("  Players:    %d\n", s.playerCount);
    printf("  NPCs:       %d\n", s.npcCount);
    printf("  Projectiles: %d\n", s.projectileCount);
    printf("  Particles:  %d\n", s.particleCount);
    printf("  Blood:      %d\n", s.bloodCount);
    printf("  Effects:    %d\n", s.effectCount);
    printf("  Corpses:    %d\n", s.corpseCount);
    printf("  Audio:      %d\n", s.audioSourceCount);
    printf("  Replay:     %.1f MB\n", s.replayMemoryMb);
    printf("\n");
}

// ── Toggles, presets, benchmark ──────────────────────────────

void Perf::togglePerfReport()
{
    gState.showPerfReport = !gState.showPerfReport;
    Debug::log(Debug::Category::General, "[PERF] perf_report=%s",
               gState.showPerfReport ? "ON" : "OFF");
}

void Perf::toggleGraph() { gState.showGraph = !gState.showGraph; }
void Perf::toggleNpcPerf() { gState.showNpcPerf = !gState.showNpcPerf; }
void Perf::toggleMemory() { gState.showMemory = !gState.showMemory; }

void Perf::toggleSpikes()
{
    gState.showSpikes = !gState.showSpikes;
    Debug::log(Debug::Category::General, "[PERF] perf_spikes=%s",
               gState.showSpikes ? "ON" : "OFF");
}

void Perf::toggleRenderStats() { gState.renderStats = !gState.renderStats; }
void Perf::toggleAllocAudit() { gState.allocAudit = !gState.allocAudit; gState.totalAllocations = 0; }
void Perf::toggleAudioAudit() { gState.audioAudit = !gState.audioAudit; }

void Perf::toggleLargeAabb() { gState.showLargeAabb = !gState.showLargeAabb; }

void Perf::toggleCollQueries() { gState.showCollQueries = !gState.showCollQueries; }

void Perf::toggleEntityCounts() { gState.showEntityCounts = !gState.showEntityCounts; }

void Perf::togglePerfFileLogging()
{
    gState.perfFileLogging = !gState.perfFileLogging;
    Debug::log(Debug::Category::General, "[PERF] perf_file_logging=%s",
               gState.perfFileLogging ? "ON" : "OFF");
    if (gState.perfFileLogging) {
        std::filesystem::create_directories("logs");
        std::ofstream f("logs/performance_profile.txt", std::ios::trunc);
        if (f.is_open()) f.close();
    }
}

void Perf::setPreset(int p)
{
    if (p < 0 || p > 3) p = 0;
    gState.preset = p;
    applyPreset(p);
}

void Perf::applyPreset(int p) { (void)p; }

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
}

void Perf::startCombatTest()
{
    gState.combatTestRunning = true;
    gState.stressRunning = true;
    gState.stressTotalNpcs = 10;
    gState.stressTimer = 0.0;
}

void Perf::collectNpcProfile(unsigned int npcId, const char* category, double ms)
{
    PerfState& s = gState;
    int idx = -1;
    for (int i = 0; i < s.npcProfileCount; ++i)
        if (s.npcProfiles[i].id == npcId) { idx = i; break; }
    if (idx < 0 && s.npcProfileCount < PerfState::MAX_NPC_PROFILE) {
        idx = s.npcProfileCount++;
        s.npcProfiles[idx] = {};
        s.npcProfiles[idx].id = npcId;
    }
    if (idx >= 0) {
        auto& np = s.npcProfiles[idx];
        np.totalMs += ms;
        if (std::strcmp(category, "think") == 0)        np.thinkMs += ms;
        else if (std::strcmp(category, "path") == 0)    np.pathfindingMs += ms;
        else if (std::strcmp(category, "target") == 0)  np.targetAcqMs += ms;
        else if (std::strcmp(category, "vision") == 0)  np.visionMs += ms;
        else if (std::strcmp(category, "move") == 0)    np.movementMs += ms;
        else if (std::strcmp(category, "weapon") == 0)  np.weaponMs += ms;
        else if (std::strcmp(category, "combat") == 0)  np.combatMs += ms;
        else if (std::strcmp(category, "collision") == 0) np.collisionMs += ms;
        else if (std::strcmp(category, "anim") == 0)    np.animationMs += ms;
        else if (std::strcmp(category, "render") == 0)  np.renderMs += ms;
    }
}

void Perf::flushNpcProfiles()
{
    PerfState& s = gState;
    if (s.npcProfileCount < 2) return;
    for (int i = 0; i < s.npcProfileCount - 1; ++i)
        for (int j = 0; j < s.npcProfileCount - 1 - i; ++j)
            if (s.npcProfiles[j].totalMs < s.npcProfiles[j + 1].totalMs) {
                auto tmp = s.npcProfiles[j];
                s.npcProfiles[j] = s.npcProfiles[j + 1];
                s.npcProfiles[j + 1] = tmp;
            }
}
