// 08 15 2026, 15 30
/* purpose
* Faces tab: pick a face, assign a PNG, edit its transform live.
* Provides copy/paste face and part and bulk apply helpers.
* DOES NOT own avatar data or rebuild the atlas itself.
*/
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar.h"
#include "avatar-editor-scroll.h"
#include "entities/player.h"

#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace {

constexpr float kRowGap = 6.0f;
constexpr float kRowH = 28.0f;

float rowButtonW(float pw)
{
    return (pw - (kPartCount - 1) * kRowGap) / kPartCount;
}

// ── Body part + face side selector rows ─────────────────────────────
float drawPartRow(GLFWwindow* win, float px, float y, float pw)
{
    const GuiLayout& layout = avatarEditorLayout();
    const char* ids[] = {"partHead", "partTorso", "partLArm", "partRArm", "partLLeg", "partRLeg"};
    glm::vec4 base = layoutBg(layout.get("partHead"), {0.12f, 0.18f, 0.3f, 1.0f});
    glm::vec4 active = layoutBg(layout.get("partHead"), base);
    if (layout.get("partHead") && layout.get("partHead")->hasPressedColor())
        active = layout.get("partHead")->getPressedColorVec();

    uiDrawText("PART", uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 22.0f;

    float bw = rowButtonW(pw);
    for (int i = 0; i < kPartCount; ++i) {
        glm::vec4 col = (i == gSelectedPart) ? active : base;
        const GuiElement* e = layout.get(ids[i]);
        if (editorButton(win, e ? e->text.c_str() : partLabel(i),
                         {px + i * (bw + kRowGap), y, bw, kRowH}, col,
                         editorFontSize(e, avatarEditorSmallTabFontSize)).clicked)
            gSelectedPart = i;
    }
    return y + 38.0f;
}

float drawFaceRow(GLFWwindow* win, float px, float y, float pw)
{
    const GuiLayout& layout = avatarEditorLayout();
    const char* ids[] = {"faceFront", "faceBack", "faceLeft", "faceRight", "faceTop", "faceBottom"};
    glm::vec4 base = layoutBg(layout.get("faceFront"), {0.12f, 0.18f, 0.3f, 1.0f});
    glm::vec4 active = layoutBg(layout.get("faceFront"), base);
    if (layout.get("faceFront") && layout.get("faceFront")->hasPressedColor())
        active = layout.get("faceFront")->getPressedColorVec();

    uiDrawText("SIDE", uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 22.0f;

    float bw = rowButtonW(pw);
    for (int i = 0; i < kFaceCount; ++i) {
        glm::vec4 col = (i == gSelectedFace) ? active : base;
        const GuiElement* e = layout.get(ids[i]);
        if (editorButton(win, e ? e->text.c_str() : faceLabel(i),
                         {px + i * (bw + kRowGap), y, bw, kRowH}, col,
                         editorFontSize(e, avatarEditorSmallTabFontSize)).clicked)
            gSelectedFace = i;
    }
    return y + 42.0f;
}

// ── Current face summary (part/side + assigned PNG thumbnail) ───────
float drawFaceSummary(GLFWwindow* win, AvatarSystem& av, const std::string& part,
                      const std::string& face, float px, float y)
{
    FaceSettings fs = av.current().resolve(part, face);

    char summary[128];
    snprintf(summary, sizeof(summary), "%s / %s", partLabel(gSelectedPart), faceLabel(gSelectedFace));
    uiDrawText(summary, uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorSummaryFontSize),
               {0.9f, 0.95f, 1.0f, 1.0f});
    y += 26.0f;

    if (fs.texture.empty()) {
        uiDrawText("No PNG assigned.", uiScaleX(px), uiScaleY(y),
                   avatarEditorFont(avatarEditorHintFontSize), {0.5f, 0.55f, 0.6f, 1.0f});
    } else {
        std::string fullPath = av.avatarPath(av.currentName()) + "/" + fs.texture;
        uiDrawImageFit(fullPath.c_str(), {uiScaleX(px), uiScaleY(y), uiScaleX(48), uiScaleY(48)}, true);
        std::string texLabel = fs.texture;
        if (texLabel.size() > 26)
            texLabel = texLabel.substr(0, 24) + "...";
        uiDrawText(texLabel.c_str(), uiScaleX(px + 56.0f), uiScaleY(y + 6.0f),
                   avatarEditorFont(avatarEditorHintFontSize), {0.6f, 0.8f, 0.6f, 1.0f});
    }
    return y + 56.0f;
}

// ── Assign selected PNG / clear face ─────────────────────────────────
float drawAssignRow(GLFWwindow* win, AvatarSystem& av, const std::string& part,
                    const std::string& face, float px, float y)
{
    const GuiLayout& layout = avatarEditorLayout();
    glm::vec4 assignCol = layoutBg(layout.get("assignBtn"), {0.18f, 0.42f, 0.28f, 1.0f});
    glm::vec4 clearCol = layoutBg(layout.get("clearFaceBtn"), {0.35f, 0.15f, 0.15f, 1.0f});
    const float gap = 8.0f;

    if (editorButton(win, "Use Selected PNG", {px, y, 180.0f, 30.0f}, assignCol,
                     editorFontSize(layout.get("assignBtn"), avatarEditorButtonFontSize)).clicked) {
        if (!gSelectedTexture.empty()) {
            av.setPartFace(part, face, gSelectedTexture);
            avatarEditorRefreshPreview();
            av.triggerSave();
        }
    }
    if (editorButton(win, "Clear Face", {px + 180.0f + gap, y, 110.0f, 30.0f}, clearCol,
                     editorFontSize(layout.get("clearFaceBtn"), avatarEditorButtonFontSize)).clicked) {
        av.setPartFace(part, face, "");
        av.setPartFaceTransform(part, face, FaceTransform{});
        avatarEditorRefreshPreview();
        av.triggerSave();
    }
    return y + 40.0f;
}

// ── Copy / paste face and part ───────────────────────────────────────
float drawCopyPasteRow(GLFWwindow* win, AvatarSystem& av, const std::string& part,
                       const FaceSettings& fs, float px, float y)
{
    const GuiLayout& layout = avatarEditorLayout();
    glm::vec4 cpCol = layoutBg(layout.get("copyFaceBtn"), {0.15f, 0.25f, 0.4f, 1.0f});
    glm::vec4 ptCol = layoutBg(layout.get("copyPartBtn"), {0.12f, 0.18f, 0.3f, 1.0f});

    if (editorButton(win, "Copy Face", {px, y, 90.0f, 26.0f}, cpCol,
                     editorFontSize(layout.get("copyFaceBtn"), avatarEditorSmallButtonFontSize)).clicked) {
        av.clipboardFace = fs;
        av.hasClipboardFace = true;
    }
    if (editorButton(win, "Paste Face", {px + 98.0f, y, 90.0f, 26.0f}, cpCol,
                     editorFontSize(layout.get("pasteFaceBtn"), avatarEditorSmallButtonFontSize)).clicked) {
        if (av.hasClipboardFace) {
            av.setPartFace(part, faceKey(gSelectedFace), av.clipboardFace.texture);
            av.setPartFaceTransform(part, faceKey(gSelectedFace), av.clipboardFace.transform);
            avatarEditorRefreshPreview();
            av.triggerSave();
        }
    }
    if (editorButton(win, "Copy Part", {px + 196.0f, y, 88.0f, 26.0f}, ptCol,
                     editorFontSize(layout.get("copyPartBtn"), avatarEditorSmallButtonFontSize)).clicked) {
        AvatarDefinition def = av.current();
        if (gSelectedPart == 0) av.clipboardPart = def.head;
        else if (gSelectedPart == 1) av.clipboardPart = def.torso;
        else if (gSelectedPart == 2) av.clipboardPart = def.leftArm;
        else if (gSelectedPart == 3) av.clipboardPart = def.rightArm;
        else if (gSelectedPart == 4) av.clipboardPart = def.leftLeg;
        else av.clipboardPart = def.rightLeg;
        av.hasClipboardPart = true;
    }
    if (editorButton(win, "Paste Part", {px + 292.0f, y, 88.0f, 26.0f}, ptCol,
                     editorFontSize(layout.get("pastePartBtn"), avatarEditorSmallButtonFontSize)).clicked) {
        if (av.hasClipboardPart) {
            for (int fi = 0; fi < kFaceCount; ++fi) {
                const FaceSettings& src = av.clipboardPart.byName(faceKey(fi));
                av.setPartFace(part, faceKey(fi), src.texture);
                av.setPartFaceTransform(part, faceKey(fi), src.transform);
            }
            avatarEditorRefreshPreview();
            av.triggerSave();
        }
    }
    return y + 36.0f;
}

// ── Stretch / crop fit mode toggle ──────────────────────────────────
float drawFitModeRow(GLFWwindow* win, AvatarSystem& av, const std::string& part,
                     const std::string& face, const FaceTransform& tf, float px, float y)
{
    const GuiLayout& layout = avatarEditorLayout();
    glm::vec4 toggle = layoutBg(layout.get("stretchBtn"), {0.15f, 0.25f, 0.4f, 1.0f});
    glm::vec4 toggleOn = toggle;
    if (layout.get("stretchBtn") && layout.get("stretchBtn")->hasPressedColor())
        toggleOn = layout.get("stretchBtn")->getPressedColorVec();

    uiDrawText("FIT MODE", uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 22.0f;

    bool isCrop = (tf.stretchMode == 1);
    if (editorButton(win, "Stretch", {px, y, 80.0f, 26.0f}, isCrop ? toggle : toggleOn,
                     editorFontSize(layout.get("stretchBtn"), avatarEditorSmallButtonFontSize)).clicked && isCrop) {
        FaceTransform next = tf;
        next.stretchMode = 0;
        av.setPartFaceTransform(part, face, next);
        avatarEditorRefreshPreview();
        av.triggerSave();
    }
    if (editorButton(win, "Crop", {px + 88.0f, y, 80.0f, 26.0f}, isCrop ? toggleOn : toggle,
                     editorFontSize(layout.get("cropBtn"), avatarEditorSmallButtonFontSize)).clicked && !isCrop) {
        FaceTransform next = tf;
        next.stretchMode = 1;
        av.setPartFaceTransform(part, face, next);
        avatarEditorRefreshPreview();
        av.triggerSave();
    }
    return y + 40.0f;
}

// ── Transform sliders (live) ────────────────────────────────────────
float drawTransformSliders(GLFWwindow* win, AvatarSystem& av, const std::string& part,
                           const std::string& face, const FaceTransform& tf, float px, float y, float pw)
{
    uiDrawText("FACE STYLE", uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 24.0f;

    FaceTransform next = tf;
    bool changed = false;
    const float sliderW = pw - 8.0f;

    changed |= drawEditorSlider(win, "Offset X", px, y, sliderW, next.offsetX, -500.0f, 500.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Offset Y", px, y, sliderW, next.offsetY, -500.0f, 500.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale X", px, y, sliderW, next.scaleX, 0.05f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Scale Y", px, y, sliderW, next.scaleY, 0.05f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Rotation", px, y, sliderW, next.rotation, 0.0f, 360.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Red tint", px, y, sliderW, next.color.r, 0.0f, 2.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Green tint", px, y, sliderW, next.color.g, 0.0f, 2.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Blue tint", px, y, sliderW, next.color.b, 0.0f, 2.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Transparency", px, y, sliderW, next.transparency, 0.0f, 1.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Saturation", px, y, sliderW, next.saturation, -10.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Brightness", px, y, sliderW, next.brightness, -10.0f, 10.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Hue shift", px, y, sliderW, next.hueShift, -180.0f, 180.0f); y += 28.0f;

    if (changed) {
        av.setPartFaceTransform(part, face, next);
        avatarEditorRefreshPreview();
        av.triggerSave();
    }
    return y + 8.0f;
}

// ── Bulk apply helpers ──────────────────────────────────────────────
float drawBulkApplyRow(GLFWwindow* win, AvatarSystem& av, const std::string& part,
                       const FaceSettings& fs, float px, float y)
{
    const GuiLayout& layout = avatarEditorLayout();
    glm::vec4 bulkCol = layoutBg(layout.get("applyPartBtn"), {0.2f, 0.45f, 0.3f, 1.0f});
    const GuiElement* partBtn = layout.get("applyPartBtn");
    const GuiElement* allBtn = layout.get("applyAllBtn");
    const char* partLabelText = partBtn && !partBtn->text.empty() ? partBtn->text.c_str() : "Apply to all sides of this part";
    const char* allLabelText = allBtn && !allBtn->text.empty() ? allBtn->text.c_str() : "Apply to every face";
    float bulkFont = editorFontSize(partBtn, avatarEditorBulkButtonFontSize);

    if (editorButton(win, partLabelText, {px, y, 288.0f, 30.0f}, bulkCol, bulkFont).clicked) {
        for (int fi = 0; fi < kFaceCount; ++fi) {
            av.setPartFace(part, faceKey(fi), fs.texture);
            av.setPartFaceTransform(part, faceKey(fi), fs.transform);
        }
        avatarEditorRefreshPreview();
        av.triggerSave();
    }
    if (editorButton(win, allLabelText, {px + 296.0f, y, 288.0f, 30.0f}, bulkCol, bulkFont).clicked) {
        for (int pi = 0; pi < kPartCount; ++pi)
            for (int fi = 0; fi < kFaceCount; ++fi) {
                av.setPartFace(partKey(pi), faceKey(fi), fs.texture);
                av.setPartFaceTransform(partKey(pi), faceKey(fi), fs.transform);
            }
        avatarEditorRefreshPreview();
        av.triggerSave();
    }
    return y + 40.0f;
}

float bodyValue(const nlohmann::json& overrides, const std::string& part,
                const char* key, int index, float fallback)
{
    if (!overrides.is_object() || !overrides.contains(part))
        return fallback;
    const auto& value = overrides[part][key];
    if (!value.is_array() || value.size() <= index)
        return fallback;
    return value[index].get<float>();
}

float drawBodyTransformSection(GLFWwindow* win, AvatarSystem& av,
                               const std::string& part, float px, float y, float pw)
{
    uiDrawText("BODY PART TRANSFORM", uiScaleX(px), uiScaleY(y),
               avatarEditorFont(avatarEditorSectionFontSize),
               {0.4f, 0.6f, 0.5f, 1.0f});
    y += 24.0f;

    const auto& overrides = av.current().bodypartOverrides;
    glm::vec3 offset{
        bodyValue(overrides, part, "offset", 0, 0.0f),
        bodyValue(overrides, part, "offset", 1, 0.0f),
        bodyValue(overrides, part, "offset", 2, 0.0f)};
    glm::vec3 rotation{
        bodyValue(overrides, part, "rotation", 0, 0.0f),
        bodyValue(overrides, part, "rotation", 1, 0.0f),
        bodyValue(overrides, part, "rotation", 2, 0.0f)};
    glm::vec3 scale{
        bodyValue(overrides, part, "scale", 0, 1.0f),
        bodyValue(overrides, part, "scale", 1, 1.0f),
        bodyValue(overrides, part, "scale", 2, 1.0f)};

    const float sliderW = pw - 8.0f;
    bool changed = false;
    changed |= drawEditorSlider(win, "Body X", px, y, sliderW, offset.x, -5.0f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body Y", px, y, sliderW, offset.y, -5.0f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body Z", px, y, sliderW, offset.z, -5.0f, 5.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body pitch", px, y, sliderW, rotation.x, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body yaw", px, y, sliderW, rotation.y, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body roll", px, y, sliderW, rotation.z, -180.0f, 180.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body scale X", px, y, sliderW, scale.x, -3.0f, 3.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body scale Y", px, y, sliderW, scale.y, -3.0f, 3.0f); y += 28.0f;
    changed |= drawEditorSlider(win, "Body scale Z", px, y, sliderW, scale.z, -3.0f, 3.0f); y += 34.0f;

    if (changed) {
        Player* preview = avatarEditorPreviewPlayer();
        if (preview)
            av.setBodypartOverride(*preview, part, offset, rotation, scale);
        av.triggerSave();
    }
    return y;
}

} // anonymous namespace

void drawAvatarFacesTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;

    const std::string part = partKey(gSelectedPart);
    const std::string face = faceKey(gSelectedFace);
    const FaceSettings fs = av.current().resolve(part, face);

    const float contentH = 1250.0f;
    ScrollState ss;
    beginScroll(win, {px, py, pw, ph}, contentH, ss);

    float y = py;
    y = drawPartRow(win, px, y, pw);
    y = drawFaceRow(win, px, y, pw);
    y = drawFaceSummary(win, av, part, face, px, y);
    y = drawAssignRow(win, av, part, face, px, y);
    y = drawCopyPasteRow(win, av, part, fs, px, y);
    y = drawFitModeRow(win, av, part, face, fs.transform, px, y);
    y = drawTransformSliders(win, av, part, face, fs.transform, px, y, pw);
    y = drawBulkApplyRow(win, av, part, fs, px, y);

    uiDrawText("Hint: pick a PNG in the left panel, then hit \"Use Selected PNG\".",
               uiScaleX(px), uiScaleY(y), avatarEditorFont(avatarEditorHintFontSize),
               {0.4f, 0.5f, 0.6f, 1.0f});
    y += 28.0f;
    y = drawBodyTransformSection(win, av, part, px, y, pw);

    endScroll({px, py, pw, ph}, contentH, ss);
}
