#include "debug/debug-diag.h"

#include <cstdio>
#include <cstring>
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

// Shared state from debug-diag.cpp
struct LogEntry {
    std::string category;
    std::string text;
    time_t timestamp;
};
extern LogEntry gLogBuffer[5000];
extern int gLogHead;
extern int gLogCount;

struct GlErrorEntry {
    std::string file;
    int line = 0;
    std::string op;
    GLenum error = 0;
    int frame = 0;
};
extern GlErrorEntry gGlErrors[256];
extern int gGlErrorHead;
extern int gGlErrorCount;

extern bool gRenderTraceEnabled;

namespace {

const char* glErrorName(GLenum err)
{
    switch (err) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_NO_ERROR: return "GL_NO_ERROR";
        default: return "UNKNOWN";
    }
}

static void cmdPostFXInfo(const std::vector<std::string>&)
{
    auto& t = Terminal::instance();
    t.addLog("=== POSTFX INFO ===");
    PostFX& pf = PostFX::instance();
    char buf[256];

    snprintf(buf, sizeof(buf), "FBO: %s (id=%u)", pf.hasFbo() ? "VALID" : "MISSING", pf.fboId());
    t.addLog(buf);
    snprintf(buf, sizeof(buf), "ColorTex: %s (id=%u)", pf.hasColorTex() ? "VALID" : "MISSING", pf.colorTexId());
    t.addLog(buf);
    snprintf(buf, sizeof(buf), "QuadVAO: %s (id=%u)", pf.hasQuadVao() ? "VALID" : "MISSING", pf.quadVaoId());
    t.addLog(buf);
    snprintf(buf, sizeof(buf), "Shader: %s (id=%u)", pf.hasPostShader() ? "VALID" : "MISSING", pf.postShaderId());
    t.addLog(buf);
    snprintf(buf, sizeof(buf), "Size: %dx%d", pf.fboWidth(), pf.fboHeight());
    t.addLog(buf);

    if (!pf.hasFbo()) t.addLog("  [FAIL] FBO not created \xe2\x80\x94 initFBO() was not called or failed");
    if (!pf.hasColorTex()) t.addLog("  [FAIL] Color texture not created");
    if (!pf.hasQuadVao()) t.addLog("  [FAIL] Quad VAO not created \xe2\x80\x94 PostFX::render() will not blit to screen");
    if (!pf.hasPostShader()) t.addLog("  [FAIL] Post shader not loaded \xe2\x80\x94 post.vert/post.frag compile issue");

    if (pf.hasFbo()) {
        glBindFramebuffer(GL_FRAMEBUFFER, pf.fboId());
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        snprintf(buf, sizeof(buf), "FBO Status: %s (0x%x)",
                 status == GL_FRAMEBUFFER_COMPLETE ? "COMPLETE" : "INCOMPLETE", status);
        t.addLog(buf);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            t.addLog("  [FAIL] FBO is incomplete \xe2\x80\x94 rendering into it produces undefined results");
        }
    }
}

static void cmdPostFXTestMagenta(const std::vector<std::string>&)
{
    PostFX& pf = PostFX::instance();
    if (!pf.hasFbo()) { Terminal::instance().addLog("[POSTFX] No FBO to clear"); return; }
    pf.requestMagentaTest();
    Terminal::instance().addLog("[POSTFX] Next frame will clear the FBO to MAGENTA after world rendering.");
}

static void cmdPostFXTestChecker(const std::vector<std::string>&)
{
    PostFX& pf = PostFX::instance();
    if (!pf.hasFbo()) { Terminal::instance().addLog("[POSTFX] No FBO"); return; }
    glBindFramebuffer(GL_FRAMEBUFFER, pf.fboId());
    int w = pf.fboWidth(), h = pf.fboHeight();
    std::vector<unsigned char> pixels(w * h * 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            bool white = ((x / 32) + (y / 32)) % 2 == 0;
            unsigned char c = white ? 255 : 0;
            int idx = (y * w + x) * 4;
            pixels[idx] = c; pixels[idx+1] = c; pixels[idx+2] = c; pixels[idx+3] = 255;
        }
    glBindTexture(GL_TEXTURE_2D, pf.colorTexId());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Terminal::instance().addLog("[POSTFX] FBO filled with checkerboard. If checkerboard visible, FBO\xe2\x86\x92screen works.");
}

static void cmdTextureList(const std::vector<std::string>&)
{
    auto& t = Terminal::instance();
    t.addLog("=== TEXTURES ===");
    size_t count = gTextures.map.size();
    char buf[128];
    snprintf(buf, sizeof(buf), "Total loaded: %zu", count);
    t.addLog(buf);
    for (const auto& pair : gTextures.map) {
        snprintf(buf, sizeof(buf), "  %s -> GL id=%u", pair.first.c_str(), pair.second);
        t.addLog(buf);
    }
}

