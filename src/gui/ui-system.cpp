#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glad/glad.h>
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "gui/gui-media.h"
#include "gui/gui-coord.h"
#include "gui/ui-tooltip.h"

#include "audio/audio.h"

namespace UISys {
GLFWwindow* gWindow = nullptr;
GLuint gProgram = 0;
GLuint gVao = 0;
GLuint gVbo = 0;
GLint gScreenLoc = -1;
GLint gColorLoc = -1;
GLint gUseTexLoc = -1;
GLint gTexLoc = -1;
GLint gImageTexLoc = -1;
int gFbW = 1;
int gFbH = 1;
bool gDebug = false;
bool gMousePrev = false;
bool gMouseDown = false;
bool gMouseClickEdge = false;
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
std::vector<UIVertex> gBatchVertices;
}

using namespace UISys;

bool uiCanPlayUISound() {
    if (!gWindow) return false;
    if (glfwGetWindowAttrib(gWindow, GLFW_FOCUSED) == 0) return false;
    if (glfwGetWindowAttrib(gWindow, GLFW_ICONIFIED) != 0) return false;
    if (glfwGetInputMode(gWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) return false;
    return true;
}

void uiInit(GLFWwindow* win)
{
    gWindow = win;
    printf("[UI] Initializing UI system\n");
    printf("[UI] Using built-in rectangle/vector fallback font; asset fonts are optional\n");
    ensureProgram();
}

void uiBeginFrame(GLFWwindow* win, const char* passName)
{
    gWindow = win;
    glfwGetFramebufferSize(win, &gFbW, &gFbH);
    if (gFbW <= 0) gFbW = 1;
    if (gFbH <= 0) gFbH = 1;

    {
        int winW = 1, winH = 1;
        glfwGetWindowSize(win, &winW, &winH);
        GuiCoordinateSystem::instance().update(gFbW, gFbH, winW, winH);
    }

    ++gFrame;
    gDrawCalls = 0;
    gWidgets = 0;
    gTrackedWidgets.clear();
    gBatchVertices.clear();
    if (gBatchVertices.capacity() == 0)
        gBatchVertices.reserve(8192);
    gHoverOwnerKey.clear();
    gMouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    gMouseClickEdge = gMouseDown && !gMousePrev;

    MIMITA_GL_CLEAR_STAGE("uiBeginFrame");
    MIMITA_GL_CALL(glViewport(0, 0, gFbW, gFbH));
    MIMITA_GL_CALL(glDisable(GL_DEPTH_TEST));
    MIMITA_GL_CALL(glEnable(GL_BLEND));
    MIMITA_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    MIMITA_GL_CALL(glDisable(GL_CULL_FACE));
    ensureProgram();
    if (gProgram)
        MIMITA_GL_CALL(glUseProgram(gProgram));

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
    if (gHoverOwnerKey != gPrevHoverOwnerKey)
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
        glfwGetCursorPos(gWindow, &mx, &my);
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
    gMousePrev = gMouseDown;
    MIMITA_GL_CALL(glUseProgram(0));
    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));
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
