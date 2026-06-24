#include "gui/gui-editor.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <algorithm>
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"

void GuiEditor::renderSelectionHandles(const GuiElement& elem)
{
    float sx = uiScaleX(elem.x), sy = uiScaleY(elem.y);
    float sw = uiScaleX(elem.w), sh = uiScaleY(elem.h);
    float hs = 6.0f;
    glm::vec4 oc = mHasOverlap
        ? glm::vec4(1,0,0,1) : glm::vec4(0,1,0.2f,1);
    uiDrawRectOutline({sx - 2, sy - 2, sw + 4, sh + 4}, oc, "gui-sel");
    glm::vec4 hc(1,1,0.3f,1);
    uiDrawRect({sx - hs, sy - hs, hs * 2, hs * 2}, hc, "gui-h");
    uiDrawRect({sx + sw - hs, sy - hs, hs * 2, hs * 2}, hc, "gui-h");
    uiDrawRect({sx - hs, sy + sh - hs, hs * 2, hs * 2}, hc, "gui-h");
    uiDrawRect({sx + sw - hs, sy + sh - hs, hs * 2, hs * 2}, hc, "gui-h");
}

void GuiEditor::renderPropertyPanel(GLFWwindow* win, const GuiElement& elem)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    auto dsx = [&](float x) { return cs.designToScreenX(x); };
    auto dsy = [&](float y) { return cs.designToScreenY(y); };

    struct SliderDef { const char* label; float val; float mn; float mx; };
    auto rc = [&](const std::vector<float>& c, int i) {
        return (int)c.size() > i ? c[i] : (i < 3 ? 0.0f : 1.0f);
    };
    SliderDef sliders[] = {
        {"X",      elem.x, 0, 1920},
        {"Y",      elem.y, 0, 1080},
        {"W",      elem.w, 10, 800},
        {"H",      elem.h, 10, 600},
        {"FontSz", elem.fontSize, 0, 2},
        {"T-R",    rc(elem.textColor, 0), 0, 1},
        {"T-G",    rc(elem.textColor, 1), 0, 1},
        {"T-B",    rc(elem.textColor, 2), 0, 1},
        {"T-A",    rc(elem.textColor, 3), 0, 1},
        {"B-R",    rc(elem.backgroundColor, 0), 0, 1},
        {"B-G",    rc(elem.backgroundColor, 1), 0, 1},
        {"B-B",    rc(elem.backgroundColor, 2), 0, 1},
        {"B-A",    rc(elem.backgroundColor, 3), 0, 1},
        {"Opacity", elem.opacity, 0, 1},
    };
    const int numSliders = sizeof(sliders) / sizeof(sliders[0]);

    float panelH = 36.0f + numSliders * PP_ROW_H + 80.0f;
    UIRect pbg = cs.designToScreen({PP_X, PP_Y, PP_W, panelH});
    uiDrawRect(pbg, {0.08f, 0.08f, 0.12f, 0.92f}, "gui-prop");
    uiDrawRectOutline(pbg, {0.3f, 0.3f, 0.4f, 0.8f}, "gui-prop-border");

    char title[128];
    snprintf(title, sizeof(title), "PROPERTIES: %s (%s)",
             elem.id.c_str(), elem.type.c_str());
    uiDrawText(title, dsx(PP_X + 8), dsy(PP_Y + 4), 0.30f,
               {0.4f, 1.0f, 0.6f, 1.0f});
    uiDrawRect({dsx(PP_X + 4), dsy(PP_Y + 28), dsx(PP_W - 8), 1},
               {0.3f, 0.3f, 0.5f, 0.6f}, "gui-p-div");

    auto drawSwatch = [&](float dx, float dy, float r, float g, float b, float a) {
        float sx = dsx(dx), sy = dsy(dy);
        float sw = dsx(14.0f), sh = dsy(12.0f);
        uiDrawRect({sx, sy, sw, sh}, {r, g, b, a}, "swatch");
        uiDrawRectOutline({sx, sy, sw, sh}, {0.5f, 0.5f, 0.5f, 0.8f}, "swatch-border");
    };

    for (int i = 0; i < numSliders; ++i) {
        float ry = PP_Y + 34.0f + i * PP_ROW_H;

        if (i >= 5 && i <= 8) {
            float swy = ry + (PP_ROW_H - 12.0f) * 0.5f;
            drawSwatch(PP_X + 8, swy,
                       sliders[5].val, sliders[6].val,
                       sliders[7].val, sliders[8].val);
        } else if (i >= 9 && i <= 12) {
            float swy = ry + (PP_ROW_H - 12.0f) * 0.5f;
            drawSwatch(PP_X + 8, swy,
                       sliders[9].val, sliders[10].val,
                       sliders[11].val, sliders[12].val);
        }

        float labelX = (i >= 5 && i <= 12) ? PP_LABEL_X + 6.0f : PP_LABEL_X;
        uiDrawText(sliders[i].label, dsx(labelX), dsy(ry), 0.26f,
                   {0.7f, 0.8f, 0.9f, 1.0f});

        float tsx = dsx(PP_TRACK_X), tsy = dsy(ry);
        float tsw = dsx(PP_TRACK_W), tsh = dsy(PP_ROW_H);
        uiDrawRect({tsx, tsy, tsw, tsh}, {0.15f, 0.15f, 0.2f, 1.0f}, "st");

        float t = (sliders[i].mx > sliders[i].mn)
            ? std::clamp((sliders[i].val - sliders[i].mn)
                        / (sliders[i].mx - sliders[i].mn), 0.0f, 1.0f)
            : 0.0f;
        uiDrawRect({tsx, tsy, tsw * t, tsh}, {0.25f, 0.6f, 0.9f, 1.0f}, "sf");

        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", sliders[i].val);
        uiDrawText(buf, dsx(PP_VAL_X), dsy(ry), 0.26f,
                   {0.9f, 0.9f, 0.5f, 1.0f});
    }

    float toy = PP_Y + 34.0f + numSliders * PP_ROW_H + 4;
    GuiLayout& propLayout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    {
        GuiElement visBtn;
        visBtn.type = "button";
        visBtn.text = elem.visible ? "VISIBLE" : "HIDDEN";
        visBtn.textColor = std::vector<float>{1,1,1,1};
        if (elem.visible)
            visBtn.backgroundColor = std::vector<float>{0.2f,0.7f,0.3f,1};
        else
            visBtn.backgroundColor = std::vector<float>{0.3f,0.2f,0.2f,1};
        UIRect visRect = {PP_X + 8, toy, 70, 20};
        if (drawGuiElement(win, visBtn, nullptr, &visRect).clicked) {
            GuiElement* e = const_cast<GuiElement*>(propLayout.get(mSelectedId));
            if (e) { e->visible = !e->visible; propLayout.setElement(*e); markEdited(); }
        }
    }
    {
        GuiElement enBtn;
        enBtn.type = "button";
        enBtn.text = elem.enabled ? "ENABLED" : "DISABLED";
        enBtn.textColor = std::vector<float>{1,1,1,1};
        if (elem.enabled)
            enBtn.backgroundColor = std::vector<float>{0.2f,0.7f,0.3f,1};
        else
            enBtn.backgroundColor = std::vector<float>{0.3f,0.2f,0.2f,1};
        UIRect enRect = {PP_X + 86, toy, 80, 20};
        if (drawGuiElement(win, enBtn, nullptr, &enRect).clicked) {
            GuiElement* e = const_cast<GuiElement*>(propLayout.get(mSelectedId));
            if (e) { e->enabled = !e->enabled; propLayout.setElement(*e); markEdited(); }
        }
    }

    char info[128];
    snprintf(info, sizeof(info), "Anchor: %s/%s  Rot: %.0f",
             elem.anchorX.c_str(), elem.anchorY.c_str(), elem.rotation);
    uiDrawText(info, dsx(PP_X + 8), dsy(toy + PP_ROW_H), 0.24f,
               {0.5f, 0.6f, 0.8f, 1.0f});

    GuiLayout& layerLayout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    float lyrY = toy + PP_ROW_H * 2 + 4;
    uiDrawText("Layer:", dsx(PP_X + 8), dsy(lyrY), 0.24f, {0.7f, 0.8f, 0.9f, 1.0f});
    char layerBuf[16];
    snprintf(layerBuf, sizeof(layerBuf), "%d", elem.layer);
    uiDrawText(layerBuf, dsx(PP_X + 56), dsy(lyrY), 0.24f, {1,1,1,1});
    GuiElement layerBtn;
    layerBtn.type = "button";
    layerBtn.textColor = {1,1,1,1};
    UIRect layerDownRect = {PP_X + 80, lyrY, 24, 20};
    layerBtn.text = "-";
    layerBtn.backgroundColor = {0.3f, 0.2f, 0.2f, 1.0f};
    if (drawGuiElement(win, layerBtn, nullptr, &layerDownRect).clicked) {
        GuiElement* e = const_cast<GuiElement*>(layerLayout.get(mSelectedId));
        if (e) { e->layer = std::max(-10, e->layer - 1); layerLayout.setElement(*e); markEdited(); }
    }
    UIRect layerUpRect = {PP_X + 108, lyrY, 24, 20};
    layerBtn.text = "+";
    layerBtn.backgroundColor = {0.2f, 0.3f, 0.2f, 1.0f};
    if (drawGuiElement(win, layerBtn, nullptr, &layerUpRect).clicked) {
        GuiElement* e = const_cast<GuiElement*>(layerLayout.get(mSelectedId));
        if (e) { e->layer = std::min(10, e->layer + 1); layerLayout.setElement(*e); markEdited(); }
    }

    {
        const char* displayText = mEditingText ? mTextEditBuffer.c_str() : elem.text.c_str();
        char textBuf[256];
        if (mEditingText) {
            bool cursorOn = (int)(glfwGetTime() * 2) % 2 == 0;
            snprintf(textBuf, sizeof(textBuf), "%s%s", displayText, cursorOn ? "|" : " ");
            uiDrawText(textBuf, dsx(PP_X + 8), dsy(toy + PP_ROW_H * 2),
                       0.24f, {0.3f, 1.0f, 0.5f, 1.0f});
        } else if (!elem.text.empty()) {
            uiDrawText(displayText, dsx(PP_X + 8), dsy(toy + PP_ROW_H * 2),
                       0.24f, {0.8f, 0.8f, 0.5f, 1.0f});
        }
        if (mEditingText) {
            uiDrawText("[EDITING TEXT - Enter=done Esc=cancel]",
                       dsx(PP_X + 8), dsy(toy + PP_ROW_H * 3),
                       0.22f, {0.5f, 1.0f, 0.5f, 1.0f});
        }
    }
}

