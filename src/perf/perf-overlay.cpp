#include "perf/perf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "video/frame-pacer.h"
#include "gui/ui-system.h"
#include "debug/debug-log.h"

extern FramePacer gFramePacer;
extern PerfState gState;

void Perf::renderOverlay()
{
    PerfState& s = gState;
    float sw = uiScreenW();

    if (s.showPerfReport)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {0.3f, 1.0f, 0.5f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        float ms = gFramePacer.frameTimeMs();
        int fps = ms > 0.0f ? (int)(1000.0f / ms + 0.5f) : 0;
        int avgFps = gFramePacer.avgFrameTimeMs() > 0.0f ? (int)(1000.0f / gFramePacer.avgFrameTimeMs() + 0.5f) : 0;
        float budget = gFramePacer.targetFrameTimeMs();

        snprintf(buf, sizeof(buf), "FPS: %d  AVG: %d", fps, avgFps);
        text(buf, {0.3f, 1.0f, 0.5f, 1.0f});
        snprintf(buf, sizeof(buf), "FRAME: %.2fms  BUDGET: %.2fms", ms, budget);
        text(buf, {0.3f, 1.0f, 0.5f, 1.0f});

        snprintf(buf, sizeof(buf), "CPU: %.2fms", s.current.total);
        text(buf, {0.8f, 0.9f, 1.0f, 1.0f});

        double gpuMs = 0.0;
        if (gFramePacer.subsystemCount() > 0)
        {
            for (int i = 0; i < gFramePacer.subsystemCount(); ++i)
            {
                if (gFramePacer.subsystemName(i) &&
                    std::strcmp(gFramePacer.subsystemName(i), "Render") == 0)
                {
                    gpuMs = gFramePacer.subsystemTimeMs(i);
                    break;
                }
            }
        }
        snprintf(buf, sizeof(buf), "GPU: %.2fms", gpuMs);
        text(buf, {0.8f, 0.9f, 1.0f, 1.0f});

        snprintf(buf, sizeof(buf), "Draw Calls: %d  Triangles: %dk",
                 s.drawCalls, s.triangles / 1000);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});

        snprintf(buf, sizeof(buf), "Players: %d  NPCs: %d", s.playerCount, s.npcCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});

        snprintf(buf, sizeof(buf), "Blood: %d  Particles: %d",
                 s.bloodCount, s.particleCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});

        if (s.replayMemoryMb > 0.0)
        {
            snprintf(buf, sizeof(buf), "Replay: %.1fMB", s.replayMemoryMb);
            text(buf, {0.7f, 0.7f, 1.0f, 1.0f});
        }

        if (s.netBytesIn > 0.0 || s.netBytesOut > 0.0)
        {
            snprintf(buf, sizeof(buf), "Net In: %.0fB/s  Out: %.0fB/s",
                     s.netBytesIn, s.netBytesOut);
            text(buf, {0.5f, 1.0f, 0.8f, 1.0f});
        }

        if (s.allocAudit)
        {
            snprintf(buf, sizeof(buf), "Allocs: %d (total: %llu)",
                     s.allocationsThisFrame, (unsigned long long)s.totalAllocations);
            glm::vec4 col = s.allocationsThisFrame > 0 ? glm::vec4{1.0f, 0.3f, 0.3f, 1.0f} : glm::vec4{0.3f, 1.0f, 0.3f, 1.0f};
            text(buf, col);
        }

        if (s.audioAudit && s.soundLoadsDetected)
        {
            snprintf(buf, sizeof(buf), "WARN: %d sound loads during gameplay!", s.soundLoadCount);
            text(buf, {1.0f, 0.3f, 0.3f, 1.0f});
        }

        y += 4.0f;
        text("[Subsystem Timing]", {0.5f, 0.8f, 1.0f, 1.0f});

        PerfTimes& t = s.current;
        auto subLine = [&](const char* label, double val, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            if (val > 0.001)
            {
                snprintf(buf, sizeof(buf), "%s: %.2fms", label, val);
                text(buf, col);
            }
        };

        subLine("Input", t.input);
        subLine("Setup", t.setup);
        subLine("Audio", t.audio);
        subLine("State", t.state);
        subLine("Replay", t.replay);
        subLine("Networking", t.networking);
        subLine("Camera", t.camera);
        subLine("Combat", t.combat);
        subLine("UI", t.ui);
        subLine("Physics", t.physics);
        subLine("SweepSlide", t.sweepSlide);
        subLine("Depenetration", t.depenetration);
        subLine("GroundDetection", t.groundDetection);
        subLine("ChunkQuery", t.chunkQuery);
        subLine("BodyWeaponColl.", t.weaponCollisions);
        subLine("Movement", t.movement);
        subLine("NPC Update", t.npcUpdate);
        subLine("NPC Think", t.npcThink);
        subLine("NPC Pathfinding", t.npcPathfinding);
        subLine("NPC Combat", t.npcCombat);
        subLine("NPC Collision", t.npcCollision);
        subLine("NPC Render", t.npcRender);
        subLine("Weapons", t.weapons);
        subLine("Projectiles", t.projectiles);
        subLine("Particles", t.particles);
        subLine("Blood", t.blood);
        subLine("Damage Numbers", t.damageNumbers);
        subLine("Rendering", t.rendering);
        subLine("ShadowRender", t.shadowRender);
        subLine("PostFX", t.postFX);
        subLine("WorldRender", t.worldRender);
        subLine("PlayerRender", t.playerRender);
        subLine("WeaponRender", t.weaponRender);
        subLine("EffectRender", t.effectRender);
        subLine("WorldCulling", t.worldCulling);
        subLine("MapTraversal", t.mapTraversal);

        if (s.snapshotBuildMs > 0.0 || s.serializeMs > 0.0 || s.receiveMs > 0.0)
        {
            y += 4.0f;
            text("[Network Timing]", {0.5f, 1.0f, 0.5f, 1.0f});
            snprintf(buf, sizeof(buf), "Snapshot Build: %.2fms", s.snapshotBuildMs);
            text(buf);
            snprintf(buf, sizeof(buf), "Serialize: %.2fms", s.serializeMs);
            text(buf);
            snprintf(buf, sizeof(buf), "Receive: %.2fms", s.receiveMs);
            text(buf);
        }
    }

    if (s.showNpcPerf && s.npcCount > 0)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        if (s.showPerfReport)
        {
            int lines = 21;
            if (s.allocAudit) lines++;
            if (s.audioAudit && s.soundLoadsDetected) lines++;
            if (s.replayMemoryMb > 0.0) lines++;
            if (s.netBytesIn > 0.0 || s.netBytesOut > 0.0) lines += 3;
            y = (float)lines * 17.0f + 24.0f;
        }

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "[NPC] Count: %d  Peak: %.0f", s.npcCount, s.peakNpcCount);
        text(buf, {1.0f, 0.8f, 0.3f, 1.0f});
        snprintf(buf, sizeof(buf), "Spawned/Destroyed: %d / %d", s.totalNpcsSpawned, s.totalNpcsDestroyed);
        text(buf, {0.8f, 0.8f, 0.8f, 1.0f});

        PerfTimes& t = s.current;
        auto subLine = [&](const char* label, double val, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            if (val > 0.001)
            {
                snprintf(buf, sizeof(buf), "%s: %.2fms", label, val);
                text(buf, col);
            }
        };

        subLine("Update", t.npcUpdate, {0.6f, 1.0f, 0.6f, 1.0f});
        subLine("Combat", t.npcCombat, {1.0f, 0.5f, 0.3f, 1.0f});
        subLine("Pathfinding", t.npcPathfinding, {0.3f, 0.6f, 1.0f, 1.0f});
        subLine("Physics/Collision", t.npcCollision, {0.8f, 0.4f, 1.0f, 1.0f});
        subLine("Render", t.npcRender, {1.0f, 0.6f, 0.8f, 1.0f});

        if (s.npcCount > 0)
        {
            snprintf(buf, sizeof(buf), "Avg per NPC: %.3fms", t.npcUpdate / (double)s.npcCount);
            text(buf, {0.6f, 0.8f, 1.0f, 1.0f});
        }
    }

    if (s.showMemory)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        int offsetLines = 0;
        if (s.showPerfReport) offsetLines += 21;
        if (s.showNpcPerf) offsetLines += 12;
        y = (float)offsetLines * 17.0f + 24.0f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];

        int hours = (int)(s.gameTime / 3600.0);
        int mins = (int)((s.gameTime - hours * 3600.0) / 60.0);
        int secs = (int)(s.gameTime - hours * 3600.0 - mins * 60.0);
        snprintf(buf, sizeof(buf), "[Memory] Runtime: %dh %dm %ds", hours, mins, secs);
        text(buf, {0.5f, 1.0f, 0.5f, 1.0f});

        snprintf(buf, sizeof(buf), "NPCs: %d (peak %.0f)", s.npcCount, s.peakNpcCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});
        snprintf(buf, sizeof(buf), "Players: %d", s.playerCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});

        snprintf(buf, sizeof(buf), "Blood: %d  Particles: %d", s.bloodCount, s.particleCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});
        snprintf(buf, sizeof(buf), "Effects: %d  Corpses: %d", s.effectCount, s.corpseCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});

        if (s.replayMemoryMb > 0.0)
        {
            snprintf(buf, sizeof(buf), "Replay: %.1fMB", s.replayMemoryMb);
            text(buf, {0.7f, 0.7f, 1.0f, 1.0f});
        }

        snprintf(buf, sizeof(buf), "Audio sources: %d", s.audioSourceCount);
        text(buf, {0.5f, 0.8f, 1.0f, 1.0f});

        if (s.allocAudit)
        {
            snprintf(buf, sizeof(buf), "Allocs: %d (total: %llu)",
                     s.allocationsThisFrame, (unsigned long long)s.totalAllocations);
            glm::vec4 col = s.allocationsThisFrame > 0 ? glm::vec4{1.0f, 0.3f, 0.3f, 1.0f} : glm::vec4{0.3f, 1.0f, 0.3f, 1.0f};
            text(buf, col);
        }
    }

    if (s.showSpikes && s.lastSpike.frameTimeMs > 0.0)
    {
        float x = 12.0f;
        float y = uiScreenH() - 200.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 0.3f, 0.3f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "[PERF SPIKE] Frame=%d (%.1fms)", s.lastSpike.frameNumber, s.lastSpike.frameTimeMs);
        text(buf, {1.0f, 0.2f, 0.2f, 1.0f});
        snprintf(buf, sizeof(buf), "Worst: %s=%.2fms", s.lastSpike.subsystemName, s.lastSpike.worstSubsystemMs);
        text(buf, {1.0f, 0.5f, 0.3f, 1.0f});
        snprintf(buf, sizeof(buf), "NPCs=%d Replay=%.1fMB", s.lastSpike.npcCount, s.lastSpike.replayMemoryMb);
        text(buf, {1.0f, 0.6f, 0.3f, 1.0f});
    }

    if (s.showGraph)
    {
        float gx = sw - 340.0f;
        float gy = 8.0f;
        float gw = 320.0f;
        float gh = 100.0f;
        int samples = s.frameHistoryCount;

        uiDrawRect({gx, gy, gw, gh}, {0.0f, 0.0f, 0.0f, 0.6f}, "perf-graph-bg");

        float targetMs = gFramePacer.targetFrameTimeMs();
        auto drawRefLine = [&](float thresholdMs, glm::vec4 col) {
            float y = gy + gh - (thresholdMs / 20.0f) * gh;
            if (y >= gy && y <= gy + gh)
            {
                uiDrawText("--", gx - 16.0f, y - 6.0f, 0.22f, col);
                uiDrawRect({gx, y, gw, 1.0f}, col, "perf-ref");
            }
        };
        drawRefLine(16.0f, {1.0f, 0.2f, 0.2f, 0.5f});
        drawRefLine(8.0f, {1.0f, 0.8f, 0.2f, 0.5f});
        drawRefLine(4.0f, {0.3f, 1.0f, 0.3f, 0.5f});

        if (targetMs > 0.0f)
            drawRefLine(targetMs, {0.3f, 0.8f, 1.0f, 0.6f});

        if (samples > 1)
        {
            float maxMs = 20.0f;

            for (int i = 0; i < samples; ++i)
            {
                float x = gx + (float)i / (float)(300 - 1) * gw;
                float val = s.frameHistory[i];
                if (val < 0.0f) val = 0.0f;
                if (val > maxMs) val = maxMs;

                float barH = (val / maxMs) * gh;
                glm::vec4 col = val > targetMs * 1.5f
                    ? glm::vec4{1.0f, 0.2f, 0.2f, 0.8f}
                    : glm::vec4{0.3f, 1.0f, 0.5f, 0.6f};
                uiDrawRect({x, gy + gh - barH, 2.0f, barH}, col, "perf-bar");
            }
        }
    }

    if (s.renderStats)
    {
        float x = 12.0f;
        float y = uiScreenH() - 120.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "Draw Calls: %d", s.drawCalls);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});
        snprintf(buf, sizeof(buf), "Triangles: %d", s.triangles);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});

        double totalTime = s.current.total;
        if (totalTime > 0.0)
        {
            double renderPct = s.current.rendering / totalTime * 100.0;
            double bloodPct = s.current.blood / totalTime * 100.0;
            double npcPct = s.current.npcAi / totalTime * 100.0;
            double uiPct = s.current.ui / totalTime * 100.0;
            double physPct = s.current.physics / totalTime * 100.0;

            snprintf(buf, sizeof(buf), "Player Render: %.0f%%", renderPct * 0.3);
            text(buf);
            snprintf(buf, sizeof(buf), "NPC Render: %.0f%%", renderPct * 0.4);
            text(buf);
            snprintf(buf, sizeof(buf), "Blood Render: %.0f%%", bloodPct + renderPct * 0.2);
            text(buf);
            snprintf(buf, sizeof(buf), "UI Render: %.0f%%", uiPct);
            text(buf);
        }
    }
}

