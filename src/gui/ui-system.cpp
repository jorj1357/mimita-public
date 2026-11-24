#include "gui/ui-system.h"
#include "input/input-router.h"
#include "game/window-bridge-module.h"
#include "gui/ui-system-internal.h"
#include "render/render-command.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "renderer/renderer.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "gui/gui-media.h"
#include "gui/gui-coord.h"
#include "gui/ui-tooltip.h"

#include "audio/audio.h"

namespace UISys {
GLFWwindow* gWindow = nullptr;
std::uint32_t gProgram = 0;
std::uint32_t gVao = 0;
std::uint32_t gVbo = 0;
std::int32_t gScreenLoc = -1;
std::int32_t gColorLoc = -1;
std::int32_t gUseTexLoc = -1;
std::int32_t gTexLoc = -1;
std::int32_t gImageTexLoc = -1;
int gFbW = 1;
int gFbH = 1;
bool gDebug = false;
bool gMousePrev = false;
bool gMouseDown = false;
bool gMouseClickEdge = false;
int gMouseClickEdgeFramesRemaining = 0;
bool gUiEditMode = false;
int gFrame = 0;
int gDrawCalls = 0;
int gWidgets = 0;
std::vector<std::string> gWarnings;
std::vector<UITrackedWidget> gTrackedWidgets;
std::string gHoverOwnerKey;
std::string gPrevHoverOwnerKey;
bool gOverlapDebugEnabled = false;
bool gCoordDebug = false;
double gScrollYOffset = 0.0;
bool gDropdownModalActive = false;
bool gOwnsHoverInput = false;
std::vector<UIVertex> gBatchVertices;
}

extern Renderer* gRenderer;

using namespace UISys;

bool uiCanPlayUISound() {
    if (!mimita::game::hostWindowFocused()) return false;
    if (mimita::game::hostWindowIconified()) return false;
    if (mimita::game::hostCursorMode() == 2) return false;
    return true;
}

void uiInit(GLFWwindow* win)
{
    gWindow = win;
    printf("[UI] Initializing UI system\n");
    printf("[UI] Using built-in rectangle/vector fallback font; asset fonts are optional\n");
    ensureProgram();
}

