// 08 20 2026, 12 00
/* purpose
* Implements PNG assignment and live image transforms for selected faces.
* Applies every edit to the shared multi-face selection.
* Keeps the live atlas and avatar JSON synchronized through AvatarSystem.
* DOES NOT render the 3D preview or discover PNG files.
*/
#include "avatar-editor-image.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-scroll.h"
#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

ScrollState gImageEditorScroll;

bool firstSelected(int& outPart, int& outFace)
{
    for (int part = 0; part < kPartCount; ++part)
        for (int face = 0; face < kFaceCount; ++face)
            if (gSelectedFaces[part][face]) {
                outPart = part;
                outFace = face;
                return true;
            }
    return false;
}

template <typename Fn>
void forSelected(Fn&& fn)
{
    for (int part = 0; part < kPartCount; ++part)
        for (int face = 0; face < kFaceCount; ++face)
            if (gSelectedFaces[part][face])
                fn(part, face);
}

void saveChanged(AvatarSystem& av)
{
    avatarEditorRefreshPreview();
    av.triggerSave();
}

} // namespace

void drawAvatarImageEditorTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;
    ensureAvatarEditorSelection();

    if (gEditingCosmeticImage) {
        CosmeticSlot* cosmetic = avatarEditorSelectedCosmetic();
        if (!cosmetic) {
            gEditingCosmeticImage = false;
        } else {
            beginScroll(win, {px, py, pw, ph}, 520.0f, gImageEditorScroll);
            float y = py;
            const std::string title = editorLabelText("imageEditorCosmeticTitle", "COSMETIC IMAGE EDITOR");
            uiDrawText(title.c_str(), uiScaleX(px), uiScaleY(y),
                       editorLabelFontSize("imageEditorCosmeticTitle", avatarEditorSectionFontSize), {0.4f,0.75f,0.55f,1});
            y += 32.0f;
            uiDrawText(cosmetic->id.empty() ? cosmetic->glb.c_str() : cosmetic->id.c_str(),
                       uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorHintFontSize),
                       {0.7f,0.8f,0.9f,1});
            y += 30.0f;
            const std::string applyPngLabel = editorLabelText("applySelectedPngButton", "Apply selected PNG");
            if (editorButton(win, applyPngLabel.c_str(),
                             {px, y, pw, 30.0f}, {0.18f,0.48f,0.28f,1},
                             avatarEditorButtonFontSize).clicked && !gSelectedTexture.empty()) {
                cosmetic->texture.image = gSelectedTexture;
                avatarEditorApplyCosmeticsToPlayer();
                av.triggerSave();
            }
            y += 42.0f;
            const float sliderW = pw - 8.0f;
            bool changed = false;
            changed |= drawEditorSlider(win, "Image X", px, y, sliderW, cosmetic->texture.offsetX, -1000.0f, 1000.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Image Y", px, y, sliderW, cosmetic->texture.offsetY, -1000.0f, 1000.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Image scale X", px, y, sliderW, cosmetic->texture.scaleX, 0.05f, 10.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Image scale Y", px, y, sliderW, cosmetic->texture.scaleY, 0.05f, 10.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Image rotate", px, y, sliderW, cosmetic->texture.rotation, -180.0f, 180.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Red multiplier", px, y, sliderW, cosmetic->texture.color.r, 0.0f, 10.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Green multiplier", px, y, sliderW, cosmetic->texture.color.g, 0.0f, 10.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Blue multiplier", px, y, sliderW, cosmetic->texture.color.b, 0.0f, 10.0f); y += 28.0f;
            changed |= drawEditorSlider(win, "Opacity", px, y, sliderW, cosmetic->texture.opacity, 0.0f, 1.0f);
            if (changed) {
                avatarEditorApplyCosmeticsToPlayer();
                av.triggerSave();
            }
            endScroll({px, py, pw, ph}, 520.0f, gImageEditorScroll);
            return;
        }
    }

    int firstPart = 0;
    int firstFace = 0;
    if (!firstSelected(firstPart, firstFace)) {
        const std::string hint = editorLabelText("imageEditorSelectionHint", "Select one or more faces in Part Picker first.");
        uiDrawText(hint.c_str(),
                   uiScaleX(px), uiScaleY(py + 24.0f),
                   avatarEditorFont(avatarEditorHintFontSize),
                   {0.8f, 0.65f, 0.4f, 1.0f});
        return;
    }

    const float contentH = 900.0f;
    beginScroll(win, {px, py, pw, ph}, contentH, gImageEditorScroll);
    float y = py;

    const FaceSettings selected = av.current().resolve(partKey(firstPart), faceKey(firstFace));
    const std::string title = editorLabelText("imageEditorTitle", "IMAGE EDITOR");
    uiDrawText(title.c_str(), uiScaleX(px), uiScaleY(y),
               editorLabelFontSize("imageEditorTitle", avatarEditorSectionFontSize),
               {0.4f, 0.75f, 0.55f, 1.0f});
    y += 28.0f;

    char selectedLabel[96];
    snprintf(selectedLabel, sizeof(selectedLabel), "%d faces: %s / %s",
             selectedFaceCount(), partLabel(firstPart), faceLabel(firstFace));
    uiDrawText(selectedLabel, uiScaleX(px), uiScaleY(y),
               avatarEditorFont(avatarEditorSummaryFontSize),
               {0.9f, 0.95f, 1.0f, 1.0f});
    y += 34.0f;

    std::string pngText = gSelectedTexture.empty()
        ? "No PNG selected in PNG Library"
        : "Selected PNG: " + gSelectedTexture;
    uiDrawText(pngText.c_str(), uiScaleX(px), uiScaleY(y),
               avatarEditorFont(avatarEditorHintFontSize),
               {0.6f, 0.8f, 0.65f, 1.0f});
    y += 28.0f;

    const GuiElement* applyPng = avatarEditorLayout().get("applySelectedPngButton");
    if (editorButton(win, editorLabelText("applySelectedPngButton", "Apply selected PNG").c_str(),
                     {px, y, pw * 0.48f, 36.0f}, layoutBg(applyPng, {0.18f, 0.48f, 0.28f, 1.0f}),
                     editorFontSize(applyPng, avatarEditorButtonFontSize)).clicked) {
        if (!gSelectedTexture.empty()) {
            forSelected([&](int part, int face) {
                av.setPartFace(partKey(part), faceKey(face), gSelectedTexture);
            });
            saveChanged(av);
        }
    }
    const GuiElement* clearFaces = avatarEditorLayout().get("clearSelectedFacesButton");
    if (editorButton(win, editorLabelText("clearSelectedFacesButton", "Clear selected faces").c_str(),
                     {px + pw * 0.52f, y, pw * 0.48f, 36.0f},
                     layoutBg(clearFaces, {0.42f, 0.16f, 0.16f, 1.0f}),
                     editorFontSize(clearFaces, avatarEditorButtonFontSize)).clicked) {
        forSelected([&](int part, int face) {
            av.setPartFace(partKey(part), faceKey(face), "");
            av.setPartFaceTransform(partKey(part), faceKey(face), FaceTransform{});
        });
        saveChanged(av);
    }
    y += 46.0f;

    FaceTransform next = selected.transform;
    bool changed = false;
    const float sliderW = pw - 8.0f;
    changed |= drawEditorSlider(win, "Offset X", px, y, sliderW, next.offsetX, -500.0f, 500.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Offset Y", px, y, sliderW, next.offsetY, -500.0f, 500.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale X", px, y, sliderW, next.scaleX, 0.05f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale Y", px, y, sliderW, next.scaleY, 0.05f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Rotation", px, y, sliderW, next.rotation, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Red tint", px, y, sliderW, next.color.r, 0.0f, 2.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Green tint", px, y, sliderW, next.color.g, 0.0f, 2.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Blue tint", px, y, sliderW, next.color.b, 0.0f, 2.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Opacity", px, y, sliderW, next.transparency, 0.0f, 1.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Saturation", px, y, sliderW, next.saturation, -10.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Brightness", px, y, sliderW, next.brightness, -10.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Contrast", px, y, sliderW, next.contrast, 0.0f, 3.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Hue shift", px, y, sliderW, next.hueShift, -180.0f, 180.0f); y += 28.0f;

    if (editorButton(win, next.stretchMode == 0 ? "Stretch mode: ON" : "Stretch mode: OFF",
                     {px, y, pw * 0.48f, 30.0f},
                     next.stretchMode == 0 ? glm::vec4{0.2f,0.5f,0.3f,1.0f} : glm::vec4{0.15f,0.22f,0.32f,1.0f},
                     avatarEditorButtonFontSize).clicked)
        next.stretchMode = 0;
    if (editorButton(win, next.stretchMode == 1 ? "Crop mode: ON" : "Crop mode: OFF",
                     {px + pw * 0.52f, y, pw * 0.48f, 30.0f},
                     next.stretchMode == 1 ? glm::vec4{0.2f,0.5f,0.3f,1.0f} : glm::vec4{0.15f,0.22f,0.32f,1.0f},
                     avatarEditorButtonFontSize).clicked)
        next.stretchMode = 1;

    if (next.stretchMode != selected.transform.stretchMode)
        changed = true;
    if (changed) {
        forSelected([&](int part, int face) {
            av.setPartFaceTransform(partKey(part), faceKey(face), next);
        });
        saveChanged(av);
    }

    endScroll({px, py, pw, ph}, contentH, gImageEditorScroll);
}
