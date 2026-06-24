#include "gui/gui-editor.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"

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

    if (pressed && !mDragging && !mResizing) {
        const float hx = 12.0f, hw = 218.0f;
        if (dx >= hx && dx <= hx + hw && dy >= 108 && dy <= 128 && !mActiveLayoutFile.empty()) {
            mFilterFocused = !mFilterFocused;
            if (!mFilterFocused) mHierarchyFilter.clear();
            return;
        }
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

    if (pressed && !mDragging && !mResizing && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
        const float labelStartY = PP_Y + 34.0f;
        if (dy >= labelStartY && dy < labelStartY + 14 * PP_ROW_H) {
            int row = (int)((dy - labelStartY) / PP_ROW_H);
            if (row >= 5 && row <= 12 && dx < PP_TRACK_X) {
                mColorPickerOpen = !mColorPickerOpen;
                mColorPickerTarget = mColorPickerOpen ? row : -1;
                return;
            }
        }
    }

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

    if (pressed && !mDragging && !mResizing) {
        for (const auto& w : uiGetTrackedWidgets()) {
            double wx = w.rect.x, wy = w.rect.y, ww = w.rect.w, wh = w.rect.h;
            if (dx >= wx && dx <= wx + ww && dy >= wy && dy <= wy + wh) {
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

void GuiEditor::handleKeyboard(GLFWwindow* win)
{
    if (mSelectedId.empty() || mActiveLayoutFile.empty()) return;

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

    if (mEditingText) {
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
        if (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS) {
            mEditingText = false;
            printf("[GUI EDIT] text editing ended for \"%s\": \"%s\"\n",
                   mSelectedId.c_str(), mTextEditBuffer.c_str());
            return;
        }
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            mEditingText = false;
            return;
        }
        return;
    }

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

    if (glfwGetKey(win, GLFW_KEY_DELETE) == GLFW_PRESS && !mSelectedId.empty()) {
        static bool delPrev = false;
        if (!delPrev) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            GuiElement removed;
            removed.id = mSelectedId;
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
