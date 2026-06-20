#include "gui-editor.h"
#include "gui-coord.h"
#include "gui-layout.h"
#include "ui-system.h"

#include <cstdio>
#include <algorithm>
#include <cmath>

GuiEditor& GuiEditor::instance()
{
    static GuiEditor editor;
    return editor;
}

void GuiEditor::setEnabled(bool e)
{
    mEnabled = e;
    uiSetEditMode(e);
    if (!e) {
        mSelectedId.clear();
        mDragging = false;
        mResizing = false;
        mResizeCorner = -1;
        mSnapGuides.clear();
        GuiLayoutManager::instance().saveAll();
    }
}

void GuiEditor::setActiveLayout(const std::string& filePath)
{
    mActiveLayoutFile = filePath;
    if (!filePath.empty())
        GuiLayoutManager::instance().getLayout(filePath);
}

void GuiEditor::update(GLFWwindow* win)
{
    if (!mEnabled) return;
    autoSave();
    handleInput(win);
    handleKeyboard(win);
    renderOverlay(win);
}

void GuiEditor::autoSave()
{
    if (!GuiLayoutManager::instance().hasUnsaved()) return;
    double now = glfwGetTime();
    if (now - mLastEditTime >= AUTO_SAVE_DELAY) {
        GuiLayoutManager::instance().saveAll();
        mLastEditTime = now;
    }
}

void GuiEditor::markEdited()
{
    mLastEditTime = glfwGetTime();
}

// ── Value rounding ────────────────────────────────────────────────
float GuiEditor::roundValue(float val)
{
    float nearest = std::round(val);
    if (std::abs(val - nearest) <= 1.2f) return nearest;
    float nearest5 = std::round(val / 5.0f) * 5.0f;
    if (std::abs(val - nearest5) <= 3.0f) return nearest5;
    return val;
}

float GuiEditor::roundCoord(float val) { return roundValue(val); }

// ── Snap guides ──────────────────────────────────────────────────
void GuiEditor::computeSnapGuides(const GuiElement& elem)
{
    mSnapGuides.clear();
    mSnapGuides.push_back({960.0f, true});
    mSnapGuides.push_back({540.0f, false});
    mSnapGuides.push_back({0.0f, true});
    mSnapGuides.push_back({1920.0f, true});
    mSnapGuides.push_back({0.0f, false});
    mSnapGuides.push_back({1080.0f, false});

    if (mActiveLayoutFile.empty()) return;
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    for (const std::string& id : layout.elementIds()) {
        if (id == mSelectedId) continue;
        const GuiElement* other = layout.get(id);
        if (!other) continue;
        mSnapGuides.push_back({other->x, true});
        mSnapGuides.push_back({other->x + other->w, true});
        mSnapGuides.push_back({other->x + other->w * 0.5f, true});
        mSnapGuides.push_back({other->y, false});
        mSnapGuides.push_back({other->y + other->h, false});
        mSnapGuides.push_back({other->y + other->h * 0.5f, false});
    }
}

void GuiEditor::snapPosition(float& x, float& y, float w, float h) const
{
    const float threshold = 8.0f;
    float best = threshold * threshold;
    float sx = x, sy = y;
    for (const auto& g : mSnapGuides) {
        if (g.vertical) {
            for (float e : {x, x + w, x + w * 0.5f}) {
                float d = e - g.pos;
                if (d * d < best) { best = d * d; sx = x - d; }
            }
        } else {
            for (float e : {y, y + h, y + h * 0.5f}) {
                float d = e - g.pos;
                if (d * d < best) { best = d * d; sy = y - d; }
            }
        }
    }
    x = sx; y = sy;
}