static void cmdShaderValidate(const std::vector<std::string>&)
{
    Terminal::instance().addLog("=== SHADER VALIDATION ===");
    char buf[256];

    if (!gRenderer) { Terminal::instance().addLog("[ERROR] No renderer"); return; }
    snprintf(buf, sizeof(buf), "Main shader: id=%u", gRenderer->shaderProgram);
    Terminal::instance().addLog(buf);

    if (gRenderer->shaderProgram) {
        GLint status = 0;
        glGetProgramiv(gRenderer->shaderProgram, GL_LINK_STATUS, &status);
        snprintf(buf, sizeof(buf), "  Link status: %s", status ? "OK" : "FAILED");
        Terminal::instance().addLog(buf);
        if (!status) {
            char log[2048];
            glGetProgramInfoLog(gRenderer->shaderProgram, sizeof(log), nullptr, log);
            Terminal::instance().addLog(std::string("  Error: ") + log);
        }
    } else {
        Terminal::instance().addLog("  [FAIL] Main shader missing \xe2\x80\x94 basic.vert/basic.frag compile failed");
    }

    PostFX& pf = PostFX::instance();
    if (pf.hasPostShader()) {
        snprintf(buf, sizeof(buf), "PostFX shader: id=%u", pf.postShaderId());
        Terminal::instance().addLog(buf);
        GLint status = 0;
        glGetProgramiv(pf.postShaderId(), GL_LINK_STATUS, &status);
        snprintf(buf, sizeof(buf), "  Link status: %s", status ? "OK" : "FAILED");
        Terminal::instance().addLog(buf);
    } else {
        Terminal::instance().addLog("  [FAIL] PostFX shader missing \xe2\x80\x94 post.vert/post.frag compile failed");
    }
}

static void cmdResourceList(const std::vector<std::string>&)
{
    Terminal::instance().addLog("=== RESOURCES ===");
    Terminal::instance().addLog("Renderer: " + std::string(gRenderer ? "valid" : "NULL"));
    if (gRenderer) {
        char buf[128];
        snprintf(buf, sizeof(buf), "  Window: %p  Size: %dx%d  Shader: %u",
                 (void*)gRenderer->window, gRenderer->width, gRenderer->height,
                 gRenderer->shaderProgram);
        Terminal::instance().addLog(buf);
    }
}

static void cmdMapStats(const std::vector<std::string>&)
{
    Terminal::instance().addLog("=== MAP STATS ===");
    Terminal::instance().addLog("(Map stats require World reference \xe2\x80\x94 run from gameplay)");
}

static void cmdGlErrors(const std::vector<std::string>&)
{
    Terminal::instance().addLog("=== GL ERRORS (recent " + std::to_string(gGlErrorCount) + ") ===");
    if (gGlErrorCount == 0) {
        Terminal::instance().addLog("  No GL errors recorded.");
        return;
    }
    int start = (gGlErrorHead - gGlErrorCount + 256) % 256;
    for (int i = 0; i < gGlErrorCount && i < 50; i++) {
        int idx = (start + i) % 256;
        const auto& e = gGlErrors[idx];
        char buf[512];
        snprintf(buf, sizeof(buf), "  [%d] %s:%d  op=%s  error=%s(0x%x)",
                 e.frame, e.file.c_str(), e.line, e.op.c_str(),
                 glErrorName(e.error), e.error);
        Terminal::instance().addLog(buf);
    }
}

static void cmdLogShow(const std::vector<std::string>& args)
{
    std::string filter = args.empty() ? "" : args[0];
    int shown = 0;
    int start = (gLogHead - gLogCount + 5000) % 5000;
    for (int i = 0; i < gLogCount; i++) {
        int idx = (start + i) % 5000;
        const auto& e = gLogBuffer[idx];
        if (!filter.empty() && e.category != filter) continue;
        char buf[512];
        snprintf(buf, sizeof(buf), "[%s] %s", e.category.c_str(), e.text.c_str());
        Terminal::instance().addLog(buf);
        shown++;
        if (shown >= 100) {
            Terminal::instance().addLog("  ... (showing first 100, " +
                std::to_string(gLogCount) + " total)");
            break;
        }
    }
    if (shown == 0) Terminal::instance().addLog("  (no matching entries)");
}

static void cmdLogClear(const std::vector<std::string>&)
{
    gLogHead = 0;
    gLogCount = 0;
    Terminal::instance().addLog("[DIAG] Log cleared");
}

static void cmdLogSave(const std::vector<std::string>&)
{
    FILE* f = fopen("diag-log.txt", "w");
    if (!f) { Terminal::instance().addLog("[ERROR] Cannot write diag-log.txt"); return; }
    int start = (gLogHead - gLogCount + 5000) % 5000;
    for (int i = 0; i < gLogCount; i++) {
        int idx = (start + i) % 5000;
        const auto& e = gLogBuffer[idx];
        fprintf(f, "[%s] %s\n", e.category.c_str(), e.text.c_str());
    }
    fclose(f);
    char buf[128];
    snprintf(buf, sizeof(buf), "[DIAG] Saved %d entries to diag-log.txt", gLogCount);
    Terminal::instance().addLog(buf);
}

