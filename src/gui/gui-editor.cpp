#include "gui-editor.h"
#include "gui-layout.h"
#include "ui-system.h"

#include <cstdio>
#include <algorithm>
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
}

void GuiEditor::update(GLFWwindow* win)
{
    if (!mEnabled) return;

    // Get framebuffer size for reference center
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    mCenterX = fbW * 0.5f;
    mCenterY = fbH * 0.5f;

    handleInput(win);
    handleKeyboard(win);
    renderOverlay(win);
}

void GuiEditor::handleInput(GLFWwindow* win)
{
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);

    bool mouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (mouseDown && !mDragging) {
        // Check if clicking on any element in the active layout
        // We iterate all layouts and look for elements under the cursor
        GuiLayoutManager& mgr = GuiLayoutManager::instance();

        // Check all layouts for the selected element
        // The editor only works with the LAST accessed layout
        // For multi-layout support, we'd need the active layout name
        // For now, use the file path from the layout manager's last accessed layout

        // Hit test against elements of the active layout
        for (const auto& layoutPair : {
            "config/gui/main-menu.json"
        }) {
            GuiLayout& layout = mgr.getLayout(layoutPair);
            for (const std::string& id : layout.elementIds()) {
                const GuiElement* elem = layout.get(id);
                if (!elem) continue;

                // Compute screen position (centered layout assumption)
                float sx = mCenterX + elem->x;
                float sy = mCenterY + elem->y;

                if (mx >= sx && mx <= sx + elem->w &&
                    my >= sy && my <= sy + elem->h) {
                    mSelectedId = id;
                    mDragOffsetX = (float)mx - sx;
                    mDragOffsetY = (float)my - sy;
                    mDragging = true;
                    printf("[GUI EDIT] selected=%s  pos=(%.0f,%.0f)  size=(%.0f,%.0f)\n",
                           id.c_str(), sx, sy, elem->w, elem->h);
                    return;
                }
            }
        }
    }

    if (mDragging) {
        if (mouseDown && !mSelectedId.empty()) {
            // Drag the element
            // Update layout offset from center
            float newOffX = (float)mx - mCenterX - mDragOffsetX;
            float newOffY = (float)my - mCenterY - mDragOffsetY;

            // Update in ALL layouts (just in case)
            GuiLayoutManager& mgr = GuiLayoutManager::instance();
            for (const auto& layoutPair : {
                "config/gui/main-menu.json"
            }) {
                GuiLayout& layout = mgr.getLayout(layoutPair);
                const GuiElement* elem = layout.get(mSelectedId);
                if (elem) {
                    layout.set(mSelectedId, newOffX, newOffY, elem->w, elem->h);
                }
            }
        } else {
            mDragging = false;
        }
    }
}

void GuiEditor::handleKeyboard(GLFWwindow* win)
{
    if (mSelectedId.empty()) return;

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
        GuiLayoutManager& mgr = GuiLayoutManager::instance();
        for (const auto& layoutPair : {
            "config/gui/main-menu.json"
        }) {
            GuiLayout& layout = mgr.getLayout(layoutPair);
            const GuiElement* elem = layout.get(mSelectedId);
            if (elem) {
                layout.set(mSelectedId, elem->x + dx, elem->y + dy, elem->w, elem->h);
            }
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
        const char* modeText = "[EDIT MODE] gui_edit 0 to exit";
        uiDrawRect({10, 10, uiMeasureText(modeText, 0.30f) + 20, 26},
                   {0.15f, 0.15f, 0.2f, 0.85f}, "gui-edit-bg");
        uiDrawText(modeText, 18, 12, 0.30f, {1.0f, 0.8f, 0.1f, 1.0f});
    }

    if (mSelectedId.empty()) return;

    GuiLayoutManager& mgr = GuiLayoutManager::instance();
    for (const auto& layoutPair : {
        "config/gui/main-menu.json"
    }) {
        GuiLayout& layout = mgr.getLayout(layoutPair);
        const GuiElement* elem = layout.get(mSelectedId);
        if (!elem) continue;

        float sx = mCenterX + elem->x;
        float sy = mCenterY + elem->y;

        // Selection highlight rectangle
        uiDrawRectOutline({sx - 2, sy - 2, elem->w + 4, elem->h + 4},
                         {1.0f, 0.8f, 0.1f, 1.0f}, "gui-edit-select");

        // Corner handles (4 small squares)
        float handleSize = 6.0f;
        glm::vec4 handleColor{1.0f, 1.0f, 0.3f, 1.0f};
        uiDrawRect({sx - handleSize, sy - handleSize, handleSize * 2, handleSize * 2},
                   handleColor, "gui-edit-handle");
        uiDrawRect({sx + elem->w - handleSize, sy - handleSize, handleSize * 2, handleSize * 2},
                   handleColor, "gui-edit-handle");
        uiDrawRect({sx - handleSize, sy + elem->h - handleSize, handleSize * 2, handleSize * 2},
                   handleColor, "gui-edit-handle");
        uiDrawRect({sx + elem->w - handleSize, sy + elem->h - handleSize, handleSize * 2, handleSize * 2},
                   handleColor, "gui-edit-handle");

        // Element info label
        char info[128];
        snprintf(info, sizeof(info), "%s  (%.0f, %.0f)  %.0f x %.0f",
                 mSelectedId.c_str(), elem->x, elem->y, elem->w, elem->h);
        float labelW = uiMeasureText(info, 0.28f);
        uiDrawRect({sx - 4, sy - 28, labelW + 8, 22},
                   {0.0f, 0.0f, 0.0f, 0.75f}, "gui-edit-label-bg");
        uiDrawText(info, sx + 2, sy - 26, 0.28f, {1.0f, 0.8f, 0.1f, 1.0f});

        break;
    }
}