// ── Overlap ──────────────────────────────────────────────────────
void GuiEditor::checkOverlaps()
{
    mHasOverlap = false;
    if (mActiveLayoutFile.empty() || mSelectedId.empty()) return;
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    const GuiElement* sel = layout.get(mSelectedId);
    if (!sel) return;
    float sx = uiScaleX(sel->x), sy = uiScaleY(sel->y);
    float sw = uiScaleX(sel->w), sh = uiScaleY(sel->h);
    for (const std::string& id : layout.elementIds()) {
        if (id == mSelectedId) continue;
        const GuiElement* e = layout.get(id);
        if (!e) continue;
        float ex = uiScaleX(e->x), ey = uiScaleY(e->y);
        if (sx < ex + uiScaleX(e->w) && sx + sw > ex &&
            sy < ey + uiScaleY(e->h) && sy + sh > ey) {
            mHasOverlap = true;
            printf("[GUI EDIT OVERLAP] \"%s\" overlaps \"%s\"\n",
                   mSelectedId.c_str(), id.c_str());
            return;
        }
    }
}

// ── Property panel constants ──────────────────────────────────────
static constexpr float PP_X = 1460.0f, PP_Y = 80.0f, PP_W = 430.0f;
static constexpr float PP_LABEL_X = 1470.0f;
static constexpr float PP_TRACK_X = 1560.0f, PP_TRACK_W = 230.0f;
static constexpr float PP_VAL_X = 1800.0f;
static constexpr float PP_ROW_H = 22.0f;