void GuiEditor::renderColorPicker()
{
    if (!mColorPickerOpen) return;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    float palX = PP_X + PP_W + 8.0f;
    float palY = PP_Y + 100.0f;
    float swatchSize = 22.0f;
    float gap = 2.0f;
    int cols = 4;
    float palW = cols * (swatchSize + gap) + 6.0f;
    int numColors = 12;
    float palH = ((numColors + cols - 1) / cols) * (swatchSize + gap) + 6.0f;

    const float presetColors[12][3] = {
        {1,1,1}, {0.8f,0.8f,0.8f}, {0.5f,0.5f,0.5f}, {0,0,0},
        {1,0,0}, {1,0.5f,0}, {1,1,0}, {0,1,0},
        {0,0.5f,1}, {0,0,1}, {0.5f,0,0.5f}, {1,0,1}
    };

    UIRect palBg = cs.designToScreen({palX, palY, palW, palH});
    uiDrawRect(palBg, {0.1f, 0.1f, 0.14f, 0.95f}, "color-pal");
    uiDrawRectOutline(palBg, {0.4f, 0.4f, 0.6f, 0.9f}, "color-pal-border");

    char closeLabel[16];
    snprintf(closeLabel, sizeof(closeLabel), "X");
    float clX = cs.designToScreenX(palX + palW - 18.0f);
    float clY = cs.designToScreenY(palY + 2);
    uiDrawText(closeLabel, clX, clY, 0.28f, {1,0.3f,0.3f,1});

    for (int i = 0; i < numColors; ++i) {
        int row = i / cols;
        int col = i % cols;
        float sx = cs.designToScreenX(palX + 3.0f + col * (swatchSize + gap));
        float sy = cs.designToScreenY(palY + 3.0f + row * (swatchSize + gap));
        float ss = cs.designToScreenX(swatchSize);
        uiDrawRect({sx, sy, ss, ss},
                   {presetColors[i][0], presetColors[i][1], presetColors[i][2], 1.0f},
                   "pal-color");
        uiDrawRectOutline({sx, sy, ss, ss}, {0.3f, 0.3f, 0.3f, 0.7f}, "pal-border");
    }

    double mx, my; glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
    double fbx, fby; cs.cursorWindowToScreen(mx, my, fbx, fby);
    double dx = cs.screenToDesignX((float)fbx);
    double dy = cs.screenToDesignY((float)fby);
    bool mouseDown = glfwGetMouseButton(glfwGetCurrentContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    static bool prevDown = false;
    bool pressed = mouseDown && !prevDown;
    prevDown = mouseDown;

    if (pressed) {
        bool hit = (dx >= palX && dx <= palX + palW && dy >= palY && dy <= palY + palH);
        if (!hit) {
            mColorPickerOpen = false;
            return;
        }
        for (int i = 0; i < numColors; ++i) {
            int row = i / cols;
            int col = i % cols;
            float cx = palX + 3.0f + col * (swatchSize + gap);
            float cy = palY + 3.0f + row * (swatchSize + gap);
            if (dx >= cx && dx <= cx + swatchSize && dy >= cy && dy <= cy + swatchSize) {
                if (!mSelectedId.empty() && !mActiveLayoutFile.empty() && mColorPickerTarget >= 0) {
                    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
                    GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
                    if (elem) {
                        int idx = mColorPickerTarget;
                        auto setCol = [&](std::vector<float>& c, float r, float g, float b) {
                            if (c.size() < 4) c.resize(4, 1.0f);
                            c[0] = r; c[1] = g; c[2] = b;
                        };
                        if (idx >= 5 && idx <= 8) setCol(elem->textColor, presetColors[i][0], presetColors[i][1], presetColors[i][2]);
                        else if (idx >= 9 && idx <= 12) setCol(elem->backgroundColor, presetColors[i][0], presetColors[i][1], presetColors[i][2]);
                        layout.setElement(*elem);
                        markEdited();
                    }
                }
                mColorPickerOpen = false;
                return;
            }
        }
        if (dx >= palX + palW - 18.0f && dx <= palX + palW &&
            dy >= palY && dy <= palY + 20.0f) {
            mColorPickerOpen = false;
        }
    }
}

void GuiEditor::renderSnapGuides()
{
    if (mSnapGuides.empty()) return;
    for (const auto& g : mSnapGuides) {
        if (g.vertical) {
            float sx = uiScaleX(g.pos);
            uiDrawRect({sx - 1, 0, 2, uiScreenH()}, {0.2f, 0.6f, 1.0f, 0.5f}, "guide");
        } else {
            float sy = uiScaleY(g.pos);
            uiDrawRect({0, sy - 1, uiScreenW(), 2}, {0.2f, 0.6f, 1.0f, 0.5f}, "guide");
        }
    }
}
