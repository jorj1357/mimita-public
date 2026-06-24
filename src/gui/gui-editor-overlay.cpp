#include "gui/gui-editor.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <algorithm>
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"

void GuiEditor::renderHierarchyView()
{
    if (mActiveLayoutFile.empty()) return;
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    auto allIds = layout.elementIds();
    if (allIds.empty()) return;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const float hx = 10.0f, hy = 80.0f, hw = 220.0f;

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

void GuiEditor::renderOverlay(GLFWwindow* win)
{
    (void)win;

    if (GuiLayoutManager::instance().hasUnsaved()) {
        const char* text = "[AUTO-SAVE PENDING]";
        float tw = uiMeasureText(text, 0.30f);
        float sx = uiScreenW() - tw - 20.0f;
        uiDrawRect({sx - 8, 8, tw + 16, 26}, {0.5f, 0.1f, 0.05f, 0.85f}, "gui-unsaved");
        uiDrawText(text, sx, 12, 0.30f, {1.0f, 0.6f, 0.4f, 1.0f});
    }

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

    if (uiDebugEnabled() && !mActiveLayoutFile.empty()) {
        GuiLayout& dbgLayout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
        for (const std::string& id : dbgLayout.elementIds()) {
            const GuiElement* de = dbgLayout.get(id);
            if (!de || id == mSelectedId) continue;
            float dsx = uiScaleX(de->x), dsy = uiScaleY(de->y);
            float dsw = uiScaleX(de->w), dsh = uiScaleY(de->h);
            uiDrawRectOutline({dsx, dsy, dsw, dsh}, {0.3f, 0.3f, 0.5f, 0.35f}, "gui-dbg");
            if (de->anchorX == "center") {
                float acx = uiScaleX(de->x + de->w * 0.5f);
                uiDrawRect({acx - 1, dsy, 2, dsh}, {0.5f, 0.5f, 1.0f, 0.3f}, "gui-anchor-x");
            }
            if (de->anchorY == "middle") {
                float acy = uiScaleY(de->y + de->h * 0.5f);
                uiDrawRect({dsx, acy - 1, dsw, 2}, {0.5f, 0.5f, 1.0f, 0.3f}, "gui-anchor-y");
            }
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
