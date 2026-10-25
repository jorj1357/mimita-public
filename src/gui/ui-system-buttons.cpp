#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "gui/gui-coord.h"
#include "gui/font-stuff/font-loader.h"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

using namespace UISys;

namespace {
// Bounded runtime evidence for one physical press; gameplay behavior remains unchanged.
bool gButtonPressDiagnosticEmitted = false;
}

void debugWidget(const char* type, const char* name, UIRect r, bool hovered, bool pressed);

UIButtonState uiButton(GLFWwindow* win, const char* text, UIRect r, glm::vec4 color,
                        const char* id,
                        const glm::vec4* hoverColorOverride,
                        const glm::vec4* pressedColorOverride,
                        const char* hoverSound,
                        const char* clickSound,
                        float fontSize)
{
    ++gWidgets;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect fbR = cs.designToScreen(r);

    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double fbx = mx, fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);

    UIButtonState s;
    const bool rawHovered = pointIn(fbx, fby, fbR);
    s.pressed = rawHovered && gMouseDown;
    s.clicked = !gUiEditMode && rawHovered && gMouseClickEdge && !gDropdownModalActive;

    if (!gMouseDown)
        gButtonPressDiagnosticEmitted = false;

    const char* key = id ? id : text;

    s.hovered = rawHovered;
    if (rawHovered) {
        gHoverOwnerKey = key;
    }

    // Apply colors: custom hover/pressed overrides or default lighten/darken
    glm::vec4 c = color;
    if (s.pressed && pressedColorOverride) {
        c = *pressedColorOverride;
    } else if (s.hovered && hoverColorOverride) {
        c = *hoverColorOverride;
    } else {
        if (s.hovered) c += glm::vec4(0.14f, 0.14f, 0.14f, 0.0f);
        if (s.pressed) c *= glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
    }

    uiDrawRect(fbR, c, text);
    uiDrawRectOutline(fbR, {1.0f, 1.0f, 1.0f, 0.85f}, "button-border");

    // Use provided fontSize or auto-calculate from button height
    float textScale = fontSize > 0.0f ? fontSize : std::clamp(fbR.h / 110.0f, 0.38f, 1.2f);
    float textW = uiMeasureText(text, textScale);
    float textH = (float)fontLineHeight * textScale;
    float textX = fbR.x + (fbR.w - textW) * 0.5f;
    float textY = fbR.y + (fbR.h - textH) * 0.5f;
    uiDrawText(text, textX, textY, textScale, {1.0f, 1.0f, 1.0f, 1.0f});
    debugWidget("BUTTON", text, fbR, s.hovered, s.pressed);
    gTrackedWidgets.push_back({key, r, s.hovered, s.pressed});

    if (rawHovered && gMouseDown && !gButtonPressDiagnosticEmitted &&
        StructuredLogger::instance().shouldLog(
            StructuredCategory::Gui, StructuredLevel::Important)) {
        const char* rejectionReason = s.clicked ? "accepted" :
            (gUiEditMode ? "ui_edit_mode" :
             (!gMouseClickEdge ? "mouse_click_edge_missing" :
              (gDropdownModalActive ? "dropdown_modal_active" : "unknown")));
        nlohmann::json fields = {
            {"element_id", key ? key : ""},
            {"text", text ? text : ""},
            {"raw_hovered", rawHovered},
            {"pressed", s.pressed},
            {"clicked", s.clicked},
            {"mouse_down", gMouseDown},
            {"mouse_click_edge", gMouseClickEdge},
            {"ui_edit_mode", gUiEditMode},
            {"dropdown_modal_active", gDropdownModalActive},
            {"cursor_mode", glfwGetInputMode(win, GLFW_CURSOR)},
            {"decision", s.clicked ? "accepted" : "rejected"},
            {"rejection_reason", rejectionReason}
        };
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Gui, StructuredLevel::Important,
            "ui.button_input", "UI_BUTTON",
            "UI button press decision at the shared button owner", 0,
            fields, __FILE__, __LINE__, __FUNCTION__);
        gButtonPressDiagnosticEmitted = true;
    }

    if (s.clicked)
    {
        printf("[UI] button clicked: %s\n", text);
        if (clickSound && clickSound[0]) {
            printf("[UI] playing click sound: %s\n", clickSound);
        }
        if (uiCanPlayUISound()) {
            playMenuClick();
        }
    }

    if (s.hovered && hoverSound && hoverSound[0]) {
        printf("[UI] playing hover sound: %s\n", hoverSound);
    }

    return s;
}

