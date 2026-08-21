// 08 15 2026, 15 30
/* purpose
* Implements shared editor state, slider widget and layout helpers.
* Implements preview refresh, outfit load, and cosmetics apply.
* DOES NOT draw any editor panel or own avatar data.
*/
#include "avatar-editor-helpers.h"
#include "avatar.h"
#include "cosmetic-system.h"
#include "entities/player.h"
#include "config/player-settings.h"
#include "gui/menus/menu-avatar-preview.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

// ── Shared editor state ─────────────────────────────────────────────
int gEditorTab = 0;
int gSelectedPart = 0;
int gSelectedFace = 0;
std::string gSelectedTexture;
bool gSelectedFaces[kPartCount][kFaceCount] = {};

void ensureAvatarEditorSelection()
{
    for (int pi = 0; pi < kPartCount; ++pi)
        for (int fi = 0; fi < kFaceCount; ++fi)
            if (gSelectedFaces[pi][fi])
                return;
    gSelectedFaces[0][0] = true;
}

void setSelectedPartFaces(int part, bool selected)
{
    if (part < 0 || part >= kPartCount) return;
    for (int fi = 0; fi < kFaceCount; ++fi)
        gSelectedFaces[part][fi] = selected;
    gSelectedPart = part;
}

void setAllSelectedFaces(bool selected)
{
    for (int pi = 0; pi < kPartCount; ++pi)
        for (int fi = 0; fi < kFaceCount; ++fi)
            gSelectedFaces[pi][fi] = selected;
}

bool anySelectedFace()
{
    for (int pi = 0; pi < kPartCount; ++pi)
        for (int fi = 0; fi < kFaceCount; ++fi)
            if (gSelectedFaces[pi][fi]) return true;
    return false;
}

int selectedFaceCount()
{
    int count = 0;
    for (int pi = 0; pi < kPartCount; ++pi)
        for (int fi = 0; fi < kFaceCount; ++fi)
            if (gSelectedFaces[pi][fi]) ++count;
    return count;
}

const char* partLabel(int idx)
{
    static const char* labels[] = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"};
    return labels[(idx < 0 || idx >= kPartCount) ? 0 : idx];
}

const char* partKey(int idx)
{
    static const char* keys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    return keys[(idx < 0 || idx >= kPartCount) ? 0 : idx];
}

const char* faceLabel(int idx)
{
    static const char* labels[] = {"Front", "Back", "Left", "Right", "Top", "Bottom"};
    return labels[(idx < 0 || idx >= kFaceCount) ? 0 : idx];
}

const char* faceKey(int idx)
{
    static const char* keys[] = {"front", "back", "left", "right", "top", "bottom"};
    return keys[(idx < 0 || idx >= kFaceCount) ? 0 : idx];
}

// ── Layout helpers ──────────────────────────────────────────────────
GuiLayout& avatarEditorLayout()
{
    return GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
}

float layoutVal(const GuiElement* e, float def)
{
    return e ? e->x : def;
}

glm::vec4 layoutBg(const GuiElement* e, glm::vec4 def)
{
    return e ? e->getBackgroundColorVec() : def;
}

std::string editorLabelText(const char* id, const char* fallback)
{
    const GuiElement* element = avatarEditorLayout().get(id);
    if (element && !element->text.empty())
        return element->text;
    return fallback ? fallback : "";
}

float editorLabelFontSize(const char* id, float fallback)
{
    return editorFontSize(avatarEditorLayout().get(id), fallback);
}

float editorStyleFontSize(const char* styleId, float fallback)
{
    const GuiElement* element = avatarEditorLayout().get(styleId);
    if (element && element->fontSize > 0.0f)
        return element->fontSize;
    return fallback;
}

// ── Editor font helpers ─────────────────────────────────────────────
float avatarEditorFontScale()
{
    const GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    return std::min(1.0f, std::min(cs.scaleX(), cs.scaleY()));
}

float avatarEditorFont(float baseSize)
{
    return baseSize * avatarEditorFontScale();
}

