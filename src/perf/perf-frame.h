#pragma once

#include <cstdint>
#include <cstdio>

// ── Per-frame breakdown record ───────────────────────────────

struct PerfBreakdownEntry {
    char label[64];
    double selfMs;
    double inclMs;
    uint32_t callCount;
    int depth;             // nesting depth for tree display
};

static constexpr int MAX_BREAKDOWN_ENTRIES = 128;

struct PerfFrame {
    int frameNumber = 0;
    double totalMs = 0.0;
    double budgetMs = 0.0;

    int entryCount = 0;
    PerfBreakdownEntry entries[MAX_BREAKDOWN_ENTRIES];

    // Entity snapshot at this frame
    int npcCount = 0;
    int playerCount = 0;
    int particleCount = 0;
    int effectCount = 0;
    int audioCount = 0;
    int corpseCount = 0;
    int ragdollCount = 0;
    int bloodDecalCount = 0;
    int projectileCount = 0;
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
    int collisionQueryCount = 0;
};

// ── Ring buffer ──────────────────────────────────────────────

static constexpr int FRAME_HISTORY_CAPACITY = 300;

extern PerfFrame gFrameHistory[FRAME_HISTORY_CAPACITY];
extern int gFrameHistoryIndex;          // current write position (cyclical)
extern int gFrameHistoryCount;          // total frames written (up to CAPACITY)

// ── Entity snapshot for delta computation ────────────────────

struct EntitySnapshot {
    int npcCount = 0;
    int playerCount = 0;
    int particleCount = 0;
    int effectCount = 0;
    int audioCount = 0;
    int corpseCount = 0;
    int ragdollCount = 0;
    int bloodDecalCount = 0;
    int projectileCount = 0;
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
    int collisionQueryCount = 0;

    EntitySnapshot& operator=(const PerfFrame& f) {
        npcCount = f.npcCount;
        playerCount = f.playerCount;
        particleCount = f.particleCount;
        effectCount = f.effectCount;
        audioCount = f.audioCount;
        corpseCount = f.corpseCount;
        ragdollCount = f.ragdollCount;
        bloodDecalCount = f.bloodDecalCount;
        projectileCount = f.projectileCount;
        allocCount = f.allocCount;
        allocBytes = f.allocBytes;
        collisionQueryCount = f.collisionQueryCount;
        return *this;
    }
};

// ── API ──────────────────────────────────────────────────────

// Capture current frame into the ring buffer
void perfCaptureFrame(double totalMs, double budgetMs, int frameNumber);

// Dump spike context: N frames before → spike → N frames after
void perfDumpSpikeContext(int spikeFrameIndex, int framesBefore, int framesAfter);

// Write a single frame breakdown to a file
void perfWriteFrameBreakdown(FILE* f, const PerfFrame& frame, bool showChildren);

// Write the per-frame summary line (compact format)
void perfWriteFrameSummary(FILE* f, const PerfFrame& frame);