// ── Input ─────────────────────────────────────────────────────────
void GuiEditor::handleInput(GLFWwindow* win)
{
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    double fbx, fby;
    GuiCoordinateSystem::instance().cursorWindowToScreen(mx, my, fbx, fby);
    double dx = GuiCoordinateSystem::instance().screenToDesignX((float)fbx);
    double dy = GuiCoordinateSystem::instance().screenToDesignY((float)fby);

    bool mouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    static bool prevDown = false;
    bool pressed = mouseDown && !prevDown;
    bool released = !mouseDown && prevDown;
    prevDown = mouseDown;

    // Property panel slider dragging (14 flat sliders)
    static int dragSlider = -1;
    if (dragSlider >= 0) {
        if (mouseDown && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
            if (elem) {
                float t = std::clamp((float)(dx - PP_TRACK_X) / PP_TRACK_W, 0.0f, 1.0f);
                auto setR = [&](float& v, float mn, float mx) { v = mn + t * (mx - mn); };
                switch (dragSlider) {
                    case 0:  setR(elem->x, 0, 1920); break;
                    case 1:  setR(elem->y, 0, 1080); break;
                    case 2:  setR(elem->w, 10, 800); break;
                    case 3:  setR(elem->h, 10, 600); break;
                    case 4:  setR(elem->fontSize, 0, 2); break;
                    case 5:  if (!elem->textColor.empty()) setR(elem->textColor[0], 0, 1); break;
                    case 6:  if (elem->textColor.size() > 1) setR(elem->textColor[1], 0, 1); break;
                    case 7:  if (elem->textColor.size() > 2) setR(elem->textColor[2], 0, 1); break;
                    case 8:  if (elem->textColor.size() > 3) setR(elem->textColor[3], 0, 1); break;
                    case 9:  if (!elem->backgroundColor.empty()) setR(elem->backgroundColor[0], 0, 1); break;
                    case 10: if (elem->backgroundColor.size() > 1) setR(elem->backgroundColor[1], 0, 1); break;
                    case 11: if (elem->backgroundColor.size() > 2) setR(elem->backgroundColor[2], 0, 1); break;
                    case 12: if (elem->backgroundColor.size() > 3) setR(elem->backgroundColor[3], 0, 1); break;
                    case 13: setR(elem->opacity, 0, 1); break;
                }
                layout.setElement(*elem);
                markEdited();
            }
            return;
        } else {
            dragSlider = -1;
        }
    }

    // Start slider drag (14 flat sliders, indices 0-13)
    if (pressed && !mDragging && !mResizing) {
        const float sliderStartY = PP_Y + 34.0f;
        const int numSliders = 14;
        if (dx >= PP_TRACK_X && dx <= PP_TRACK_X + PP_TRACK_W &&
            dy >= sliderStartY && dy < sliderStartY + numSliders * PP_ROW_H) {
            int idx = (int)((dy - sliderStartY) / PP_ROW_H);
            if (idx >= 0 && idx < numSliders) {
                dragSlider = idx;
                return;
            }
        }
    }

    // Hierarchy click
    if (pressed && !mDragging && !mResizing) {
        const float hx = 12.0f, hy = 112.0f, hw = 218.0f;
        if (dx >= hx && dx <= hx + hw && dy >= hy && !mActiveLayoutFile.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            auto ids = layout.elementIds();
            int idx = (int)((dy - hy) / PP_ROW_H);
            if (idx >= 0 && idx < (int)ids.size()) {
                mSelectedId = ids[idx];
                mDragOffsetX = 0; mDragOffsetY = 0;
                printf("[GUI EDIT] hierarchy selected \"%s\"\n", mSelectedId.c_str());
                return;
            }
        }
    }

    // Resize corner detection
    if (pressed && !mDragging && !mResizing && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
        GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
        const GuiElement* elem = layout.get(mSelectedId);
        if (elem) {
            const float hr = 12.0f;
            float cx[4] = {elem->x, elem->x + elem->w, elem->x, elem->x + elem->w};
            float cy[4] = {elem->y, elem->y, elem->y + elem->h, elem->y + elem->h};
            for (int i = 0; i < 4; ++i) {
                if (std::abs((float)dx - cx[i]) <= hr && std::abs((float)dy - cy[i]) <= hr) {
                    mResizing = true;
                    mResizeCorner = i;
                    mResizeStartX = elem->x; mResizeStartY = elem->y;
                    mResizeStartW = elem->w; mResizeStartH = elem->h;
                    return;
                }
            }
        }
    }

    // Selection click
    if (pressed && !mDragging && !mResizing) {
        for (const auto& w : uiGetTrackedWidgets()) {
            double wx = w.rect.x, wy = w.rect.y, ww = w.rect.w, wh = w.rect.h;
            if (dx >= wx && dx <= wx + ww && dy >= wy && dy <= wy + wh) {
                mSelectedId = w.id;
                mDragOffsetX = (float)dx - wx;
                mDragOffsetY = (float)dy - wy;
                mDragging = true;
                mHasOverlap = false;
                printf("[GUI EDIT] selected \"%s\" (%.0f,%.0f) %.0fx%.0f\n",
                       w.id.c_str(), wx, wy, ww, wh);
                if (!mActiveLayoutFile.empty()) {
                    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
                    layout.set(mSelectedId, (float)wx, (float)wy, (float)ww, (float)wh);
                }
                checkOverlaps();
                return;
            }
        }
    }

    // Drag with snap
    if (mDragging) {
        if (mouseDown && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            const GuiElement* elem = layout.get(mSelectedId);
            if (elem) {
                float nx = (float)dx - mDragOffsetX;
                float ny = (float)dy - mDragOffsetY;
                computeSnapGuides(*elem);
                snapPosition(nx, ny, elem->w, elem->h);
                layout.set(mSelectedId, roundCoord(nx), roundCoord(ny),
                           roundValue(elem->w), roundValue(elem->h));
                checkOverlaps();
                markEdited();
            }
        } else {
            mDragging = false;
            mHasOverlap = false;
            mSnapGuides.clear();
        }
    }

    // Resize
    if (mResizing) {
        if (mouseDown && mResizeCorner >= 0 && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
            if (elem) {
                float ndx = (float)dx, ndy = (float)dy;
                float nw = mResizeStartW, nh = mResizeStartH;
                float nx = mResizeStartX, ny = mResizeStartY;
                switch (mResizeCorner) {
                    case 0:
                        nw = std::max(10.0f, mResizeStartW + (mResizeStartX - ndx));
                        nh = std::max(10.0f, mResizeStartH + (mResizeStartY - ndy));
                        nx = mResizeStartX + (mResizeStartW - nw);
                        ny = mResizeStartY + (mResizeStartH - nh);
                        break;
                    case 1:
                        nw = std::max(10.0f, ndx - mResizeStartX);
                        nh = std::max(10.0f, mResizeStartH + (mResizeStartY - ndy));
                        ny = mResizeStartY + (mResizeStartH - nh);
                        break;
                    case 2:
                        nw = std::max(10.0f, mResizeStartW + (mResizeStartX - ndx));
                        nh = std::max(10.0f, ndy - mResizeStartY);
                        nx = mResizeStartX + (mResizeStartW - nw);
                        break;
                    case 3:
                        nw = std::max(10.0f, ndx - mResizeStartX);
                        nh = std::max(10.0f, ndy - mResizeStartY);
                        break;
                }
                elem->w = roundValue(nw); elem->h = roundValue(nh);
                elem->x = roundCoord(nx); elem->y = roundCoord(ny);
                layout.setElement(*elem);
                checkOverlaps();
                markEdited();
            }
        } else {
            mResizing = false;
            mResizeCorner = -1;
            mHasOverlap = false;
        }
    }
}