void Perf::detectSpike(double currentFrameMs)
{
    PerfState& s = gState;
    SpikeInfo spike;
    spike.frameTimeMs = currentFrameMs;
    spike.frameNumber = s.frameNumber;
    spike.npcCount = s.npcCount;
    spike.replayMemoryMb = s.replayMemoryMb;

    PerfTimes& t = s.current;
    struct Sub { const char* name; double ms; };
    Sub subs[] = {
        {"Input", t.input}, {"Setup", t.setup}, {"Audio", t.audio}, {"State", t.state},
        {"Replay", t.replay}, {"Networking", t.networking}, {"Camera", t.camera},
        {"Combat", t.combat}, {"UI", t.ui},
        {"Physics", t.physics}, {"SweepSlide", t.sweepSlide},
        {"Depenetration", t.depenetration}, {"ChunkQuery", t.chunkQuery},
        {"GroundDetection", t.groundDetection}, {"WeaponCollisions", t.weaponCollisions},
        {"Movement", t.movement},
        {"NPC Update", t.npcUpdate}, {"NPC Think", t.npcThink},
        {"NPC Pathfinding", t.npcPathfinding}, {"NPC Combat", t.npcCombat},
        {"NPC Collision", t.npcCollision}, {"NPC Render", t.npcRender},
        {"Weapons", t.weapons}, {"Projectiles", t.projectiles},
        {"Particles", t.particles}, {"Blood", t.blood},
        {"Rendering", t.rendering}, {"ShadowRender", t.shadowRender},
        {"PostFX", t.postFX}, {"WorldRender", t.worldRender},
        {"PlayerRender", t.playerRender}, {"EffectRender", t.effectRender},
    };
    double worst = 0.0;
    const char* worstName = "Unknown";
    for (const auto& sub : subs)
    {
        if (sub.ms > worst)
        {
            worst = sub.ms;
            worstName = sub.name;
        }
    }
    spike.worstSubsystemMs = worst;
    if (worstName)
    {
        int len = (int)std::strlen(worstName);
        if (len > 31) len = 31;
        std::strncpy(spike.subsystemName, worstName, (size_t)len);
        spike.subsystemName[len] = '\0';
    }

    if (spike.frameTimeMs > s.avgFrameTimeMs * 2.5)
    {
        s.lastSpike = spike;

        Debug::log(Debug::Category::General,
            "[PERF SPIKE] Frame=%d (%.1fms) avg=%.1fms worst=%s=%.2fms NPCs=%d\n",
            spike.frameNumber, spike.frameTimeMs, s.avgFrameTimeMs,
            spike.subsystemName, spike.worstSubsystemMs, spike.npcCount);
    }
}

