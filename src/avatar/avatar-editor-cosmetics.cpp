// 08 15 2026, 15 30
/* purpose
* Cosmetics tab: pick GLB cosmetics per slot and tweak transforms.
* Choice + transform edits are stored in avatar.json.
* DOES NOT load or render cosmetic meshes itself.
*/
// Cosmetics tab: pick GLB cosmetics per slot (game assets shipped in
// assets/objects/things/cosmetics/) and tweak placement/size/color.
// Choices are stored in avatar.json; the meshes themselves live in-game.
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar.h"
#include "avatar-editor-dropdown.h"
#include "avatar-editor-scroll.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

const char* kSlotKeys[] = {"head", "torso", "arms", "legs"};
const char* kSlotLabels[] = {"Headwear", "Body", "Arms", "Legs"};
const char* kSlotAttach[] = {"head", "torso", "leftArm", "leftLeg"};
constexpr int kSlotCount = 4;

DropdownState gSlotStates[kSlotCount];
UIRect gSlotRects[kSlotCount];
float gSlotItemH = 28.0f;

std::vector<std::string> scanCosmetics()
{
    std::vector<std::string> result;
    const std::string dir = "assets/objects/things/cosmetics";
    if (!std::filesystem::exists(dir))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".glb")
            result.push_back(entry.path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}

CosmeticSlot* findSlot(std::vector<CosmeticSlot>& vec, const char* key)
{
    for (auto& c : vec)
        if (c.slot == key)
            return &c;
    return nullptr;
}

void applyChoiceToSlot(int si, const std::string& choice)
{
    AvatarSystem& av = AvatarSystem::instance();
    auto& cosmetics = const_cast<std::vector<CosmeticSlot>&>(av.current().cosmetics);
    CosmeticSlot* slot = findSlot(cosmetics, kSlotKeys[si]);
    if (slot) {
        if (slot->choice != choice) {
            slot->choice = choice;
            slot->attachTo = kSlotAttach[si];
            avatarEditorApplyCosmeticsToPlayer();
            av.triggerSave();
        }
    } else if (choice != "none") {
        CosmeticSlot ns;
        ns.slot = kSlotKeys[si];
        ns.choice = choice;
        ns.attachTo = kSlotAttach[si];
        cosmetics.push_back(ns);
        avatarEditorApplyCosmeticsToPlayer();
        av.triggerSave();
    }
}

} // anonymous namespace

void drawAvatarCosmeticsTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;

    auto items = scanCosmetics();
    std::vector<std::string> dropdownItems;
    dropdownItems.push_back("none");
    for (auto& it : items)
        dropdownItems.push_back(it);

    float contentH = kSlotCount * 150.0f;
    ScrollState ss;
    beginScroll(win, {px, py, pw, ph}, contentH, ss);

    float y = py;
    float ddW = pw - 16.0f;
    gSlotItemH = 28.0f;

    for (int si = 0; si < kSlotCount; ++si) {
        std::string currentChoice = "none";
        for (auto& c : av.current().cosmetics)
            if (c.slot == kSlotKeys[si]) { currentChoice = c.choice; break; }

        // Sync the dropdown's displayed selection with the stored choice
        // (unless the user is interacting with the dropdown right now).
        if (!gSlotStates[si].open) {
            int curIdx = 0;
            for (int i = 0; i < (int)dropdownItems.size(); ++i)
                if (dropdownItems[i] == currentChoice) { curIdx = i; break; }
            gSlotStates[si].selectedIndex = curIdx;
        }

        uiDrawText(kSlotLabels[si], uiScaleX(px + 4.0f), uiScaleY(y),
                   avatarEditorFont(avatarEditorSectionFontSize), {0.7f, 0.8f, 0.9f, 1.0f});
        y += 26.0f;

        gSlotRects[si] = {px + 8.0f, y, ddW, gSlotItemH};
        drawDropdown(win, gSlotStates[si], px + 8.0f, y, ddW, gSlotItemH, nullptr, dropdownItems);
        y += gSlotItemH + 6.0f;

        // Transform editing for an equipped cosmetic
        if (currentChoice != "none" && !currentChoice.empty()) {
            auto& cosmetics = const_cast<std::vector<CosmeticSlot>&>(av.current().cosmetics);
            CosmeticSlot* slot = findSlot(cosmetics, kSlotKeys[si]);
            if (slot) {
                bool changed = false;
                float sliderW = pw - 8.0f;
                float ox = slot->offset.x, oy = slot->offset.y, oz = slot->offset.z;
                float sc = slot->scale.x;
                float cr = slot->color.r, cg = slot->color.g, cb = slot->color.b;

                changed |= drawEditorSlider(win, "Offset X", px, y, sliderW, ox, -5.0f, 5.0f); y += 28.0f;
                changed |= drawEditorSlider(win, "Offset Y", px, y, sliderW, oy, -5.0f, 5.0f); y += 28.0f;
                changed |= drawEditorSlider(win, "Offset Z", px, y, sliderW, oz, -5.0f, 5.0f); y += 28.0f;
                changed |= drawEditorSlider(win, "Size", px, y, sliderW, sc, 0.1f, 5.0f); y += 28.0f;
                changed |= drawEditorSlider(win, "Red tint", px, y, sliderW, cr, 0.0f, 2.0f); y += 28.0f;
                changed |= drawEditorSlider(win, "Green tint", px, y, sliderW, cg, 0.0f, 2.0f); y += 28.0f;
                changed |= drawEditorSlider(win, "Blue tint", px, y, sliderW, cb, 0.0f, 2.0f); y += 28.0f;

                if (changed) {
                    slot->offset = glm::vec3(ox, oy, oz);
                    slot->scale = glm::vec3(sc);
                    slot->color = glm::vec3(cr, cg, cb);
                    avatarEditorApplyCosmeticsToPlayer();
                    av.triggerSave();
                }
                y += 14.0f;
            }
        }
        y += 20.0f;
    }

    endScroll({px, py, pw, ph}, contentH, ss);
}

// Post-pass: render the open dropdown's item list on top of all panels and
// apply the clicked choice immediately (so the tab's display-sync cannot
// overwrite it next frame).
void drawAvatarCosmeticsOverlay(GLFWwindow* win)
{
    std::vector<std::string> items;
    items.push_back("none");
    for (auto& it : scanCosmetics())
        items.push_back(it);

    for (int si = 0; si < kSlotCount; ++si) {
        int sel = drawDropdownOverlay(win, gSlotStates[si],
                                      gSlotRects[si].x, gSlotRects[si].y,
                                      gSlotRects[si].w, gSlotItemH, items);
        if (sel >= 0 && sel < (int)items.size())
            applyChoiceToSlot(si, items[sel]);
    }
}
