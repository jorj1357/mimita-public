// 08 15 2026, 15 30
/* purpose
* Outfits panel: list and manage outfit folders.
* Selection loads + applies the outfit to the player.
* DOES NOT draw popups or own avatar data.
*/
// Outfits panel: list, create, select, rename, copy, delete outfits.
// An outfit is a folder under assets/avatars/<name>/.
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string uniqueOutfitName(AvatarSystem& av, const std::string& base)
{
    std::string test = base;
    int counter = 1;
    auto existing = av.listAvatars();
    while (std::find(existing.begin(), existing.end(), test) != existing.end())
        test = base + " " + std::to_string(counter++);
    return test;
}

} // anonymous namespace

// Draws the outfits list panel. Popup open/close flags are handled here;
// the popups themselves are drawn in avatar-editor-popups.cpp.
void drawAvatarOutfitsPanel(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    const GuiLayout& layout = avatarEditorLayout();

    // "+ New Outfit"
    const GuiElement* newBtn = layout.get("outfitsNewButton");
    if (newBtn && newBtn->visible) {
        if (uiButton(win, newBtn->text.c_str(),
                     {px + 8.0f, py, pw - 16.0f, 28.0f},
                     newBtn->getBackgroundColorVec()).clicked) {
            avatarEditorLoadOutfit(uniqueOutfitName(av, "New Outfit"));
        }
    }
    py += 38.0f;

    const float entryH = 28.0f;
    const float gap = 3.0f;
    const float btnW = 60.0f;
    const float btnH = 22.0f;

    glm::vec4 entrySelCol = layoutBg(layout.get("outfitEntrySelected"), {0.2f, 0.48f, 0.28f, 1.0f});
    glm::vec4 entryNormCol = layoutBg(layout.get("outfitEntryNormal"), {0.08f, 0.09f, 0.14f, 1.0f});
    glm::vec4 rnCol = layoutBg(layout.get("outfitRenameBtn"), {0.15f, 0.25f, 0.4f, 1.0f});
    glm::vec4 cpCol = layoutBg(layout.get("outfitCopyBtn"), {0.2f, 0.35f, 0.25f, 1.0f});
    glm::vec4 dlCol = layoutBg(layout.get("outfitDeleteBtn"), {0.4f, 0.12f, 0.12f, 1.0f});

    float y = py;
    const float listX = px + 8.0f;
    const float listW = pw - 16.0f;

    auto outfits = av.listAvatars();
    for (auto& o : outfits) {
        bool isCurrent = (o == av.currentName());
        glm::vec4 col = isCurrent ? entrySelCol : entryNormCol;

        UIRect entryRect = {listX, y, listW, entryH};
        UIRect screenEntry = {uiScaleX(listX), uiScaleY(y), uiScaleX(listW), uiScaleY(entryH)};
        uiDrawRect(screenEntry, col, "outfit-entry");
        if (isCurrent)
            uiDrawRectOutline(screenEntry, {0.3f, 0.8f, 0.5f, 1.0f}, "outfit-sel");

        uiDrawText(o.c_str(), screenEntry.x + 6.0f, screenEntry.y + 4.0f, 0.28f, {1, 1, 1, 1});

        if (uiButton(win, "", entryRect, {0, 0, 0, 0}).clicked && !isCurrent)
            avatarEditorLoadOutfit(o);

        if (isCurrent) {
            float bY = y + entryH + 2.0f;
            if (uiButton(win, "Rename", {listX, bY, btnW, btnH}, rnCol).clicked) {
                gRenamePopupOpen = true;
                memset(gRenameBuf, 0, sizeof(gRenameBuf));
                strncpy(gRenameBuf, av.currentName().c_str(), sizeof(gRenameBuf) - 1);
            }
            if (uiButton(win, "Copy", {listX + btnW + 4.0f, bY, btnW, btnH}, cpCol).clicked)
                av.duplicateOutfit(av.currentName(), uniqueOutfitName(av, av.currentName() + " Copy"));
            if (uiButton(win, "Delete", {listX + 2 * (btnW + 4.0f), bY, btnW, btnH}, dlCol).clicked)
                gDeleteConfirmOpen = true;
            y += btnH + 6.0f;
        }
        y += entryH + gap;
    }

    if (outfits.empty())
        uiDrawText("No outfits yet. Create one!", uiScaleX(px + 8.0f), uiScaleY(py), 0.30f,
                   {0.4f, 0.5f, 0.6f, 1.0f});
}