bool uiCheckbox(GLFWwindow* win, const char* label, UIRect r, bool* value)
{
    UIButtonState s = uiButton(win, *value ? "ON" : "OFF", r, *value ? glm::vec4(0.2f,0.8f,0.35f,1) : glm::vec4(0.7f,0.25f,0.25f,1));
    uiDrawText(label, r.x + r.w + 14.0f, r.y + 10.0f, 0.42f, {0.88f,0.9f,0.94f,1});
    if (s.clicked) *value = !*value;
    debugWidget("CHECKBOX", label, r, s.hovered, s.pressed);
    return s.clicked;
}

bool uiSlider(GLFWwindow* win, const char* label, UIRect r, float* value, float minValue, float maxValue)
{
    ++gWidgets;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect fbR = cs.designToScreen(r);

    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double fbx = mx, fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);

    bool hovered = pointIn(fbx, fby, fbR);
    if (!gUiEditMode && hovered && gMouseDown) {
        float t = std::clamp((float(fbx) - fbR.x) / fbR.w, 0.0f, 1.0f);
        *value = minValue + t * (maxValue - minValue);
    }

    float t = (*value - minValue) / (maxValue - minValue);
    uiDrawRect(fbR, {0.12f,0.14f,0.18f,1}, "slider-track");
    uiDrawRect({fbR.x, fbR.y, fbR.w * t, fbR.h}, {0.25f,0.65f,0.95f,1}, "slider-fill");
    uiDrawRectOutline(fbR, {0.9f,0.9f,0.9f,0.9f}, "slider-border");
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %.1f", label, *value);
    uiDrawText(buf, fbR.x, fbR.y - 28.0f, 0.42f, {0.88f,0.9f,0.94f,1});
    debugWidget("SLIDER", label, fbR, hovered, hovered && gMouseDown);
    return !gUiEditMode && hovered && gMouseDown;
}

void uiPlaceholderImageButton(GLFWwindow* win, const char* label, UIRect r)
{
    UIButtonState s = uiButton(win, "IMG", r, {0.35f,0.24f,0.65f,1});
    uiDrawRect({r.x + 10, r.y + 10, r.w - 20, r.h - 20}, {0.95f,0.2f,0.85f,0.35f}, "missing-image");
    uiDrawText("[MISSING TEXTURE]", r.x + 8, r.y + r.h + 8, 0.35f, {1.0f,0.8f,0.2f,1});
    debugWidget("IMAGE_BUTTON", label, r, s.hovered, s.pressed);
}

namespace {

void beginScreenScrollArea(GLFWwindow* win, UIRect area, float contentHeight,
                           UIScrollState& scroll)
{
    uiFlushBatch();
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)area.x, (int)(UISys::gFbH - area.y - area.h),
              (int)area.w, (int)area.h);

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double fbx = mx;
    double fby = my;
    GuiCoordinateSystem::instance().cursorWindowToScreen(mx, my, fbx, fby);
    const bool inArea = fbx >= area.x && fbx <= area.x + area.w &&
                        fby >= area.y && fby <= area.y + area.h;
    if (inArea) {
        const double scrollDelta = UISys::gScrollYOffset;
        UISys::gScrollYOffset = 0.0;
        if (scrollDelta != 0.0)
            scroll.scrollY -= (float)(scrollDelta * 40.0f);
    }

    const float maxScroll = std::max(0.0f, contentHeight - area.h);
    scroll.scrollY = std::clamp(scroll.scrollY, 0.0f, maxScroll);
}

