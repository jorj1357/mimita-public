#include "gui-editor.h"
#include "gui-coord.h"

#include "gui-layout.h"
#include "ui-system.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>

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
    }
}

void GuiEditor::setActiveLayout(const std::string& filePath)
{
    mActiveLayoutFile = filePath;
    // Pre-load the layout to ensure it's in the manager
    if (!filePath.empty())
        GuiLayoutManager::instance().getLayout(filePath);
}

void GuiEditor::update(GLFWwindow* win)
{
    if (!mEnabled) return;
    handleInput(win);
    handleKeyboard(win);
    renderOverlay(win);
}

void GuiEditor::checkOverlaps()
{
    mHasOverlap = false;
    if (mActiveLayoutFile.empty() || mSelectedId.empty()) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    const GuiElement* selected = layout.get(mSelectedId);
    if (!selected) return;

    // Convert design coords to framebuffer for overlap comparison
    float sx = uiScaleX(selected->x);
    float sy = uiScaleY(selected->y);
    float sw = uiScaleX(selected->w);
    float sh = uiScaleY(selected->h);

    for (const std::string& id : layout.elementIds())
    {
        if (id == mSelectedId) continue;
        const GuiElement* elem = layout.get(id);
        if (!elem) continue;

        float ex = uiScaleX(elem->x);
        float ey = uiScaleY(elem->y);

        if (sx < ex + uiScaleX(elem->w) && sx + sw > ex &&
            sy < ey + uiScaleY(elem->h) && sy + sh > ey)
        {
            mHasOverlap = true;
            printf("[GUI EDIT OVERLAP] \"%s\" overlaps \"%s\"\n",
                   mSelectedId.c_str(), id.c_str());
            return;
        }
    }
}

void GuiEditor::handleInput(GLFWwindow* win)
{
    // Get cursor in screen (framebuffer) coordinates
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    double fbx, fby;
    GuiCoordinateSystem::instance().cursorWindowToScreen(mx, my, fbx, fby);

    // Convert cursor to design coordinates for comparison with tracked widgets
    double dx = GuiCoordinateSystem::instance().screenToDesignX((float)fbx);
    double dy = GuiCoordinateSystem::instance().screenToDesignY((float)fby);

    bool mouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (mouseDown && !mDragging && !mResizing &&
        !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
        GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
        const GuiElement* elem = layout.get(mSelectedId);
        if (elem) {
            const float handleRadius = 12.0f;
            const float right = elem->x + elem->w;
            const float bottom = elem->y + elem->h;
            if (std::abs((float)dx - right) <= handleRadius &&
                std::abs((float)dy - bottom) <= handleRadius) {
                mResizing = true;
                return;
            }
        }
    }

    if (mouseDown && !mDragging && !mResizing) {
        // Only check widgets rendered this frame (tracked in design coordinates)
        const auto& widgets = uiGetTrackedWidgets();
        for (const auto& w : widgets) {
            double wx = w.rect.x, wy = w.rect.y, ww = w.rect.w, wh = w.rect.h;
            if (dx >= wx && dx <= wx + ww && dy >= wy && dy <= wy + wh) {
                mSelectedId = w.id;
                mDragOffsetX = (float)dx - wx;
                mDragOffsetY = (float)dy - wy;
                mDragging = true;
                mHasOverlap = false;
                printf("[GUI EDIT] selected widget=\"%s\"  design=(%.0f,%.0f)  size=(%.0f,%.0f)\n",
                       w.id.c_str(), wx, wy, ww, wh);

                // Store directly in layout (already design coordinates)
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
        if (mouseDown && !mSelectedId.empty()) {
            if (!mActiveLayoutFile.empty()) {
                GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
                const GuiElement* elem = layout.get(mSelectedId);
                float w = elem ? elem->w : 50.0f;
                float h = elem ? elem->h : 30.0f;
                // Drag delta is in design coordinates
                float newX = (float)dx - mDragOffsetX;
                float newY = (float)dy - mDragOffsetY;
                layout.set(mSelectedId, newX, newY, w, h);
                checkOverlaps();
            }
        } else {
            mDragging = false;
            mHasOverlap = false;
        }
    }

    if (mResizing) {
        if (mouseDown && !mSelectedId.empty() && !mActiveLayoutFile.empty()) {
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
            const GuiElement* elem = layout.get(mSelectedId);
            if (elem) {
                GuiElement updated = *elem;
                updated.w = std::max(1.0f, (float)dx - updated.x);
                updated.h = std::max(1.0f, (float)dy - updated.y);
                layout.setElement(updated);
                checkOverlaps();
            }
        } else {
            mResizing = false;
            mHasOverlap = false;
        }
    }
}

void GuiEditor::handleKeyboard(GLFWwindow* win)
{
    if (mSelectedId.empty() || mActiveLayoutFile.empty()) return;

    float step = 1.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        step = 10.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
        step = 50.0f;

    float dx = 0.0f, dy = 0.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) dx -= step;
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) dx += step;
    if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) dy -= step;
    if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) dy += step;

    if (dx != 0.0f || dy != 0.0f) {
        GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
        const GuiElement* elem = layout.get(mSelectedId);
        if (elem) {
            GuiElement updated = *elem;
            if (glfwGetKey(win, GLFW_KEY_T) == GLFW_PRESS) {
                updated.textOffsetX += dx;
                updated.textOffsetY += dy;
            } else if (glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                       glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
                updated.w = std::max(1.0f, updated.w + dx);
                updated.h = std::max(1.0f, updated.h + dy);
            } else {
                updated.x += dx;
                updated.y += dy;
            }
            layout.setElement(updated);
            checkOverlaps();
        }
    }
}

