#include "gui/gui-editor.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"

float GuiEditor::roundValue(float val)
{
    float nearest = std::round(val);
    if (std::abs(val - nearest) <= 1.2f) return nearest;
    float nearest5 = std::round(val / 5.0f) * 5.0f;
    if (std::abs(val - nearest5) <= 3.0f) return nearest5;
    return val;
}

float GuiEditor::roundCoord(float val) { return roundValue(val); }

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


