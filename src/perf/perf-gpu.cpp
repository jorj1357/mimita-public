// 08 31 2026, 11 40
/* purpose
* Implements deferred OpenGL GPU timer queries for performance diagnostics.
* Maintains a small ring of query pairs so query results are read later.
* Emits completed region timings through the existing performance logger.
* DOES NOT stall the GPU with glFinish or wait for unavailable query results.
* Does not change OpenGL state beyond timer-query objects.
* Does not measure or alter gameplay, replay, physics, or frame pacing.
*/
#include "perf/perf-gpu.h"

#include <array>
#include <cstdio>
#include <cstring>

#include <glad/glad.h>

#include "debug/debug-log.h"

namespace {
constexpr int kRingSize = 4;
constexpr int kMaxRegions = 16;

struct Region {
    const char* name = nullptr;
    GLuint begin = 0;
    GLuint end = 0;
    bool active = false;
};

struct Frame {
    std::array<Region, kMaxRegions> regions{};
    int count = 0;
    bool submitted = false;
};

std::array<Frame, kRingSize> gFrames{};
int gWriteFrame = 0;
int gCurrentRegion = -1;
bool gEnabled = false;
bool gInitialized = false;
double gLastCompletedFrameMs = 0.0;
double gLastCompletedWorldMs = 0.0;
double gLastCompletedShadowMs = 0.0;

void ensureQueries(Frame& frame)
{
    for (Region& region : frame.regions) {
        if (!region.begin)
            glGenQueries(1, &region.begin);
        if (!region.end)
            glGenQueries(1, &region.end);
    }
}
}

namespace PerfGpu {
bool enabled() { return gEnabled; }

double lastCompletedFrameMs() { return gLastCompletedFrameMs; }

double lastCompletedRegionMs(const char* name)
{
    if (!name) return 0.0;
    if (std::strcmp(name, "GPU::World") == 0) return gLastCompletedWorldMs;
    if (std::strcmp(name, "GPU::Shadows") == 0) return gLastCompletedShadowMs;
    return 0.0;
}

void setEnabled(bool enabledValue)
{
    gEnabled = enabledValue;
    if (gEnabled && !gInitialized) {
        for (Frame& frame : gFrames)
            ensureQueries(frame);
        gInitialized = true;
    }
}

void beginFrame()
{
    if (!gEnabled || !gInitialized)
        return;

    flushCompleted();
    Frame& frame = gFrames[gWriteFrame];
    frame.count = 0;
    frame.submitted = false;
    gCurrentRegion = -1;
}

void beginRegion(const char* name)
{
    if (!gEnabled || gCurrentRegion >= 0)
        return;

    Frame& frame = gFrames[gWriteFrame];
    if (frame.count >= kMaxRegions)
        return;

    Region& region = frame.regions[frame.count++];
    region.name = name;
    region.active = true;
    gCurrentRegion = frame.count - 1;
    glQueryCounter(region.begin, GL_TIMESTAMP);
}

void endRegion()
{
    if (!gEnabled || gCurrentRegion < 0)
        return;

    Frame& frame = gFrames[gWriteFrame];
    Region& region = frame.regions[gCurrentRegion];
    glQueryCounter(region.end, GL_TIMESTAMP);
    region.active = false;
    gCurrentRegion = -1;
}

void endFrame()
{
    if (!gEnabled || !gInitialized)
        return;

    if (gCurrentRegion >= 0)
        endRegion();

    gFrames[gWriteFrame].submitted = true;
    gWriteFrame = (gWriteFrame + 1) % kRingSize;
}

void flushCompleted()
{
    if (!gEnabled || !gInitialized)
        return;

    const int readFrame = (gWriteFrame + 1) % kRingSize;
    Frame& frame = gFrames[readFrame];
    if (!frame.submitted)
        return;

    gLastCompletedFrameMs = 0.0;
    for (int i = 0; i < frame.count; ++i) {
        Region& region = frame.regions[i];
        GLuint available = 0;
        glGetQueryObjectuiv(region.end, GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available)
            continue;

        GLuint64 beginNs = 0;
        GLuint64 endNs = 0;
        glGetQueryObjectui64v(region.begin, GL_QUERY_RESULT, &beginNs);
        glGetQueryObjectui64v(region.end, GL_QUERY_RESULT, &endNs);
        if (endNs >= beginNs) {
            const double elapsedMs = static_cast<double>(endNs - beginNs) / 1000000.0;
            gLastCompletedFrameMs += elapsedMs;
            if (region.name && std::strcmp(region.name, "GPU::World") == 0)
                gLastCompletedWorldMs = elapsedMs;
            if (region.name && std::strcmp(region.name, "GPU::Shadows") == 0)
                gLastCompletedShadowMs = elapsedMs;
            Debug::logThrottled(Debug::Category::General,
                region.name ? region.name : "gpu", 0.25f,
                "[PERF][GPU] %s=%.3fms\n",
                region.name ? region.name : "unknown",
                elapsedMs);
        }
    }
    frame.submitted = false;
}
}
