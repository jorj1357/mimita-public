#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"
#include "gui/gui-media.h"
#include "gui/gui-coord.h"
#include "gui/font-stuff/font-loader.h"

using namespace UISys;

#include <cstdio>

float uiMeasureText(const char* text, float scale)
{
    float w = 0.0f;
    for (const char* p = text; *p; ++p)
    {
        Glyph g{};
        if (fontGetGlyph((unsigned char)*p, g))
            w += g.xadvance * scale;
    }
    return w;
}

float uiScreenW()
{
    return GuiCoordinateSystem::instance().screenW();
}

float uiScreenH()
{
    return GuiCoordinateSystem::instance().screenH();
}

float uiScaleX(float px)
{
    return GuiCoordinateSystem::instance().designToScreenX(px);
}

float uiScaleY(float px)
{
    return GuiCoordinateSystem::instance().designToScreenY(px);
}

UIRect uiCentered(float w, float h, float y)
{
    return {
        uiScreenW() * 0.5f - w * 0.5f,
        y,
        w,
        h
    };
}

UIRect uiRow(
    float x,
    float& y,
    float w,
    float h,
    float gap
)
{
    UIRect r{x, y, w, h};
    y += h + gap;
    return r;
}

void uiRenderFrameDebugOverlay(GLFWwindow* win, const char* activeScene, bool worldPassRan)
{
    (void)win;
    if (!gDebug) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "FPS/FRAME UI PASS OK | scene=%s worldPass=%s framebuffer=%dx%d drawCalls=%d widgets=%d",
             activeScene, worldPassRan ? "ran" : "skipped", gFbW, gFbH, gDrawCalls, gWidgets);
    uiDrawRect({10, 10, 760, 54}, {0.0f, 0.0f, 0.0f, 0.62f}, "debug-overlay-bg");
    uiDrawText(buf, 18, 24, 0.32f, {0.55f, 1.0f, 0.65f, 1.0f});
}