void GuiEditor::renderOverlay(GLFWwindow* win)
{
    // Top-right unsaved indicator
    if (GuiLayoutManager::instance().hasUnsaved()) {
        const char* unsavedText = "[UNSAVED] Use gui_save to persist layout changes";
        float tw = uiMeasureText(unsavedText, 0.30f);
        float sx = uiScreenW() - tw - 20.0f;
        float sy = 12.0f;
        uiDrawRect({sx - 8, sy - 4, tw + 16, 26},
                   {0.5f, 0.1f, 0.05f, 0.85f}, "gui-unsaved-bg");
        uiDrawText(unsavedText, sx, sy, 0.30f, {1.0f, 0.6f, 0.4f, 1.0f});
    }

    // Editor mode indicator (top-left)
    {
        const char* modeText = "[EDIT MODE] drag=move corner=resize T+arrows=text offset";
        uiDrawRect({10, 10, uiMeasureText(modeText, 0.30f) + 20, 26},
                   {0.15f, 0.15f, 0.2f, 0.85f}, "gui-edit-bg");
        uiDrawText(modeText, 18, 12, 0.30f, {1.0f, 0.8f, 0.1f, 1.0f});

        // Show active layout file
        if (!mActiveLayoutFile.empty()) {
            char layoutInfo[128];
            snprintf(layoutInfo, sizeof(layoutInfo), "Layout: %s", mActiveLayoutFile.c_str());
            uiDrawRect({10, 42, uiMeasureText(layoutInfo, 0.24f) + 20, 22},
                       {0.1f, 0.1f, 0.15f, 0.8f}, "gui-layout-bg");
            uiDrawText(layoutInfo, 18, 44, 0.24f, {0.6f, 0.8f, 1.0f, 1.0f});
        }
    }

    if (mSelectedId.empty()) return;
    if (mActiveLayoutFile.empty()) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    const GuiElement* elem = layout.get(mSelectedId);
    if (!elem) return;

    // Convert design coordinates to framebuffer for rendering
    float sx = uiScaleX(elem->x);
    float sy = uiScaleY(elem->y);
    float sw = uiScaleX(elem->w);
    float sh = uiScaleY(elem->h);

    // Selection highlight rectangle: green = no overlap, red = overlap
    glm::vec4 outlineColor = mHasOverlap
        ? glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
        : glm::vec4(0.0f, 1.0f, 0.2f, 1.0f);

    if (mHasOverlap) {
        const char* warnText = "[OVERLAP] Element overlaps another";
        float tw = uiMeasureText(warnText, 0.28f);
        uiDrawRect({sx - 4, sy - 52, tw + 8, 22},
                   {0.5f, 0.0f, 0.0f, 0.85f}, "gui-overlap-warn-bg");
        uiDrawText(warnText, sx + 2, sy - 50, 0.28f, {1.0f, 0.3f, 0.2f, 1.0f});
    }
    uiDrawRectOutline({sx - 2, sy - 2, sw + 4, sh + 4},
                     outlineColor, "gui-edit-select");

    // Corner handles (4 small squares)
    float handleSize = 6.0f;
    glm::vec4 handleColor{1.0f, 1.0f, 0.3f, 1.0f};
    uiDrawRect({sx - handleSize, sy - handleSize, handleSize * 2, handleSize * 2},
               handleColor, "gui-edit-handle");
    uiDrawRect({sx + sw - handleSize, sy - handleSize, handleSize * 2, handleSize * 2},
               handleColor, "gui-edit-handle");
    uiDrawRect({sx - handleSize, sy + sh - handleSize, handleSize * 2, handleSize * 2},
               handleColor, "gui-edit-handle");
    uiDrawRect({sx + sw - handleSize, sy + sh - handleSize, handleSize * 2, handleSize * 2},
               handleColor, "gui-edit-handle");

    // Element info label (show design coordinates)
    char info[128];
    snprintf(info, sizeof(info), "%s  design=(%.0f, %.0f)  %.0f x %.0f  text=(%.0f, %.0f)",
             mSelectedId.c_str(), elem->x, elem->y, elem->w, elem->h,
             elem->textOffsetX, elem->textOffsetY);
    float labelW = uiMeasureText(info, 0.28f);
    uiDrawRect({sx - 4, sy - 28, labelW + 8, 22},
               {0.0f, 0.0f, 0.0f, 0.75f}, "gui-edit-label-bg");
    uiDrawText(info, sx + 2, sy - 26, 0.28f, {1.0f, 0.8f, 0.1f, 1.0f});
}
