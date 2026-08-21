// 08 15 2026, 15 30
/* purpose
* Draws the PNG library as large one-per-row tiles with scroll + hover.
* Clicking a thumbnail selects it for the face editor.
* DOES NOT import files and does not own avatar data.
*/
// Draws the PNG library panel: a scrollable one-column list of the current outfit's
// PNGs. Clicking a PNG selects it (drives the face editor's "Use Selected
// PNG" action). Import happens through drag & drop (avatarEditorHandleDrop).
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-scroll.h"
#include "avatar.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

ScrollState gPngLibraryScroll;

} // anonymous namespace

void drawAvatarLibrary(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;

    const std::string avatarName = av.currentName();
    auto pngs = av.listPngs(avatarName);
    if (pngs.empty()) {
        const std::string emptyText = editorLabelText("pngLibraryEmpty", "No PNGs yet.");
        const std::string importText = editorLabelText("pngLibraryImportHint", "Drag PNGs here to import.");
        uiDrawText(emptyText.c_str(), uiScaleX(px), uiScaleY(py + 20.0f),
                   editorLabelFontSize("pngLibraryEmpty", avatarEditorHintFontSize), {0.5f, 0.6f, 0.7f, 1.0f});
        uiDrawText(importText.c_str(), uiScaleX(px), uiScaleY(py + 48.0f),
                   editorLabelFontSize("pngLibraryImportHint", avatarEditorHintFontSize), {0.4f, 0.5f, 0.6f, 1.0f});
        return;
    }

    const GuiLayout& layout = avatarEditorLayout();
    glm::vec4 bgNormal = layoutBg(layout.get("thumbBgNormal"), {0.07f, 0.08f, 0.12f, 1.0f});
    glm::vec4 bgSelected = layoutBg(layout.get("thumbBgSelected"), {0.2f, 0.5f, 0.3f, 1.0f});
    glm::vec4 outlineSel = layout.get("thumbOutlineSelected")
        ? layout.get("thumbOutlineSelected")->getTextColorVec()
        : glm::vec4{0.3f, 0.8f, 0.5f, 1.0f};

    const float gap = 6.0f;
    const float labelH = 28.0f;
    const float padding = 4.0f;

    const float tileH = std::max(150.0f, std::min(220.0f, pw * 0.42f));
    const float imageSize = tileH - labelH;
    const float itemH = tileH + gap;
    const float contentH = (float)pngs.size() * itemH + padding * 2.0f;

    beginScroll(win, {px, py, pw, ph}, contentH, gPngLibraryScroll);

    for (int i = 0; i < (int)pngs.size(); ++i) {
        float ix = px + padding;
        float iy = py + padding + i * itemH;
        float tileW = pw - padding * 2.0f;

        std::string fullPath = av.avatarPath(avatarName) + "/" + pngs[i];
        bool isSelected = (pngs[i] == gSelectedTexture);

        auto btn = uiButton(win, "", {ix, iy, tileW, tileH}, bgNormal);

        glm::vec4 bgCol = isSelected ? bgSelected : bgNormal;
        if (btn.hovered && !isSelected)
            bgCol += glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);

        float sx = uiScaleX(ix), sy = uiScaleY(iy), sw = uiScaleX(tileW), sh = uiScaleY(tileH);
        UIRect bgScreen = {sx, sy, sw, sh};
        uiDrawRect(bgScreen, bgCol, "lib-thumb-bg");

        uiDrawImageFit(fullPath.c_str(),
                       {sx + uiScaleX((tileW - imageSize) * 0.5f),
                        sy + uiScaleY(4.0f),
                        uiScaleX(imageSize), uiScaleY(imageSize - 8.0f)},
                       true);

        if (isSelected) {
            uiDrawRectOutline({bgScreen.x - 3.0f, bgScreen.y - 3.0f,
                               bgScreen.w + 6.0f, bgScreen.h + 6.0f},
                              outlineSel + glm::vec4(0.2f, 0.0f, 0.0f, 0.0f), "lib-thumb-sel");
        }
        if (btn.hovered && !isSelected)
            uiDrawRectOutline(bgScreen, {0.5f, 0.7f, 1.0f, 0.7f}, "lib-thumb-hover");

        if (btn.clicked)
            gSelectedTexture = pngs[i];

        std::string label = pngs[i];
        if (label.size() > 14)
            label = label.substr(0, 12) + "...";
        uiDrawText(label.c_str(), uiScaleX(ix + 8.0f), uiScaleY(iy + imageSize + 4.0f),
                   avatarEditorFont(avatarEditorHintFontSize), {0.6f, 0.7f, 0.8f, 1.0f});
    }

    endScroll({px, py, pw, ph}, contentH, gPngLibraryScroll);
}
