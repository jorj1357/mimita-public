// 08 15 2026, 15 30
/* purpose
* Draws the PNG library thumbnail grid with scroll + hover.
* Clicking a thumbnail selects it for the face editor.
* DOES NOT import files and does not own avatar data.
*/
// Draws the PNG library panel: a scrollable grid of the current outfit's
// PNGs. Clicking a PNG selects it (drives the face editor's "Use Selected
// PNG" action). Import happens through drag & drop (avatarEditorHandleDrop).
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-scroll.h"
#include "avatar.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace {

struct HoverAnim {
    float progress = 0.0f;
};

} // anonymous namespace

void drawAvatarLibrary(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;

    const std::string avatarName = av.currentName();
    auto pngs = av.listPngs(avatarName);
    if (pngs.empty()) {
        uiDrawText("No PNGs yet.", uiScaleX(px), uiScaleY(py + 20.0f),
                   avatarEditorFont(avatarEditorHintFontSize), {0.5f, 0.6f, 0.7f, 1.0f});
        uiDrawText("Drag PNGs here to import.", uiScaleX(px), uiScaleY(py + 48.0f),
                   avatarEditorFont(avatarEditorHintFontSize), {0.4f, 0.5f, 0.6f, 1.0f});
        return;
    }

    const GuiLayout& layout = avatarEditorLayout();
    glm::vec4 bgNormal = layoutBg(layout.get("thumbBgNormal"), {0.07f, 0.08f, 0.12f, 1.0f});
    glm::vec4 bgSelected = layoutBg(layout.get("thumbBgSelected"), {0.2f, 0.5f, 0.3f, 1.0f});
    glm::vec4 outlineSel = layout.get("thumbOutlineSelected")
        ? layout.get("thumbOutlineSelected")->getTextColorVec()
        : glm::vec4{0.3f, 0.8f, 0.5f, 1.0f};

    const int cols = 3;
    const float gap = 6.0f;
    const float labelH = 22.0f;
    const float padding = 4.0f;

    float thumbSize = (pw - padding * 2.0f - (cols - 1) * gap) / cols;
    float itemH = thumbSize + labelH;
    float contentH = ((int)pngs.size() + cols - 1) / cols * itemH;

    ScrollState ss;
    beginScroll(win, {px, py, pw, ph}, contentH, ss);

    static std::unordered_map<std::string, HoverAnim> gHoverAnims;
    float tx = px + padding;

    for (int i = 0; i < (int)pngs.size(); ++i) {
        int col = i % cols;
        int row = i / cols;
        float ix = tx + col * (thumbSize + gap);
        float iy = py + row * itemH;

        std::string fullPath = av.avatarPath(avatarName) + "/" + pngs[i];
        bool isSelected = (pngs[i] == gSelectedTexture);

        auto btn = uiButton(win, "", {ix, iy, thumbSize, thumbSize}, bgNormal);

        HoverAnim& anim = gHoverAnims[pngs[i]];
        float target = btn.hovered ? 1.0f : 0.0f;
        anim.progress += (target - anim.progress) * 0.15f;

        float hoverScale = 1.0f + anim.progress * 0.05f;
        float expand = (hoverScale - 1.0f) * thumbSize * 0.5f;

        glm::vec4 bgCol = isSelected ? bgSelected : bgNormal;
        if (btn.hovered && !isSelected)
            bgCol += glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);

        float sx = uiScaleX(ix), sy = uiScaleY(iy), sSize = uiScaleX(thumbSize);
        UIRect bgScreen = {sx - expand, sy - expand, sSize * hoverScale, sSize * hoverScale};
        uiDrawRect(bgScreen, bgCol, "lib-thumb-bg");

        float hoverExpand = (hoverScale - 1.0f) * sSize * 0.5f;
        uiDrawImageFit(fullPath.c_str(),
                       {sx - hoverExpand, sy - hoverExpand, sSize * hoverScale, sSize * hoverScale},
                       true);

        if (isSelected) {
            uiDrawRectOutline({bgScreen.x - 3.0f, bgScreen.y - 3.0f,
                               bgScreen.w + 6.0f, bgScreen.h + 6.0f},
                              outlineSel + glm::vec4(0.2f, 0.0f, 0.0f, 0.0f), "lib-thumb-sel");
        }
        if (btn.hovered && !isSelected)
            uiDrawRectOutline(bgScreen, {0.5f, 0.7f, 1.0f, 0.4f + anim.progress * 0.3f}, "lib-thumb-hover");

        if (btn.clicked)
            gSelectedTexture = pngs[i];

        std::string label = pngs[i];
        if (label.size() > 14)
            label = label.substr(0, 12) + "...";
        uiDrawText(label.c_str(), uiScaleX(ix), uiScaleY(iy + thumbSize + 4.0f),
                   avatarEditorFont(avatarEditorHintFontSize), {0.6f, 0.7f, 0.8f, 1.0f});
    }

    if (gHoverAnims.size() > pngs.size() * 2) {
        for (auto it = gHoverAnims.begin(); it != gHoverAnims.end(); ) {
            bool found = false;
            for (const auto& p : pngs)
                if (it->first == p) { found = true; break; }
            if (!found && it->second.progress < 0.01f)
                it = gHoverAnims.erase(it);
            else
                ++it;
        }
    }

    endScroll({px, py, pw, ph}, contentH, ss);
}