float editorFontSize(const GuiElement* e, float defBase)
{
    const float base = (e && e->fontSize > 0.0f) ? e->fontSize : defBase;
    return avatarEditorFont(base);
}

UIButtonState editorButton(GLFWwindow* win, const char* text, UIRect r,
                           glm::vec4 color, float fontSize)
{
    return uiButton(win, text, r, color, nullptr, nullptr, nullptr, nullptr, nullptr, fontSize);
}

// ── Slider ──────────────────────────────────────────────────────────
bool drawEditorSlider(GLFWwindow* win, const char* label, float x, float y,
                      float w, float& value, float minVal, float maxVal)
{
    const float labelW = 130.0f;
    const float valueW = 60.0f;
    const float trackX = x + labelW;
    const float trackW = w - labelW - valueW;
    const float trackH = 22.0f;

    std::string labelId = "slider";
    for (const char* p = label; *p; ++p)
        if (*p != ' ') labelId += *p;
    const GuiElement* labelElement = avatarEditorLayout().get(labelId);
    const std::string displayLabel = editorLabelText(labelId.c_str(), label);
    uiDrawText(displayLabel.c_str(), uiScaleX(x), uiScaleY(y + 3.0f),
               editorFontSize(labelElement, avatarEditorSliderLabelFontSize),
               {0.7f, 0.8f, 0.9f, 1.0f});

    UIRect s = GuiCoordinateSystem::instance().designToScreen({trackX, y, trackW, trackH});

    uiDrawRect(s, {0.12f, 0.13f, 0.18f, 1.0f}, "slider-track");
    float t = (maxVal > minVal) ? (value - minVal) / (maxVal - minVal) : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    uiDrawRect({s.x, s.y, s.w * t, s.h}, {0.25f, 0.55f, 0.35f, 1.0f}, "slider-fill");
    uiDrawRectOutline(s, {0.4f, 0.45f, 0.55f, 0.7f}, "slider-border");
    uiDrawRect({s.x + s.w * t - 3.0f, s.y - 2.0f, 6.0f, s.h + 4.0f},
               {0.7f, 0.9f, 0.8f, 1.0f}, "slider-handle");

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", value);
    uiDrawText(buf, uiScaleX(trackX + trackW + 6.0f), uiScaleY(y + 3.0f),
               avatarEditorFont(avatarEditorSliderValueFontSize), {1, 1, 1, 1});

    if (maxVal <= minVal)
        return false;

    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    double fbx = mx, fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);

    if (pointIn(fbx, fby, s) && UISys::gMouseDown) {
        float nt = std::clamp((float)((fbx - s.x) / s.w), 0.0f, 1.0f);
        float nv = minVal + nt * (maxVal - minVal);
        if (std::abs(nv - value) > 0.0001f) {
            value = nv;
            return true;
        }
    }
    return false;
}

// ── Live preview ────────────────────────────────────────────────────
// The editor's live preview uses the MenuAvatarPreview player so the
// character is always centered and animated, independent of gameplay state.
Player* avatarEditorPreviewPlayer()
{
    return MenuAvatarPreview::instance().ensurePlayer();
}

void avatarEditorRefreshPreview()
{
    Player* p = avatarEditorPreviewPlayer();
    if (p && AvatarSystem::instance().hasAvatar())
        AvatarSystem::instance().requestAtlasBuild(*p);
}

void avatarEditorApplyCosmeticsToPlayer()
{
    Player* p = avatarEditorPreviewPlayer();
    AvatarSystem& av = AvatarSystem::instance();
    if (!p || !av.hasAvatar())
        return;
    CosmeticSystem::instance().loadCosmetics(av.current().cosmetics);
    p->setCosmetics(av.current().cosmetics);
}

void avatarEditorLoadOutfit(const std::string& name)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.loadAvatar(name))
        return;
    Player* p = avatarEditorPreviewPlayer();
    if (p)
        av.applyToPlayer(*p, true);
    avatarEditorApplyCosmeticsToPlayer();
    GetPlayerSettings().avatarName = name;
    SavePlayerSettings();
}