// ── Keyboard ─────────────────────────────────────────────────────
void GuiEditor::handleKeyboard(GLFWwindow* win)
{
    if (mSelectedId.empty() || mActiveLayoutFile.empty()) return;
    float step = 1.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) step = 10.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) step = 50.0f;

    float dx = 0, dy = 0;
    if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) dx -= step;
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) dx += step;
    if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) dy -= step;
    if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) dy += step;
    if (dx == 0 && dy == 0) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
    if (!elem) return;
    if (glfwGetKey(win, GLFW_KEY_T) == GLFW_PRESS) {
        elem->textOffsetX += dx; elem->textOffsetY += dy;
    } else if (glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
               glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
        elem->w = std::max(10.0f, elem->w + dx);
        elem->h = std::max(10.0f, elem->h + dy);
    } else {
        elem->x += dx; elem->y += dy;
    }
    elem->x = roundCoord(elem->x); elem->y = roundCoord(elem->y);
    layout.setElement(*elem);
    checkOverlaps();
    markEdited();
}

// ── Selection handles ────────────────────────────────────────────
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

// ── Property panel ───────────────────────────────────────────────
void GuiEditor::renderPropertyPanel(const GuiElement& elem)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    auto dsx = [&](float x) { return cs.designToScreenX(x); };
    auto dsy = [&](float y) { return cs.designToScreenY(y); };

    // 14 flat sliders (no section header rows to keep index→Y mapping clean)
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

    // Panel background
    float panelH = 36.0f + numSliders * PP_ROW_H + 80.0f;
    UIRect pbg = cs.designToScreen({PP_X, PP_Y, PP_W, panelH});
    uiDrawRect(pbg, {0.08f, 0.08f, 0.12f, 0.92f}, "gui-prop");
    uiDrawRectOutline(pbg, {0.3f, 0.3f, 0.4f, 0.8f}, "gui-prop-border");

    // Title
    char title[128];
    snprintf(title, sizeof(title), "PROPERTIES: %s (%s)",
             elem.id.c_str(), elem.type.c_str());
    uiDrawText(title, dsx(PP_X + 8), dsy(PP_Y + 4), 0.30f,
               {0.4f, 1.0f, 0.6f, 1.0f});
    uiDrawRect({dsx(PP_X + 4), dsy(PP_Y + 28), dsx(PP_W - 8), 1},
               {0.3f, 0.3f, 0.5f, 0.6f}, "gui-p-div");

    // Color swatch helper: draw a small rect filled with a color
    auto drawSwatch = [&](float dx, float dy, float r, float g, float b, float a) {
        float sx = dsx(dx), sy = dsy(dy);
        float sw = dsx(14.0f), sh = dsy(12.0f);
        uiDrawRect({sx, sy, sw, sh}, {r, g, b, a}, "swatch");
        uiDrawRectOutline({sx, sy, sw, sh}, {0.5f, 0.5f, 0.5f, 0.8f}, "swatch-border");
    };

    // Draw each slider
    for (int i = 0; i < numSliders; ++i) {
        float ry = PP_Y + 34.0f + i * PP_ROW_H;

        // Color swatch before textColor and bgColor rows
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

        // Label (offset for color swatch columns)
        float labelX = (i >= 5 && i <= 12) ? PP_LABEL_X + 6.0f : PP_LABEL_X;
        uiDrawText(sliders[i].label, dsx(labelX), dsy(ry), 0.26f,
                   {0.7f, 0.8f, 0.9f, 1.0f});

        // Track
        float tsx = dsx(PP_TRACK_X), tsy = dsy(ry);
        float tsw = dsx(PP_TRACK_W), tsh = dsy(PP_ROW_H);
        uiDrawRect({tsx, tsy, tsw, tsh}, {0.15f, 0.15f, 0.2f, 1.0f}, "st");

        // Fill
        float t = (sliders[i].mx > sliders[i].mn)
            ? std::clamp((sliders[i].val - sliders[i].mn)
                        / (sliders[i].mx - sliders[i].mn), 0.0f, 1.0f)
            : 0.0f;
        uiDrawRect({tsx, tsy, tsw * t, tsh}, {0.25f, 0.6f, 0.9f, 1.0f}, "sf");

        // Value
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", sliders[i].val);
        uiDrawText(buf, dsx(PP_VAL_X), dsy(ry), 0.26f,
                   {0.9f, 0.9f, 0.5f, 1.0f});
    }

    // Visibility / enabled / anchor / text
    float toy = PP_Y + 34.0f + numSliders * PP_ROW_H + 4;
    uiDrawText(elem.visible ? "[VISIBLE]" : "[HIDDEN]",
               dsx(PP_X + 8), dsy(toy), 0.26f,
               elem.visible ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(1,0.3f,0.3f,1));
    uiDrawText(elem.enabled ? "[ENABLED]" : "[DISABLED]",
               dsx(PP_X + 120), dsy(toy), 0.26f,
               elem.enabled ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(1,0.3f,0.3f,1));

    char info[128];
    snprintf(info, sizeof(info), "Anchor: %s/%s  Layer: %d  Rot: %.0f",
             elem.anchorX.c_str(), elem.anchorY.c_str(), elem.layer, elem.rotation);
    uiDrawText(info, dsx(PP_X + 8), dsy(toy + PP_ROW_H), 0.24f,
               {0.5f, 0.6f, 0.8f, 1.0f});

    if (!elem.text.empty()) {
        uiDrawText(elem.text.c_str(), dsx(PP_X + 8), dsy(toy + PP_ROW_H * 2),
                   0.24f, {0.8f, 0.8f, 0.5f, 1.0f});
    }
}

