#include "debug-diag.h"

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <algorithm>
#include <array>

#include "devtools/terminal.h"
#include "render/post-fx.h"
#include "render/render-world.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "physics/movement/physics-collision.h"
#include "terminal/terminal-state.h"

extern Renderer* gRenderer;

// ============================================================
// Phase 1 — Global structured log ring buffer
// ============================================================

struct LogEntry {
    std::string category;
    std::string text;
    time_t timestamp;
};

constexpr int MAX_LOG = 5000;
LogEntry gLogBuffer[MAX_LOG];
int gLogHead = 0;
int gLogCount = 0;
int gLogFrame = 0;

struct GlErrorEntry {
    std::string file;
    int line = 0;
    std::string op;
    GLenum error = 0;
    int frame = 0;
};

constexpr int MAX_GL_ERRORS = 256;
GlErrorEntry gGlErrors[MAX_GL_ERRORS];
int gGlErrorHead = 0;
int gGlErrorCount = 0;

void diagLog(const char* category, const char* format, ...)
{
    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    LogEntry& e = gLogBuffer[gLogHead];
    e.category = category ? category : "";
    e.text = buf;
    e.timestamp = std::time(nullptr);
    gLogHead = (gLogHead + 1) % MAX_LOG;
    if (gLogCount < MAX_LOG) gLogCount++;
}

void diagLogGL(const char* file, int line, const char* op, GLenum error)
{
    GlErrorEntry& e = gGlErrors[gGlErrorHead];
    e.file = file ? file : "";
    e.line = line;
    e.op = op ? op : "";
    e.error = error;
    e.frame = gLogFrame;
    gGlErrorHead = (gGlErrorHead + 1) % MAX_GL_ERRORS;
    if (gGlErrorCount < MAX_GL_ERRORS) gGlErrorCount++;
}

void diagCheckGL(const char* file, int line, const char* op)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        diagLogGL(file, line, op, err);
        printf("[GL ERROR] file=%s line=%d op=%s error=0x%x\n", file, line, op, err);
    }
}

bool gRenderTraceEnabled = true;

// ============================================================
// Phase 6 — Render trace
// ============================================================
namespace {
float gRenderTraceTimer = 0.0f;
bool gRenderTraceSample = false;
std::array<bool, 9> gRenderStageReached{};
std::array<bool, 9> gRenderStageOk{};
size_t gWorldBatches = 0;
size_t gWorldVertices = 0;
size_t gWorldDrawCalls = 0;
size_t gPlayerDrawCalls = 0;
size_t gWeaponDrawCalls = 0;
size_t gEffectDrawCalls = 0;

} // anonymous namespace

void diagRenderFrameBegin(float dt)
{
    gRenderTraceTimer += dt;
    gRenderTraceSample = gRenderTraceEnabled && gRenderTraceTimer >= 1.0f;
    if (!gRenderTraceSample)
        return;

    gRenderTraceTimer = 0.0f;
    gRenderStageReached.fill(false);
    gRenderStageOk.fill(false);
    gWorldBatches = 0;
    gWorldVertices = 0;
    gWorldDrawCalls = 0;
    gPlayerDrawCalls = 0;
    gWeaponDrawCalls = 0;
    gEffectDrawCalls = 0;
}

bool diagRenderTraceSampling()
{
    return gRenderTraceSample;
}

void diagRenderStage(int stage)
{
    if (!gRenderTraceSample || stage < 1 || stage > 9)
        return;
    gRenderStageReached[(size_t)stage - 1] = true;
    gRenderStageOk[(size_t)stage - 1] = glGetError() == GL_NO_ERROR;
}

void diagRenderWorldCounts(size_t batches, size_t vertices, size_t drawCalls)
{
    if (!gRenderTraceSample) return;
    gWorldBatches = batches;
    gWorldVertices = vertices;
    gWorldDrawCalls = drawCalls;
}

void diagRenderCountPlayerDraw() { if (gRenderTraceSample) ++gPlayerDrawCalls; }
void diagRenderCountWeaponDraw() { if (gRenderTraceSample) ++gWeaponDrawCalls; }
void diagRenderCountEffectDraw() { if (gRenderTraceSample) ++gEffectDrawCalls; }

void diagRenderFrameEnd()
{
    if (!gRenderTraceSample) return;

    static const char* names[] = {
        "bind postfx fbo", "render world", "render players",
        "render weapons", "render effects", "unbind postfx fbo",
        "postfx render", "gui render", "swap buffers"
    };
    printf("\nFRAME\n\n");
    for (int i = 0; i < 9; ++i) {
        printf("%d. %s: PASS %s\n", i + 1, names[i],
            gRenderStageReached[i] && gRenderStageOk[i] ? "OK" : "FAILED");
    }
    printf("\nWorld batches: %zu\nWorld vertices: %zu\nWorld draw calls: %zu\n\n"
           "Player draw calls: %zu\nWeapon draw calls: %zu\nEffect draw calls: %zu\n",
           gWorldBatches, gWorldVertices, gWorldDrawCalls,
           gPlayerDrawCalls, gWeaponDrawCalls, gEffectDrawCalls);
    printf("Debug modes: master=%d wireframe=%d lightingOnly=%d uDebugView=%d\n",
           (int)DebugVis::masterEnabled(), (int)DebugVis::wireframe(),
           (int)DebugVis::lightingOnly(), DebugVis::shaderDebugView());
    gRenderTraceSample = false;
}

void diagRenderTrace(const char* stage)
{
    if (gRenderTraceEnabled) {
        printf("[RENDER TRACE] %s\n", stage);
    }
}

extern "C" void diagFrameTrace(const char* stage)
{
    diagRenderTrace(stage);
}