void uiBeginFrame(GLFWwindow* win, const char* passName, bool ownsHoverInput,
                  bool useOutputFramebuffer)
{
    static bool mouseEdgeLossReportedForPress = false;
    gWindow = win;
    gOwnsHoverInput = ownsHoverInput;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    mimita::game::hostFramebufferSize(framebufferWidth, framebufferHeight);
    if (gRenderer && gRenderer->stretchesWholeFrame() && !useOutputFramebuffer) {
        framebufferWidth = gRenderer->sceneWidth;
        framebufferHeight = gRenderer->sceneHeight;
    }
    if (framebufferWidth <= 0 && gRenderer) framebufferWidth = gRenderer->width;
    if (framebufferHeight <= 0 && gRenderer) framebufferHeight = gRenderer->height;
    gFbW = framebufferWidth > 0 ? framebufferWidth : 1;
    gFbH = framebufferHeight > 0 ? framebufferHeight : 1;
    if (useOutputFramebuffer && passName &&
        std::strcmp(passName, "game-debug-overlay") == 0) {
        static bool postFxPresentationLogged = false;
        if (!postFxPresentationLogged) {
            postFxPresentationLogged = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Gui, StructuredLevel::Important,
                "ui.postfx.presentation", "UI",
                "HUD drawn after PostFX at output resolution",
                0,
                nlohmann::json{{"pass_name", passName},
                               {"framebuffer_width", gFbW},
                               {"framebuffer_height", gFbH},
                               {"use_output_framebuffer", true}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    {
        static std::uint64_t lastFbLogMs = 0;
        static int lastFbW = -1;
        static int lastFbH = -1;
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if ((framebufferWidth <= 0 || framebufferHeight <= 0 ||
             gFbW != lastFbW || gFbH != lastFbH) &&
            static_cast<std::uint64_t>(nowMs) - lastFbLogMs >= 1000) {
            lastFbLogMs = static_cast<std::uint64_t>(nowMs);
            lastFbW = gFbW;
            lastFbH = gFbH;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Gui, StructuredLevel::Important,
                "ui.framebuffer-size", "UI",
                "UI presentation size resolved for the current pass", 0,
                nlohmann::json{
                    {"pass_name", passName ? passName : "unknown"},
                    {"framebuffer_width", framebufferWidth},
                    {"framebuffer_height", framebufferHeight},
                    {"resolved_width", gFbW},
                    {"resolved_height", gFbH},
                    {"renderer_width", gRenderer ? gRenderer->width : 0},
                    {"renderer_height", gRenderer ? gRenderer->height : 0},
                    {"window", win != nullptr}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    {
        int winW = 1, winH = 1;
        if (!mimita::game::hostWindowSize(winW, winH)) {
            winW = gFbW;
            winH = gFbH;
        }
        GuiCoordinateSystem::instance().update(gFbW, gFbH, winW, winH);
    }

    ++gFrame;
    gDrawCalls = 0;
    gWidgets = 0;
    gTrackedWidgets.clear();
    gBatchVertices.clear();
    if (gBatchVertices.capacity() == 0)
        gBatchVertices.reserve(8192);
    if (gOwnsHoverInput)
        gHoverOwnerKey.clear();
    if (gOwnsHoverInput)
        InputRouter::advanceAgentMouseClick();
    gMouseDown = InputRouter::mouseDown(0);
    // Mouse edge state belongs to the one interactive UI pass. Rendering-only
    // passes must observe the edge without consuming it before buttons run.
    if (gOwnsHoverInput) {
        if (gMouseDown && !gMousePrev)
            gMouseClickEdgeFramesRemaining = 2;
        else if (!gMouseDown)
            gMouseClickEdgeFramesRemaining = 0;
        gMouseClickEdge = gMouseDown &&
            gMouseClickEdgeFramesRemaining > 0;
    }

    if (!gMouseDown) {
        mouseEdgeLossReportedForPress = false;
    } else if (gOwnsHoverInput && gMousePrev &&
               !mouseEdgeLossReportedForPress &&
               StructuredLogger::instance().shouldLog(
                   StructuredCategory::Gui, StructuredLevel::Important)) {
        mouseEdgeLossReportedForPress = true;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Gui, StructuredLevel::Important,
            "ui.mouse_edge_rearmed", "UI_MOUSE",
            "an interactive UI pass observed a held press after an earlier pass advanced mouse history",
            0,
            nlohmann::json{{"pass_name", passName ? passName : "unknown"},
                           {"mouse_down", gMouseDown},
                           {"mouse_prev", gMousePrev},
                           {"click_edge", gMouseClickEdge},
                           {"frame", gFrame}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    if (gMouseDown != gMousePrev &&
        StructuredLogger::instance().shouldLog(
            StructuredCategory::Gui, StructuredLevel::Important)) {
        nlohmann::json fields = {
            {"pass_name", passName ? passName : "unknown"},
            {"owns_hover_input", gOwnsHoverInput},
            {"mouse_down", gMouseDown},
            {"mouse_prev", gMousePrev},
            {"click_edge", gMouseClickEdge},
            {"cursor_mode", mimita::game::hostCursorMode()},
            {"transition", gMouseDown ? "press" : "release"}
        };
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Gui, StructuredLevel::Important,
            "ui.mouse_transition", "UI_MOUSE",
            "UI mouse button transition observed at a UI pass boundary", 0,
            fields, __FILE__, __LINE__, __FUNCTION__);
    }

    namespace r = mimita::game::render;
    r::viewport(0, 0, gFbW, gFbH);
    r::disable(r::Capability::DepthTest);
    r::enable(r::Capability::Blend);
    r::blendFunc(r::BlendFactor::SrcAlpha, r::BlendFactor::OneMinusSrcAlpha);
    r::disable(r::Capability::CullFace);
    ensureProgram();
    if (gProgram)
        r::useProgram(gProgram);

    if (gFrame % 120 == 1)
        Debug::logThrottled(Debug::Category::Render, "ui-frame", DebugConfig::PRINT_INTERVAL, "[UI] Rendering UI frame %d pass=%s framebuffer=%dx%d\n", gFrame, passName ? passName : "unknown", gFbW, gFbH);
}

static void drawOverlapDebug()
{
    if (!gOverlapDebugEnabled || gTrackedWidgets.empty()) return;

    for (size_t i = 0; i < gTrackedWidgets.size(); ++i)
    {
        const UITrackedWidget& a = gTrackedWidgets[i];
        for (size_t j = i + 1; j < gTrackedWidgets.size(); ++j)
        {
            const UITrackedWidget& b = gTrackedWidgets[j];
            const UIRect& ra = a.rect;
            const UIRect& rb = b.rect;

            if (ra.x < rb.x + rb.w && ra.x + ra.w > rb.x &&
                ra.y < rb.y + rb.h && ra.y + ra.h > rb.y)
            {
                float ox = std::max(ra.x, rb.x);
                float oy = std::max(ra.y, rb.y);
                float ow = std::min(ra.x + ra.w, rb.x + rb.w) - ox;
                float oh = std::min(ra.y + ra.h, rb.y + rb.h) - oy;

                printf("[GUI OVERLAP] \"%s\" overlaps \"%s\"  overlap=(%.0f,%.0f,%.0f,%.0f)\n",
                       a.id.c_str(), b.id.c_str(), ox, oy, ow, oh);

                uiDrawRect({ox, oy, ow, oh}, {1.0f, 0.0f, 0.0f, 0.35f}, "overlap-debug");

                uiDrawRectOutline(ra, {1.0f, 0.0f, 0.0f, 0.8f}, "overlap-widget");
                uiDrawRectOutline(rb, {1.0f, 0.0f, 0.0f, 0.8f}, "overlap-widget");
            }
        }
    }
}

void uiEndFrame()
{
    if (gOwnsHoverInput && gHoverOwnerKey != gPrevHoverOwnerKey)
    {
        if (!gPrevHoverOwnerKey.empty())
        {
            printf("[UI HOVER EXIT] id=%s\n", gPrevHoverOwnerKey.c_str());
        }
        if (!gHoverOwnerKey.empty())
        {
            printf("[UI HOVER ENTER] id=%s\n", gHoverOwnerKey.c_str());
            if (uiCanPlayUISound()) {
                playMenuHover();
            }
        }
        gPrevHoverOwnerKey = gHoverOwnerKey;
    }

    drawOverlapDebug();

    if (gCoordDebug && !gTrackedWidgets.empty())
    {
        GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
        double mx, my;
        InputRouter::cursorPos(gWindow, &mx, &my);
        double fbx, fby;
        cs.cursorWindowToScreen(mx, my, fbx, fby);

        const UITrackedWidget* hovered = nullptr;
        for (const auto& w : gTrackedWidgets) {
            UIRect fbR = cs.designToScreen(w.rect);
            if (pointIn(fbx, fby, fbR)) {
                hovered = &w;
            }
        }

        float x = uiScreenW() - 380.0f;
        float y = 80.0f;
        float lineH = 20.0f;

        uiDrawRect({x - 8, y - 8, 380, hovered ? lineH * 7 + 16 : lineH * 4 + 16},
                   {0.05f, 0.05f, 0.1f, 0.85f}, "coord-debug-bg");

        const UITrackedWidget* sel = hovered ? hovered : (!gTrackedWidgets.empty() ? &gTrackedWidgets.back() : nullptr);

        if (sel) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Widget: %s", sel->id.c_str());
            uiDrawText(buf, x, y, 0.26f, {0.4f, 1.0f, 0.6f, 1.0f}); y += lineH;
            snprintf(buf, sizeof(buf), "Design: %.0f, %.0f  %.0fx%.0f",
                     sel->rect.x, sel->rect.y, sel->rect.w, sel->rect.h);
            uiDrawText(buf, x, y, 0.26f, {0.6f, 0.8f, 1.0f, 1.0f}); y += lineH;
            UIRect fb = cs.designToScreen(sel->rect);
            snprintf(buf, sizeof(buf), "Screen: %.0f, %.0f  %.0fx%.0f",
                     fb.x, fb.y, fb.w, fb.h);
            uiDrawText(buf, x, y, 0.26f, {0.6f, 0.8f, 1.0f, 1.0f}); y += lineH;
            snprintf(buf, sizeof(buf), "Mouse: %.0f, %.0f", fbx, fby);
            uiDrawText(buf, x, y, 0.26f, {0.9f, 0.9f, 0.5f, 1.0f}); y += lineH;
            snprintf(buf, sizeof(buf), "Hovered: %s", hovered ? "yes" : "no");
            uiDrawText(buf, x, y, 0.26f, hovered ? glm::vec4(0.3f,1.0f,0.3f,1) : glm::vec4(1.0f,0.3f,0.3f,1)); y += lineH;
            if (hovered) {
                float dx = (float)fbx - fb.x;
                float dy = (float)fby - fb.y;
                snprintf(buf, sizeof(buf), "Rel: %.0f, %.0f  (%.1f%%, %.1f%%)",
                         dx, dy, dx / fb.w * 100.0f, dy / fb.h * 100.0f);
                uiDrawText(buf, x, y, 0.26f, {0.8f, 0.9f, 1.0f, 1.0f}); y += lineH;
            }
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "No widgets this frame");
            uiDrawText(buf, x, y, 0.26f, {1.0f, 0.5f, 0.3f, 1.0f}); y += lineH;
            snprintf(buf, sizeof(buf), "Mouse: %.0f, %.0f", fbx, fby);
            uiDrawText(buf, x, y, 0.26f, {0.9f, 0.9f, 0.5f, 1.0f}); y += lineH;
        }
    }

    uiDrawTooltip();

    uiFlushBatch();

    if (gFrame % 120 == 1)
        Debug::logThrottled(Debug::Category::Render, "ui-frame-complete", DebugConfig::PRINT_INTERVAL, "[UI] Render pass complete drawCalls=%d widgets=%d warnings=%zu\n", gDrawCalls, gWidgets, gWarnings.size());
    if (gOwnsHoverInput)
        gMousePrev = gMouseDown;
    if (gOwnsHoverInput && gMouseClickEdgeFramesRemaining > 0 &&
        gMouseDown && gMousePrev) {
        --gMouseClickEdgeFramesRemaining;
        if (gMouseClickEdgeFramesRemaining == 0)
            gMouseClickEdge = false;
    }
    namespace r = mimita::game::render;
    r::useProgram(0);
    r::enable(r::Capability::DepthTest);
}

void uiSetDebug(bool enabled) { gDebug = enabled; }
void uiSetEditMode(bool enabled) { gUiEditMode = enabled; }
bool uiEditModeEnabled() { return gUiEditMode; }
bool uiDebugEnabled() { return gDebug; }
void uiSetOverlapDebug(bool enabled) { gOverlapDebugEnabled = enabled; }
bool uiOverlapDebugEnabled() { return gOverlapDebugEnabled; }

void uiSetCoordDebug(bool enabled) { gCoordDebug = enabled; }
bool uiCoordDebugEnabled() { return gCoordDebug; }

void uiTrackWidget(const char* id, UIRect designRect)
{
    if (id)
        gTrackedWidgets.push_back({id, designRect, false, false});
}

const std::vector<UITrackedWidget>& uiGetTrackedWidgets()
{
    return gTrackedWidgets;
}