// ── Hierarchy ────────────────────────────────────────────────────
void GuiEditor::renderHierarchyView()
{
    if (mActiveLayoutFile.empty()) return;
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    auto ids = layout.elementIds();
    if (ids.empty()) return;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const float hx = 10.0f, hy = 80.0f, hw = 220.0f;
    float maxH = std::max(60.0f, (float)ids.size() * PP_ROW_H + 34.0f);

    UIRect bg = cs.designToScreen({hx, hy, hw, maxH});
    uiDrawRect(bg, {0.08f, 0.08f, 0.12f, 0.92f}, "gui-hier");
    uiDrawRectOutline(bg, {0.3f, 0.3f, 0.4f, 0.7f}, "gui-hier-border");

    uiDrawText("ELEMENTS", cs.designToScreenX(hx + 8), cs.designToScreenY(hy + 4),
               0.28f, {0.4f, 1.0f, 0.6f, 1.0f});
    uiDrawRect({cs.designToScreenX(hx + 4), cs.designToScreenY(hy + 28),
                cs.designToScreenX(hw - 8), 1},
               {0.3f, 0.3f, 0.5f, 0.6f}, "gui-h-div");

    for (size_t i = 0; i < ids.size(); ++i) {
        float ry = hy + 32.0f + i * PP_ROW_H;
        float rsx = cs.designToScreenX(hx + 2);
        float rsy = cs.designToScreenY(ry);
        float rsw = cs.designToScreenX(hw - 4);
        float rsh = cs.designToScreenY(PP_ROW_H - 2);
        bool sel = (ids[i] == mSelectedId);
        if (sel)
            uiDrawRect({rsx, rsy, rsw, rsh}, {0.2f, 0.4f, 0.6f, 0.7f}, "gui-h-sel");

        const GuiElement* el = layout.get(ids[i]);
        const char* icon = "?";
        if (el) {
            if (el->type == "button") icon = "B";
            else if (el->type == "text" || el->type == "label") icon = "T";
            else if (el->type == "image") icon = "I";
            else if (el->type == "panel") icon = "P";
            else if (el->type == "checkbox") icon = "C";
        }
        char label[128];
        snprintf(label, sizeof(label), "[%s] %s", icon, ids[i].c_str());
        uiDrawText(label, rsx + 4, rsy, 0.24f,
                   sel ? glm::vec4(1,1,1,1) : glm::vec4(0.7f,0.8f,0.9f,1));
    }
}

