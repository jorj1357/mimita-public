#include "gui-editor.h"
#include "gui-coord.h"

#include "gui-layout.h"
#include "gui-element-render.h"
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

    // Hierarchy click (elements start at y=130 now, filter at y=108-128)
    if (pressed && !mDragging && !mResizing) {
        const float hx = 12.0f, hw = 218.0f;
        // Filter area click
        if (dx >= hx && dx <= hx + hw && dy >= 108 && dy <= 128 && !mActiveLayoutFile.empty()) {
            mFilterFocused = !mFilterFocused;
            if (!mFilterFocused) mHierarchyFilter.clear();
            return;
        }
        // Element list click (with shift multi-select)
        const float hy = 130.0f;
        if (dx >= hx && dx <= hx + hw && dy >= hy && !mActiveLayoutFile.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            auto allIds = layout.elementIds();
            std::vector<std::string> ids;
            for (const std::string& id : allIds)
                if (mHierarchyFilter.empty() || id.find(mHierarchyFilter) != std::string::npos)
                    ids.push_back(id);
            int idx = (int)((dy - hy) / PP_ROW_H);
            if (idx >= 0 && idx < (int)ids.size()) {
                bool shiftHeld = glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                 glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
                if (shiftHeld) {
                    // Toggle multi-select
                    auto it = std::find(mMultiSelectedIds.begin(), mMultiSelectedIds.end(), ids[idx]);
                    if (it != mMultiSelectedIds.end())
                        mMultiSelectedIds.erase(it);
                    else
                        mMultiSelectedIds.push_back(ids[idx]);
                    if (!mMultiSelectedIds.empty())
                        mSelectedId = mMultiSelectedIds.back();
                } else {
                    mMultiSelectedIds.clear();
                    mSelectedId = ids[idx];
                }
                mDragOffsetX = 0; mDragOffsetY = 0;
                printf("[GUI EDIT] %sselected \"%s\"\n", shiftHeld ? "multi-" : "", ids[idx].c_str());
                return;
            }
        }
    }

    // Color picker trigger: click on color label area (left of track for rows 5-12)
    if (pressed && !mDragging && !mResizing && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
        const float labelStartY = PP_Y + 34.0f;
        if (dy >= labelStartY && dy < labelStartY + 14 * PP_ROW_H) {
            int row = (int)((dy - labelStartY) / PP_ROW_H);
            if (row >= 5 && row <= 12 && dx < PP_TRACK_X) { // Color rows, left of track
                mColorPickerOpen = !mColorPickerOpen;
                mColorPickerTarget = mColorPickerOpen ? row : -1;
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

    // Selection click with double-click detection for text editing
    if (pressed && !mDragging && !mResizing) {
        for (const auto& w : uiGetTrackedWidgets()) {
            double wx = w.rect.x, wy = w.rect.y, ww = w.rect.w, wh = w.rect.h;
            if (dx >= wx && dx <= wx + ww && dy >= wy && dy <= wy + wh) {
                // Double-click on same element → enter text editing
                double now = glfwGetTime();
                if (w.id == mSelectedId && !mSelectedId.empty() && now - mLastClickTime < 0.5) {
                    mEditingText = true;
                    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
                    const GuiElement* elem = layout.get(mSelectedId);
                    mTextEditBuffer = elem ? elem->text : "";
                    printf("[GUI EDIT] text editing started for \"%s\": \"%s\"\n",
                           mSelectedId.c_str(), mTextEditBuffer.c_str());
                    return;
                }
                mEditingText = false;
                mLastClickTime = now;
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

    // Hierarchy filter keyboard input
    if (mFilterFocused) {
        if (glfwGetKey(win, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
            static bool fbPrev = false;
            if (!mHierarchyFilter.empty() && !fbPrev) mHierarchyFilter.pop_back();
            fbPrev = true;
        } else { static bool fbPrev = false; (void)fbPrev; }
        if (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS) { mFilterFocused = false; return; }
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) { mFilterFocused = false; mHierarchyFilter.clear(); return; }
        return;
    }

    // Text editing mode: keyboard input for text
    if (mEditingText) {
        // Backspace
        if (glfwGetKey(win, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
            static bool bsPrev = false;
            if (!mTextEditBuffer.empty() && !bsPrev) {
                mTextEditBuffer.pop_back();
                GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
                GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
                if (elem) { elem->text = mTextEditBuffer; layout.setElement(*elem); markEdited(); }
            }
            bsPrev = true;
        } else { static bool bsPrev = false; (void)bsPrev; }
        // Enter to confirm
        if (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS) {
            mEditingText = false;
            printf("[GUI EDIT] text editing ended for \"%s\": \"%s\"\n",
                   mSelectedId.c_str(), mTextEditBuffer.c_str());
            return;
        }
        // Escape to cancel
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            mEditingText = false;
            return;
        }
        return; // Don't process movement keys during text editing
    }

    // Ctrl+D: duplicate selected element
    if ((glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
         glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) &&
        glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) {
        static bool dupPrev = false;
        if (!dupPrev && !mSelectedId.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
            if (elem) {
                GuiElement dup = *elem;
                dup.id = elem->id + "_copy";
                dup.x += 20.0f;
                dup.y += 20.0f;
                layout.setElement(dup);
                mSelectedId = dup.id;
                markEdited();
                printf("[GUI EDIT] duplicated \"%s\" -> \"%s\"\n", elem->id.c_str(), dup.id.c_str());
            }
            dupPrev = true;
        }
    } else { static bool dupPrev = false; (void)dupPrev; }

    // Delete key: remove selected element
    if (glfwGetKey(win, GLFW_KEY_DELETE) == GLFW_PRESS && !mSelectedId.empty()) {
        static bool delPrev = false;
        if (!delPrev) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            GuiElement removed;
            removed.id = mSelectedId;
            // Overwrite with an invisible element (can't delete from map, so make it invisible)
            removed.visible = false;
            removed.w = 0;
            removed.h = 0;
            layout.setElement(removed);
            printf("[GUI EDIT] deleted element \"%s\"\n", mSelectedId.c_str());
            mSelectedId.clear();
            markEdited();
            delPrev = true;
        }
    } else { static bool delPrev = false; (void)delPrev; }
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

// ── Character input (called from glfw char callback) ──────────────
void GuiEditor::handleChar(unsigned int codepoint)
{
    if (codepoint < 32 || codepoint > 126) return;

    // Hierarchy filter
    if (mFilterFocused) {
        if (mHierarchyFilter.size() < 40) {
            mHierarchyFilter.push_back((char)codepoint);
        }
        return;
    }

    if (!mEditingText || mSelectedId.empty() || mActiveLayoutFile.empty()) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
    if (!elem) return;

    mTextEditBuffer.push_back((char)codepoint);
    elem->text = mTextEditBuffer;
    layout.setElement(*elem);
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
void GuiEditor::renderPropertyPanel(GLFWwindow* win, const GuiElement& elem)
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

    // Visibility / enabled toggles (clickable)
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

    // Layer up/down buttons
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
            // Show blinking cursor
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

// ── Color picker palette ──────────────────────────────────────────
void GuiEditor::renderColorPicker()
{
    if (!mColorPickerOpen) return;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    // Palette position: right of property panel, same vertical center
    float palX = PP_X + PP_W + 8.0f;
    float palY = PP_Y + 100.0f;
    float swatchSize = 22.0f;
    float gap = 2.0f;
    int cols = 4;
    float palW = cols * (swatchSize + gap) + 6.0f;
    int numColors = 12;
    float palH = ((numColors + cols - 1) / cols) * (swatchSize + gap) + 6.0f;

    // Preset colors (R,G,B)
    const float presetColors[12][3] = {
        {1,1,1}, {0.8f,0.8f,0.8f}, {0.5f,0.5f,0.5f}, {0,0,0},
        {1,0,0}, {1,0.5f,0}, {1,1,0}, {0,1,0},
        {0,0.5f,1}, {0,0,1}, {0.5f,0,0.5f}, {1,0,1}
    };

    UIRect palBg = cs.designToScreen({palX, palY, palW, palH});
    uiDrawRect(palBg, {0.1f, 0.1f, 0.14f, 0.95f}, "color-pal");
    uiDrawRectOutline(palBg, {0.4f, 0.4f, 0.6f, 0.9f}, "color-pal-border");

    // Close button
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

    // Interaction handled in handleInput
    // Check if user clicked outside palette to close
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
        // Check color swatch clicks
        for (int i = 0; i < numColors; ++i) {
            int row = i / cols;
            int col = i % cols;
            float cx = palX + 3.0f + col * (swatchSize + gap);
            float cy = palY + 3.0f + row * (swatchSize + gap);
            if (dx >= cx && dx <= cx + swatchSize && dy >= cy && dy <= cy + swatchSize) {
                // Apply preset color to the target element
                if (!mSelectedId.empty() && !mActiveLayoutFile.empty() && mColorPickerTarget >= 0) {
                    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
                    GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
                    if (elem) {
                        // Determine which color vector to update based on target
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
        // Close button (top-right X area)
        if (dx >= palX + palW - 18.0f && dx <= palX + palW &&
            dy >= palY && dy <= palY + 20.0f) {
            mColorPickerOpen = false;
        }
    }
}

// ── Hierarchy ────────────────────────────────────────────────────
void GuiEditor::renderHierarchyView()
{
    if (mActiveLayoutFile.empty()) return;
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    auto allIds = layout.elementIds();
    if (allIds.empty()) return;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const float hx = 10.0f, hy = 80.0f, hw = 220.0f;

    // Filter
    std::vector<std::string> ids;
    for (const std::string& id : allIds) {
        if (mHierarchyFilter.empty() ||
            id.find(mHierarchyFilter) != std::string::npos)
            ids.push_back(id);
    }

    float maxH = std::max(60.0f, (float)ids.size() * PP_ROW_H + 60.0f);

    UIRect bg = cs.designToScreen({hx, hy, hw, maxH});
    uiDrawRect(bg, {0.08f, 0.08f, 0.12f, 0.92f}, "gui-hier");
    uiDrawRectOutline(bg, {0.3f, 0.3f, 0.4f, 0.7f}, "gui-hier-border");

    uiDrawText("ELEMENTS", cs.designToScreenX(hx + 8), cs.designToScreenY(hy + 4),
               0.28f, {0.4f, 1.0f, 0.6f, 1.0f});

    // Filter display
    float filterY = hy + 28.0f;
    char filterBuf[64];
    snprintf(filterBuf, sizeof(filterBuf), "Filter: %s", mHierarchyFilter.c_str());
    uiDrawText(filterBuf, cs.designToScreenX(hx + 4), cs.designToScreenY(filterY),
               0.22f, {0.6f, 0.7f, 0.9f, 1.0f});
    uiDrawRect({cs.designToScreenX(hx + 4), cs.designToScreenY(filterY + 18),
                cs.designToScreenX(hw - 8), 1},
               {0.3f, 0.3f, 0.5f, 0.6f}, "gui-h-div");

    for (size_t i = 0; i < ids.size(); ++i) {
        float ry = hy + 50.0f + i * PP_ROW_H;
        float rsx = cs.designToScreenX(hx + 2);
        float rsy = cs.designToScreenY(ry);
        float rsw = cs.designToScreenX(hw - 4);
        float rsh = cs.designToScreenY(PP_ROW_H - 2);
        bool sel = (ids[i] == mSelectedId) ||
                   std::find(mMultiSelectedIds.begin(), mMultiSelectedIds.end(), ids[i]) != mMultiSelectedIds.end();
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

    // "+" button to create new element
    float addY = hy + 50.0f + ids.size() * PP_ROW_H + 4;
    UIRect addBtnRect = {hx + 4, addY, 40, 22};
    GuiElement addBtn;
    addBtn.type = "button";
    addBtn.text = "+";
    addBtn.textColor = {1,1,1,1};
    addBtn.backgroundColor = {0.2f, 0.4f, 0.2f, 1.0f};
    if (drawGuiElement(glfwGetCurrentContext(), addBtn, nullptr, &addBtnRect).clicked) {
        GuiLayout& lay = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
        int seq = 0;
        std::string newId = "newElement";
        while (lay.get(newId)) { seq++; newId = "newElement" + std::to_string(seq); }
        GuiElement ne;
        ne.id = newId;
        ne.type = "text";
        ne.text = "New Element";
        ne.x = 100; ne.y = 100; ne.w = 200; ne.h = 30;
        lay.setElement(ne);
        mSelectedId = newId;
        markEdited();
        printf("[GUI EDIT] created element \"%s\"\n", newId.c_str());
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

    // Debug overlay: show bounds + hover tooltip for all elements
    if (uiDebugEnabled() && !mActiveLayoutFile.empty()) {
        GuiLayout& dbgLayout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);

        // Hover tooltip: show element ID + type at cursor
        double mxx, myy; glfwGetCursorPos(glfwGetCurrentContext(), &mxx, &myy);
        double fbbx, fbby;
        GuiCoordinateSystem::instance().cursorWindowToScreen(mxx, myy, fbbx, fbby);
        double hdx = GuiCoordinateSystem::instance().screenToDesignX((float)fbbx);
        double hdy = GuiCoordinateSystem::instance().screenToDesignY((float)fbby);
        const GuiElement* hoveredEl = nullptr;
        std::string hoveredId;

        for (const std::string& id : dbgLayout.elementIds()) {
            const GuiElement* de = dbgLayout.get(id);
            if (!de) continue;
            float dsx = uiScaleX(de->x), dsy = uiScaleY(de->y);
            float dsw = uiScaleX(de->w), dsh = uiScaleY(de->h);
            if (de->visible && hdx >= de->x && hdx <= de->x + de->w &&
                hdy >= de->y && hdy <= de->y + de->h) {
                hoveredEl = de;
                hoveredId = id;
            }
            // Outline for all non-selected elements
            if (id != mSelectedId)
                uiDrawRectOutline({dsx, dsy, dsw, dsh}, {0.3f, 0.3f, 0.5f, 0.35f}, "gui-dbg");
            // Anchor indicator
            if (de->anchorX == "center") {
                float acx = uiScaleX(de->x + de->w * 0.5f);
                uiDrawRect({acx - 1, dsy, 2, dsh}, {0.5f, 0.5f, 1.0f, 0.3f}, "gui-anchor-x");
            }
            if (de->anchorY == "middle") {
                float acy = uiScaleY(de->y + de->h * 0.5f);
                uiDrawRect({dsx, acy - 1, dsw, 2}, {0.5f, 0.5f, 1.0f, 0.3f}, "gui-anchor-y");
            }
        }

        // Render hover tooltip
        if (hoveredEl && hoveredId != mSelectedId) {
            char tip[256];
            snprintf(tip, sizeof(tip), "[%s] %s  (%.0f,%.0f) %.0fx%.0f",
                     hoveredEl->type.c_str(), hoveredId.c_str(),
                     hoveredEl->x, hoveredEl->y, hoveredEl->w, hoveredEl->h);
            float tipX = (float)fbbx + 12.0f;
            float tipY = (float)fbby - 20.0f;
            if (tipX + 400 > uiScreenW()) tipX = (float)fbbx - 400.0f;
            if (tipY < 0) tipY = (float)fbby + 12.0f;
            uiDrawRect({tipX - 4, tipY - 2, uiMeasureText(tip, 0.28f) + 8, 22},
                       {0.1f, 0.1f, 0.15f, 0.9f}, "gui-tip-bg");
            uiDrawRectOutline({tipX - 4, tipY - 2, uiMeasureText(tip, 0.28f) + 8, 22},
                              {0.5f, 0.8f, 1.0f, 0.8f}, "gui-tip-border");
            uiDrawText(tip, tipX, tipY, 0.28f, {0.5f, 0.9f, 1.0f, 1.0f});
        }
    }

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
    renderPropertyPanel(win, *elem);
    renderColorPicker();
}