void endScreenScrollArea(UIRect area, float contentHeight, UIScrollState& scroll,
                         float scrollbarWidth, float scrollbarInset)
{
    uiFlushBatch();
    glDisable(GL_SCISSOR_TEST);

    const float maxScroll = std::max(0.0f, contentHeight - area.h);
    if (maxScroll <= 0.0f) return;

    const float sbW = std::max(4.0f, scrollbarWidth);
    const float sbX = area.x + std::max(0.0f, area.w - sbW - scrollbarInset);
    const float thumbH = std::clamp(area.h * (area.h / contentHeight),
        std::min(44.0f, area.h), area.h);
    const float thumbY = area.y + (scroll.scrollY / maxScroll) * (area.h - thumbH);

    uiDrawRect({sbX, area.y, sbW, area.h}, {0.1f, 0.12f, 0.16f, 0.55f}, "scroll-track");
    uiDrawRect({sbX, thumbY, sbW, thumbH}, {0.3f, 0.5f, 0.7f, 0.8f}, "scroll-thumb");
}

} // namespace

void uiBeginScrollArea(GLFWwindow* win, UIRect area, float contentHeight, UIScrollState& scroll)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    uiFlushBatch();
    const UIRect fbArea = cs.designToScreen(area);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)fbArea.x, (int)(UISys::gFbH - fbArea.y - fbArea.h),
              (int)fbArea.w, (int)fbArea.h);

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double fbx = mx;
    double fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);
    const bool inArea = fbx >= fbArea.x && fbx <= fbArea.x + fbArea.w &&
                        fby >= fbArea.y && fby <= fbArea.y + fbArea.h;
    if (inArea) {
        const double scrollDelta = UISys::gScrollYOffset;
        UISys::gScrollYOffset = 0.0;
        if (scrollDelta != 0.0)
            scroll.scrollY -= (float)(scrollDelta * 40.0f);
    }

    const float maxScroll = std::max(0.0f, contentHeight - area.h);
    scroll.scrollY = std::clamp(scroll.scrollY, 0.0f, maxScroll);
    cs.pushTranslate(scroll.scrollY);
}

void uiEndScrollArea(UIRect area, float contentHeight, UIScrollState& scroll)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    cs.popTranslate();

    uiFlushBatch();
    glDisable(GL_SCISSOR_TEST);

    const float maxScroll = std::max(0.0f, contentHeight - area.h);
    if (maxScroll <= 0.0f) return;

    const float sbW = 8.0f;
    const float sbX = area.x + area.w - sbW - 4.0f;
    const float thumbH = std::max(30.0f, area.h * (area.h / contentHeight));
    const float thumbY = area.y + (scroll.scrollY / maxScroll) * (area.h - thumbH);

    uiDrawRect(cs.designToScreen({sbX, area.y, sbW, area.h}),
        {0.1f, 0.12f, 0.16f, 0.5f}, "scroll-track");
    uiDrawRect(cs.designToScreen({sbX, thumbY, sbW, thumbH}),
        {0.3f, 0.5f, 0.7f, 0.7f}, "scroll-thumb");
}

void uiBeginScreenScrollArea(GLFWwindow* win, UIRect area, float contentHeight,
                             UIScrollState& scroll, float scrollbarWidth,
                             float scrollbarInset)
{
    (void)scrollbarWidth;
    (void)scrollbarInset;
    beginScreenScrollArea(win, area, contentHeight, scroll);
}

void uiEndScreenScrollArea(UIRect area, float contentHeight, UIScrollState& scroll,
                           float scrollbarWidth, float scrollbarInset)
{
    endScreenScrollArea(area, contentHeight, scroll, scrollbarWidth, scrollbarInset);
}