// ── Snap guides ──────────────────────────────────────────────────
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

// ── Main overlay ─────────────────────────────────────────────────
void GuiEditor::renderOverlay(GLFWwindow* win)
{
    (void)win;

    // Auto-save indicator
    if (GuiLayoutManager::instance().hasUnsaved()) {
        const char* text = "[AUTO-SAVE PENDING]";
        float tw = uiMeasureText(text, 0.30f);
        float sx = uiScreenW() - tw - 20.0f;
        uiDrawRect({sx - 8, 8, tw + 16, 26}, {0.5f, 0.1f, 0.05f, 0.85f}, "gui-unsaved");
        uiDrawText(text, sx, 12, 0.30f, {1.0f, 0.6f, 0.4f, 1.0f});
    }

    // Mode indicator
    {
        const char* mt = "[EDIT MODE] drag=move corner=resize T+arrows=text-offset";
        float tw = uiMeasureText(mt, 0.28f);
        uiDrawRect({10, 10, tw + 20, 24}, {0.15f, 0.15f, 0.2f, 0.85f}, "gui-mode");
        uiDrawText(mt, 18, 12, 0.28f, {1.0f, 0.8f, 0.1f, 1.0f});
        if (!mActiveLayoutFile.empty()) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Layout: %s", mActiveLayoutFile.c_str());
            uiDrawRect({10, 38, uiMeasureText(buf, 0.22f) + 16, 20},
                       {0.1f, 0.1f, 0.15f, 0.8f}, "gui-layout");
            uiDrawText(buf, 18, 40, 0.22f, {0.6f, 0.8f, 1.0f, 1.0f});
        }
    }

    renderHierarchyView();
    renderSnapGuides();

    if (mSelectedId.empty() || mActiveLayoutFile.empty()) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    const GuiElement* elem = layout.get(mSelectedId);
    if (!elem) return;

    if (mHasOverlap) {
        const char* wt = "[OVERLAP]";
        float tw = uiMeasureText(wt, 0.28f);
        float sx = uiScaleX(elem->x), sy = uiScaleY(elem->y);
        uiDrawRect({sx - 4, sy - 28, tw + 8, 22}, {0.5f, 0, 0, 0.85f}, "gui-ow");
        uiDrawText(wt, sx + 2, sy - 26, 0.28f, {1, 0.3f, 0.2f, 1});
    }

    renderSelectionHandles(*elem);
    renderPropertyPanel(*elem);
}