static void cmdDebugEverything(const std::vector<std::string>&)
{
    Terminal::instance().addLog("========================================");
    Terminal::instance().addLog("  DEBUG EVERYTHING");
    Terminal::instance().addLog("========================================");
    cmdGlErrors({});
    cmdPostFXInfo({});
    cmdTextureList({});
    cmdShaderValidate({});
    cmdResourceList({});
}

} // anonymous namespace

void registerDiagnosticCommands()
{
    auto& t = Terminal::instance();

    t.registerCommand({
        "debug_log", "Log a message to the diagnostic ring buffer",
        "debug_log <category> <message>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) { Terminal::instance().addLog("[ERROR] Usage: debug_log <category> <message>"); return; }
            std::string msg;
            for (size_t i = 1; i < args.size(); i++) {
                if (!msg.empty()) msg += " ";
                msg += args[i];
            }
            diagLog(args[0].c_str(), "%s", msg.c_str());
            Terminal::instance().addLog("[DIAG] Logged to category '" + args[0] + "'");
        }
    });

    t.registerCommand({
        "debug_log_show", "Show recent diagnostic log entries",
        "debug_log_show [category]",
        cmdLogShow
    });

    t.registerCommand({
        "debug_log_clear", "Clear diagnostic log buffer",
        "debug_log_clear",
        cmdLogClear
    });

    t.registerCommand({
        "debug_log_save", "Save diagnostic log to diag-log.txt",
        "debug_log_save",
        cmdLogSave
    });

    t.registerCommand({
        "gl_errors", "Show recent OpenGL errors",
        "gl_errors",
        cmdGlErrors
    });

    t.registerCommand({
        "postfx_info", "Show PostFX FBO, texture, shader, and quad status",
        "postfx_info",
        cmdPostFXInfo
    });
    t.registerCommand({
        "postfx_dump", "Dump PostFX framebuffer validity and dimensions",
        "postfx_dump",
        cmdPostFXInfo
    });
    t.registerCommand({
        "postfx_bypass", "Render the world directly to the backbuffer",
        "postfx_bypass <0|1>",
        [](const std::vector<std::string>& args) {
            PostFX& pf = PostFX::instance();
            const bool bypass = args.empty() ? !pf.bypass() : args[0] != "0";
            pf.setBypass(bypass);
            Terminal::instance().addLog(std::string("[POSTFX] bypass=") +
                (bypass ? "1 (direct backbuffer)" : "0 (FBO + PostFX)"));
        }
    });
    t.registerCommand({
        "postfx_test_magenta", "Fill FBO with magenta to test FBO\xe2\x86\x92screen pipeline",
        "postfx_test_magenta",
        cmdPostFXTestMagenta
    });
    t.registerCommand({
        "postfx_test_checker", "Fill FBO with checkerboard to test texture sampling",
        "postfx_test_checker",
        cmdPostFXTestChecker
    });
    t.registerCommand({
        "world_solid_red", "Bypass world texture and lighting with solid red",
        "world_solid_red <0|1>",
        [](const std::vector<std::string>& args) {
            const bool enabled = args.empty() ? !worldSolidRedDebug() : args[0] != "0";
            setWorldSolidRedDebug(enabled);
            Terminal::instance().addLog(std::string("[WORLD] solid red=") +
                (enabled ? "1" : "0"));
        }
    });

    t.registerCommand({
        "texture_list", "List loaded textures",
        "texture_list",
        cmdTextureList
    });

    t.registerCommand({
        "shader_validate", "Validate all shader compile/link status",
        "shader_validate",
        cmdShaderValidate
    });

    t.registerCommand({
        "render_trace", "Toggle verbose frame render trace logging",
        "render_trace [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { gRenderTraceEnabled = !gRenderTraceEnabled; }
            else { gRenderTraceEnabled = args[0] == "1"; }
            Terminal::instance().addLog(std::string("[RENDER TRACE] ") + (gRenderTraceEnabled ? "ON" : "OFF"));
        }
    });

    t.registerCommand({
        "resource_list", "List loaded engine resources",
        "resource_list",
        cmdResourceList
    });

    t.registerCommand({
        "map_stats", "Show map mesh statistics",
        "map_stats",
        cmdMapStats
    });

    t.registerCommand({
        "collision_state", "Show current collision state (pos, vel, grounded, contacts)",
        "collision_state",
        [](const std::vector<std::string>&) {
            if (!gpPlayer) {
                Terminal::instance().addLog("[ERROR] No player available");
                return;
            }
            Terminal::instance().addLog(collisionStateSummary(*gpPlayer));
        }
    });

    t.registerCommand({
        "collision_dump_frame", "Dump last frame's collision trace",
        "collision_dump_frame",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(collisionLastTraceSummary());
        }
    });

    t.registerCommand({
        "debug_everything", "Run all diagnostics and print one comprehensive report",
        "debug_everything",
        cmdDebugEverything
    });
}
