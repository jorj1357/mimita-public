// 08 20 2026, 00 00
/* purpose
* Draws the editable cosmetic-instance list and cosmetic controls.
* Keeps GLB discovery, selection, transforms, and image settings live.
* Uses AvatarSystem as the one owner of saved cosmetic data.
* DOES NOT load GLB meshes or own the renderer.
* DOES NOT create texture atlases or paint pixels.
*/
#include "avatar-editor-cosmetics.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-dropdown.h"
#include "avatar-editor-scroll.h"
#include "avatar.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

int gSelectedCosmetic = -1;
bool gEditingCosmeticImage = false;

namespace {

ScrollState gCosmeticsScroll;

DropdownState gAvailableState;
DropdownState gInstanceState;
DropdownState gAnchorState;
DropdownState gTextureState;
UIRect gAvailableRect{};
UIRect gInstanceRect{};
UIRect gAnchorRect{};
UIRect gTextureRect{};
std::vector<std::string> gAvailableItems;
std::vector<std::string> gInstanceItems;
std::vector<std::string> gAnchorItems;
std::vector<std::string> gTextureItems;
const char* kAnchors[] = {
    "head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"
};

std::vector<std::string> scanGlbs()
{
    std::vector<std::string> result;
    const std::filesystem::path dir("assets/objects/things/cosmetics");
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".glb")
                result.push_back(entry.path().filename().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string cosmeticName(const CosmeticSlot& cosmetic, int index)
{
    const std::string name = cosmetic.id.empty()
        ? (cosmetic.glb.empty() ? cosmetic.choice : cosmetic.glb)
        : cosmetic.id;
    return std::to_string(index + 1) + ": " + name;
}

int selectedAvailableIndex()
{
    return gAvailableState.selectedIndex > 0
        ? gAvailableState.selectedIndex - 1 : -1;
}

CosmeticSlot* selectedCosmetic(AvatarSystem& avatar)
{
    auto& list = const_cast<std::vector<CosmeticSlot>&>(avatar.current().cosmetics);
    if (gSelectedCosmetic < 0 || gSelectedCosmetic >= (int)list.size())
        return nullptr;
    return &list[gSelectedCosmetic];
}

void applyCosmeticEdit()
{
    avatarEditorApplyCosmeticsToPlayer();
    AvatarSystem::instance().triggerSave();
}

void addSelectedCosmetic()
{
    AvatarSystem& avatar = AvatarSystem::instance();
    const int available = selectedAvailableIndex();
    if (available < 0 || available >= (int)gAvailableItems.size())
        return;

    CosmeticSlot cosmetic;
    cosmetic.id = gAvailableItems[available];
    cosmetic.glb = gAvailableItems[available];
    cosmetic.choice = cosmetic.glb;
    cosmetic.anchorPart = "torso";
    cosmetic.attachTo = cosmetic.anchorPart;
    cosmetic.enabled = true;
    auto& list = const_cast<std::vector<CosmeticSlot>&>(avatar.current().cosmetics);
    list.push_back(cosmetic);
    gSelectedCosmetic = (int)list.size() - 1;
    gEditingCosmeticImage = true;
    applyCosmeticEdit();
}

void removeSelectedCosmetic()
{
    AvatarSystem& avatar = AvatarSystem::instance();
    auto& list = const_cast<std::vector<CosmeticSlot>&>(avatar.current().cosmetics);
    if (gSelectedCosmetic < 0 || gSelectedCosmetic >= (int)list.size())
        return;
    list.erase(list.begin() + gSelectedCosmetic);
    if (gSelectedCosmetic >= (int)list.size())
        gSelectedCosmetic = (int)list.size() - 1;
    applyCosmeticEdit();
}

void duplicateSelectedCosmetic()
{
    AvatarSystem& avatar = AvatarSystem::instance();
    auto& list = const_cast<std::vector<CosmeticSlot>&>(avatar.current().cosmetics);
    if (gSelectedCosmetic < 0 || gSelectedCosmetic >= (int)list.size())
        return;
    CosmeticSlot copy = list[gSelectedCosmetic];
    copy.id += "_copy";
    list.insert(list.begin() + gSelectedCosmetic + 1, copy);
    ++gSelectedCosmetic;
    applyCosmeticEdit();
}

UIRect panelRelativeRect(const GuiLayout& layout, const char* id,
                         float px, float y, UIRect fallback)
{
    const GuiElement* element = layout.get(id);
    if (!element)
        return fallback;
    return {px + element->x, y + element->y,
            element->w > 0.0f ? element->w : fallback.w,
            element->h > 0.0f ? element->h : fallback.h};
}

void drawCosmeticButtons(GLFWwindow* win, AvatarSystem& avatar,
                         float px, float& y, float pw)
{
    const GuiLayout& layout = avatarEditorLayout();
    const float gap = 6.0f;
    const float buttonW = (pw - gap * 2.0f) / 3.0f;
    UIRect add = panelRelativeRect(layout, "cosmeticAddButton", px, y, {px, y, buttonW, 34.0f});
    UIRect remove = panelRelativeRect(layout, "cosmeticRemoveButton", px, y, {px + buttonW + gap, y, buttonW, 34.0f});
    UIRect duplicate = panelRelativeRect(layout, "cosmeticDuplicateButton", px, y, {px + (buttonW + gap) * 2.0f, y, buttonW, 34.0f});

    const GuiElement* addElem = layout.get("cosmeticAddButton");
    const GuiElement* removeElem = layout.get("cosmeticRemoveButton");
    const GuiElement* duplicateElem = layout.get("cosmeticDuplicateButton");
    if (editorButton(win, addElem ? addElem->text.c_str() : "Add",
                     add, layoutBg(addElem, {0.18f, 0.42f, 0.28f, 1.0f}),
                     editorFontSize(addElem, avatarEditorSmallButtonFontSize)).clicked)
        addSelectedCosmetic();
    if (editorButton(win, removeElem ? removeElem->text.c_str() : "Remove",
                     remove, layoutBg(removeElem, {0.4f, 0.14f, 0.14f, 1.0f}),
                     editorFontSize(removeElem, avatarEditorSmallButtonFontSize)).clicked)
        removeSelectedCosmetic();
    if (editorButton(win, duplicateElem ? duplicateElem->text.c_str() : "Duplicate",
                     duplicate, layoutBg(duplicateElem, {0.15f, 0.25f, 0.4f, 1.0f}),
                     editorFontSize(duplicateElem, avatarEditorSmallButtonFontSize)).clicked)
        duplicateSelectedCosmetic();
    y += std::max({add.h, remove.h, duplicate.h}) + 10.0f;
    (void)avatar;
}

void drawCosmeticControls(GLFWwindow* win, AvatarSystem& avatar,
                          CosmeticSlot& cosmetic, float px, float& y, float pw)
{
    const std::string transformTitle = editorLabelText("cosmeticsTransformTitle", "COSMETIC TRANSFORM");
    uiDrawText(transformTitle.c_str(), uiScaleX(px), uiScaleY(y),
               editorLabelFontSize("cosmeticsTransformTitle", avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 24.0f;

    bool changed = false;
    const std::string enabledLabel = editorLabelText("cosmeticsEnabled", "Enabled");
    changed |= uiCheckbox(win, enabledLabel.c_str(), {px, y, pw, 32.0f}, &cosmetic.enabled);
    y += 34.0f;

    gAnchorItems.assign(std::begin(kAnchors), std::end(kAnchors));
    int anchorIndex = 0;
    if (!gAnchorState.open) {
        for (int i = 0; i < (int)gAnchorItems.size(); ++i)
            if (gAnchorItems[i] == cosmetic.anchorPart)
                anchorIndex = i;
        gAnchorState.selectedIndex = anchorIndex;
    }
    gAnchorRect = {px, y, pw, 28.0f};
    drawDropdown(win, gAnchorState, px, y, pw, 28.0f, "", gAnchorItems);
    y += 36.0f;

    const float sliderW = pw - 8.0f;
    changed |= drawEditorSlider(win, "Offset X", px, y, sliderW, cosmetic.offset.x, -5.0f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Offset Y", px, y, sliderW, cosmetic.offset.y, -5.0f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Offset Z", px, y, sliderW, cosmetic.offset.z, -5.0f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Rotate X", px, y, sliderW, cosmetic.rotation.x, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Rotate Y", px, y, sliderW, cosmetic.rotation.y, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Rotate Z", px, y, sliderW, cosmetic.rotation.z, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale X", px, y, sliderW, cosmetic.scale.x, 0.05f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale Y", px, y, sliderW, cosmetic.scale.y, 0.05f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale Z", px, y, sliderW, cosmetic.scale.z, 0.05f, 5.0f); y += 34.0f;

    const std::string imageTitle = editorLabelText("cosmeticsImageTitle", "COSMETIC IMAGE");
    uiDrawText(imageTitle.c_str(), uiScaleX(px), uiScaleY(y),
               editorLabelFontSize("cosmeticsImageTitle", avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 24.0f;

    gTextureItems.clear();
    gTextureItems.push_back("none");
    for (const std::string& png : avatar.listPngs(avatar.currentName()))
        gTextureItems.push_back(png);
    if (!gTextureState.open) {
        int textureIndex = 0;
        for (int i = 0; i < (int)gTextureItems.size(); ++i)
            if (gTextureItems[i] == cosmetic.texture.image)
                textureIndex = i;
        gTextureState.selectedIndex = textureIndex;
    }
    gTextureRect = {px, y, pw, 28.0f};
    drawDropdown(win, gTextureState, px, y, pw, 28.0f, "", gTextureItems);
    y += 36.0f;
    const GuiElement* applyPng = avatarEditorLayout().get("applySelectedPngButton");
    const std::string applyPngText = editorLabelText("applySelectedPngButton", "Apply selected PNG");
    if (editorButton(win, applyPngText.c_str(),
                     {px, y, pw, 36.0f}, layoutBg(applyPng, {0.18f,0.42f,0.28f,1.0f}),
                     editorFontSize(applyPng, avatarEditorButtonFontSize)).clicked && !gSelectedTexture.empty()) {
        cosmetic.texture.image = gSelectedTexture;
        applyCosmeticEdit();
    }
    y += 38.0f;

    changed |= drawEditorSlider(win, "Image X", px, y, sliderW, cosmetic.texture.offsetX, -1000.0f, 1000.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Image Y", px, y, sliderW, cosmetic.texture.offsetY, -1000.0f, 1000.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Image scale X", px, y, sliderW, cosmetic.texture.scaleX, 0.05f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Image scale Y", px, y, sliderW, cosmetic.texture.scaleY, 0.05f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Image rotate", px, y, sliderW, cosmetic.texture.rotation, 0.0f, 360.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Red multiplier", px, y, sliderW, cosmetic.texture.color.r, 0.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Green multiplier", px, y, sliderW, cosmetic.texture.color.g, 0.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Blue multiplier", px, y, sliderW, cosmetic.texture.color.b, 0.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Brightness", px, y, sliderW, cosmetic.texture.brightness, 0.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Opacity", px, y, sliderW, cosmetic.texture.opacity, 0.0f, 1.0f); y += 34.0f;

    if (changed)
        applyCosmeticEdit();
}

} // namespace

CosmeticSlot* avatarEditorSelectedCosmetic()
{
    return selectedCosmetic(AvatarSystem::instance());
}

void drawAvatarCosmeticsTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& avatar = AvatarSystem::instance();
    if (!avatar.hasAvatar())
        return;

    gAvailableItems = {"none"};
    for (const std::string& glb : scanGlbs())
        gAvailableItems.push_back(glb);
    gInstanceItems.clear();
    for (int i = 0; i < (int)avatar.current().cosmetics.size(); ++i)
        gInstanceItems.push_back(cosmeticName(avatar.current().cosmetics[i], i));

    if (gSelectedCosmetic >= (int)gInstanceItems.size())
        gSelectedCosmetic = (int)gInstanceItems.size() - 1;
    gInstanceState.selectedIndex = std::max(gSelectedCosmetic, 0);
    gAvailableState.selectedIndex = std::clamp(gAvailableState.selectedIndex, 0,
                                               std::max(0, (int)gAvailableItems.size() - 1));

    const float contentH = 1150.0f;
    beginScroll(win, {px, py, pw, ph}, contentH, gCosmeticsScroll);
    float y = py;

    const std::string availableTitle = editorLabelText("cosmeticsAvailableTitle", "AVAILABLE GLB COSMETICS");
    uiDrawText(availableTitle.c_str(), uiScaleX(px), uiScaleY(y),
               editorLabelFontSize("cosmeticsAvailableTitle", avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 24.0f;
    gAvailableRect = {px, y, pw, 28.0f};
    drawDropdown(win, gAvailableState, px, y, pw, 28.0f, "", gAvailableItems);
    y += 36.0f;
    drawCosmeticButtons(win, avatar, px, y, pw);

    const std::string instancesTitle = editorLabelText("cosmeticsInstancesTitle", "INSTANCES");
    uiDrawText(instancesTitle.c_str(), uiScaleX(px), uiScaleY(y),
               editorLabelFontSize("cosmeticsInstancesTitle", avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 24.0f;
    gInstanceRect = {px, y, pw, 28.0f};
    if (gInstanceItems.empty())
        uiDrawText(editorLabelText("cosmeticsEmpty", "No cosmetics added yet.").c_str(), uiScaleX(px), uiScaleY(y + 4.0f),
                   editorLabelFontSize("cosmeticsEmpty", avatarEditorHintFontSize), {0.5f, 0.6f, 0.7f, 1.0f});
    else
        drawDropdown(win, gInstanceState, px, y, pw, 28.0f, "", gInstanceItems);
    y += 40.0f;

    if (CosmeticSlot* cosmetic = selectedCosmetic(avatar))
        drawCosmeticControls(win, avatar, *cosmetic, px, y, pw);
    else
        uiDrawText(editorLabelText("cosmeticsChooseHint", "Choose a GLB, then press Add.").c_str(), uiScaleX(px), uiScaleY(y),
                   editorLabelFontSize("cosmeticsChooseHint", avatarEditorHintFontSize), {0.5f, 0.6f, 0.7f, 1.0f});

    endScroll({px, py, pw, ph}, contentH, gCosmeticsScroll);
}

void drawAvatarCosmeticsOverlay(GLFWwindow* win)
{
    AvatarSystem& avatar = AvatarSystem::instance();
    const int available = drawDropdownOverlay(win, gAvailableState,
                                              gAvailableRect.x, gAvailableRect.y,
                                              gAvailableRect.w, gAvailableRect.h,
                                              gAvailableItems);
    if (available >= 0)
        gAvailableState.selectedIndex = available;

    const int instance = drawDropdownOverlay(win, gInstanceState,
                                             gInstanceRect.x, gInstanceRect.y,
                                             gInstanceRect.w, gInstanceRect.h,
                                             gInstanceItems);
    if (instance >= 0) {
        gSelectedCosmetic = instance;
        gEditingCosmeticImage = true;
        gInstanceState.selectedIndex = instance;
    }

    const int anchor = drawDropdownOverlay(win, gAnchorState,
                                           gAnchorRect.x, gAnchorRect.y,
                                           gAnchorRect.w, gAnchorRect.h,
                                           gAnchorItems);
    if (anchor >= 0) {
        if (CosmeticSlot* cosmetic = selectedCosmetic(avatar)) {
            cosmetic->anchorPart = gAnchorItems[anchor];
            cosmetic->attachTo = cosmetic->anchorPart;
            applyCosmeticEdit();
        }
    }

    const int texture = drawDropdownOverlay(win, gTextureState,
                                            gTextureRect.x, gTextureRect.y,
                                            gTextureRect.w, gTextureRect.h,
                                            gTextureItems);
    if (texture >= 0) {
        if (CosmeticSlot* cosmetic = selectedCosmetic(avatar)) {
            cosmetic->texture.image = texture == 0 ? "" : gTextureItems[texture];
            applyCosmeticEdit();
        }
    }
}
