#include "avatar-menu.h"
#include "avatar.h"
#include "avatar-editor-scroll.h"
#include "avatar-editor-dropdown.h"
#include "avatar-drop-target.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>

#include "../gui/ui-system.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-element-render.h"
#include "../gui/gui-coord.h"
#include "../gui/gui-media.h"
#include "../devtools/terminal.h"
#include "../config/player-settings.h"

class Player;
extern Player* gpPlayer;

static void liveApply()
{
    if (gpPlayer && AvatarSystem::instance().hasAvatar())
        AvatarSystem::instance().applyToPlayer(*gpPlayer);
}

namespace {

// ── State ───────────────────────────────────────────────────────────
std::string gSelectedTexture;
GLuint gSelectedTextureGL = 0;
int gSelectedTexW = 0, gSelectedTexH = 0;
std::unordered_set<int> gCheckedFaces;
int gEditorTab = 0;
int gCopyPartIdx = -1;
int gCopyFaceIdx = -1;
int gColorPickerPart = -1;
float gColorPickerHue = 0.0f;
DropdownState gPartDropdown;

// ── String tables ───────────────────────────────────────────────────
const char* kPartLabels[] = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"};
const char* kPartKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
const char* kFaceLabels[] = {"Front", "Back", "Left", "Right", "Top", "Bottom"};
const char* kFaceKeys[] = {"front", "back", "left", "right", "top", "bottom"};
const char* kTabLabels[] = {"Faces", "Colors", "Cosmetics", "Presets"};
const int kPartCount = 6;
const int kFaceCount = 6;

// ── Layout helper ────────────────────────────────────────────────────
static GuiLayout& getLayout()
{
    return GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
}

static float lx(const std::string& id, float def = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->x : def;
}
static float ly(const std::string& id, float def = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->y : def;
}
static float lw(const std::string& id, float def = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->w : def;
}
static float lh(const std::string& id, float def = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->h : def;
}

int faceSlotIndex(int part, int face) { return part * kFaceCount + face; }

void clearTexturePreview()
{
    if (gSelectedTextureGL) { glDeleteTextures(1, &gSelectedTextureGL); gSelectedTextureGL = 0; }
    gSelectedTexture.clear();
    gSelectedTexW = gSelectedTexH = 0;
}

void loadTexturePreview(const std::string& avatarName, const std::string& filename)
{
    clearTexturePreview();
    gSelectedTexture = filename;
    std::string fullPath = AvatarSystem::instance().avatarPath(avatarName) + "/" + filename;
    gSelectedTextureGL = loadMediaTexture(fullPath.c_str(), &gSelectedTexW, &gSelectedTexH);
}

void checkAllFaces() { for (int i = 0; i < kPartCount * kFaceCount; ++i) gCheckedFaces.insert(i); }
void uncheckAllFaces() { gCheckedFaces.clear(); }
void toggleFaceCheck(int idx)
{
    if (gCheckedFaces.count(idx)) gCheckedFaces.erase(idx);
    else gCheckedFaces.insert(idx);
}

// ── Slider helper ───────────────────────────────────────────────────
struct SliderRange { float min, max, step; };

static float drawSlider(GLFWwindow* win, float x, float y, float w, float val,
                         const SliderRange& range, const char* label)
{
    float sx = uiScaleX(x), sy = uiScaleY(y), sw = uiScaleX(w), sh = uiScaleY(24.0f);
    if (label)
        uiDrawText(label, sx - uiScaleX(60.0f), sy, 0.60f, {0.7f, 0.8f, 0.9f, 1.0f});

    UIRect track = {sx, sy + sh * 0.3f, sw, sh * 0.4f};
    uiDrawRect(track, {0.12f, 0.13f, 0.18f, 1.0f}, "slider-track");

    float t = (val - range.min) / (range.max - range.min);
    UIRect fill = {sx, sy + sh * 0.3f, sw * std::max(0.001f, std::min(1.0f, t)), sh * 0.4f};
    uiDrawRect(fill, {0.25f, 0.55f, 0.35f, 1.0f}, "slider-fill");

    float hx = sx + sw * t - 4.0f;
    UIRect handle = {hx, sy, 8.0f, sh};
    uiDrawRect(handle, {0.6f, 0.85f, 0.7f, 1.0f}, "slider-handle");

    UIRect clickArea = {x, y, w, 24.0f};
    auto btn = uiButton(win, "", clickArea, {0,0,0,0});
    if (btn.clicked) {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);
        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        float scaleX = (float)fbW / 1920.0f;
        float designX = (float)mx / scaleX;
        float t = (designX - x) / w;
        float newVal = range.min + t * (range.max - range.min);
        newVal = std::max(range.min, std::min(range.max, newVal));
        if (range.step > 0)
            newVal = std::round(newVal / range.step) * range.step;
        return newVal;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", val);
    uiDrawText(buf, sx + sw + 4.0f, sy, 0.60f, {1.0f, 1.0f, 1.0f, 1.0f});
    return val;
}

// ── Draw LEFT panel: PNG Library ────────────────────────────────────
static void drawLibraryPanel(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;

    std::string avatarName = av.currentName();
    auto pngs = av.listPngs(avatarName);

    if (pngs.empty())
    {
        uiDrawText("No PNGs found.", uiScaleX(px), uiScaleY(py + 20.0f), 0.68f, {0.5f, 0.6f, 0.7f, 1.0f});
        return;
    }

    // Read layout config for thumbnail grid
    const GuiElement* colsEl = getLayout().get("thumbCols");
    const GuiElement* gapEl = getLayout().get("thumbGap");
    const GuiElement* labelHEl = getLayout().get("thumbLabelH");
    const GuiElement* padEl = getLayout().get("thumbPadding");
    const GuiElement* bgNEl = getLayout().get("thumbBgNormal");
    const GuiElement* bgSEl = getLayout().get("thumbBgSelected");
    const GuiElement* outEl = getLayout().get("thumbOutlineSelected");

    int cols = colsEl ? (int)colsEl->x : 3;
    float gap = gapEl ? gapEl->x : 6.0f;
    float labelH = labelHEl ? labelHEl->x : 22.0f;
    float padding = padEl ? padEl->x : 4.0f;
    glm::vec4 bgNormal = bgNEl ? bgNEl->getBackgroundColorVec() : glm::vec4{0.07f, 0.08f, 0.12f, 1.0f};
    glm::vec4 bgSelected = bgSEl ? bgSEl->getBackgroundColorVec() : glm::vec4{0.2f, 0.5f, 0.3f, 1.0f};
    glm::vec4 outlineSel = outEl ? outEl->getTextColorVec() : glm::vec4{0.3f, 0.8f, 0.5f, 1.0f};

    float thumbSize = (pw - padding * 2.0f - (cols - 1) * gap) / cols;
    float itemH = thumbSize + labelH;
    float contentH = ((int)pngs.size() + cols - 1) / cols * itemH;

    UIRect scrollArea = {px, py, pw, ph};
    ScrollState ss;
    beginScroll(win, scrollArea, contentH, ss);

    float tx = px + padding;

    // Hover animation state - stored per-frame using a static map
    struct HoverAnim {
        float progress = 0.0f;
    };
    static std::unordered_map<std::string, HoverAnim> gHoverAnims;

    for (int i = 0; i < (int)pngs.size(); ++i)
    {
        int col = i % cols;
        int row = i / cols;
        float ix = tx + col * (thumbSize + gap);
        float iy = py + row * itemH;

        std::string fullPath = av.avatarPath(avatarName) + "/" + pngs[i];
        UIRect thumbRect = {ix, iy, thumbSize, thumbSize};
        float screenX = uiScaleX(ix);
        float screenY = uiScaleY(iy);
        float screenSize = uiScaleX(thumbSize);
        bool isSelected = (pngs[i] == gSelectedTexture);

        // Button for interaction
        auto btn = uiButton(win, "", thumbRect, bgNormal);

        // Hover animation smoothing
        HoverAnim& anim = gHoverAnims[pngs[i]];
        float target = btn.hovered ? 1.0f : 0.0f;
        anim.progress += (target - anim.progress) * 0.15f;

        // Hover scale: slightly enlarge on hover (up to 5%)
        float hoverScale = 1.0f + anim.progress * 0.05f;
        float hoverExpand = (hoverScale - 1.0f) * thumbSize * 0.5f;

        // Background rect
        UIRect bgScreen = {screenX - hoverExpand, screenY - hoverExpand,
                           screenSize * hoverScale, screenSize * hoverScale};

        glm::vec4 bgCol = isSelected ? bgSelected : bgNormal;
        if (btn.hovered && !isSelected)
            bgCol += glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);

        uiDrawRect(bgScreen, bgCol, "thumb-bg");

        // Draw the actual image with aspect-ratio fit and checkerboard
        // Must use screen coordinates (same as bgScreen) since the renderer draws at raw pixel coords
        float hoverScreenExpand = (hoverScale - 1.0f) * screenSize * 0.5f;
        UIRect fitScreen = {screenX - hoverScreenExpand, screenY - hoverScreenExpand,
                            screenSize * hoverScale, screenSize * hoverScale};
        uiDrawImageFit(fullPath.c_str(), fitScreen, true);

        // Selected outline (brighter + thicker)
        if (isSelected) {
            float outlineThick = 3.0f;
            glm::vec4 selCol = outlineSel + glm::vec4(0.2f, 0.0f, 0.0f, 0.0f);
            UIRect os = {bgScreen.x - outlineThick, bgScreen.y - outlineThick,
                         bgScreen.w + outlineThick * 2, bgScreen.h + outlineThick * 2};
            uiDrawRectOutline(os, selCol, "thumb-sel");
            uiDrawRectOutline(bgScreen, outlineSel, "thumb-sel-inner");
        }

        // Hover highlight border
        if (btn.hovered && !isSelected) {
            glm::vec4 hovCol = {0.5f, 0.7f, 1.0f, 0.4f + anim.progress * 0.3f};
            uiDrawRectOutline(bgScreen, hovCol, "thumb-hover");
        }

        if (btn.clicked)
            loadTexturePreview(avatarName, pngs[i]);

        // Label
        std::string label = pngs[i];
        if (label.size() > 12) label = label.substr(0, 10) + "...";
        uiDrawText(label.c_str(), uiScaleX(ix), uiScaleY(iy + thumbSize + 4.0f), 0.48f, {0.6f, 0.7f, 0.8f, 1.0f});
    }

    // Cleanup stale hover anims (keep map small)
    if (gHoverAnims.size() > pngs.size() * 2) {
        for (auto it = gHoverAnims.begin(); it != gHoverAnims.end(); ) {
            bool found = false;
            for (auto& p : pngs) {
                if (it->first == p) { found = true; break; }
            }
            if (!found && it->second.progress < 0.01f)
                it = gHoverAnims.erase(it);
            else
                ++it;
        }
    }

    endScroll(scrollArea, contentH, ss);
}

// ── Draw CENTER panel: Editor tabs + content ────────────────────────
static void drawEditorPanel(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
    if (!av.hasAvatar()) return;

    auto readVal = [&](const char* id, float def) -> float {
        const GuiElement* e = layout.get(id);
        return e ? e->x : def;
    };
    auto readBgCol = [&](const char* id, glm::vec4 def) -> glm::vec4 {
        const GuiElement* e = layout.get(id);
        return e ? e->getBackgroundColorVec() : def;
    };

    float cy = py;
    float remainingH = py + ph - cy - 50.0f;

    // Scrollable content area for the currently selected tab
    UIRect contentArea = {px + 4.0f, cy, pw - 8.0f, remainingH};
    ScrollState cs;

    switch (gEditorTab)
    {
    case 0: // Faces tab
    {
        float contentH = 500.0f;
        beginScroll(win, contentArea, contentH, cs);

        if (gSelectedTexture.empty())
        {
            const GuiElement* h1 = layout.get("facesTabHint1");
            const GuiElement* h2 = layout.get("facesTabHint2");
            if (h1 && h1->visible) drawGuiElement(win, *h1);
            if (h2 && h2->visible) { cy = h2->y + h2->h + 4.0f; drawGuiElement(win, *h2); }
            else { cy += 52.0f; }
        }
        else
        {
            // Texture preview
            float previewSize = readVal("facesTabPreviewSize", 80.0f);
            std::string texPath = av.avatarPath(av.currentName()) + "/" + gSelectedTexture;
            uiDrawImage(texPath.c_str(), {px + 4.0f, cy, previewSize, previewSize});
            uiDrawText(gSelectedTexture.c_str(), uiScaleX(px + previewSize + 10.0f), uiScaleY(cy + 6.0f),
                        0.60f, {0.6f, 0.9f, 0.6f, 1.0f});
            if (uiButton(win, "X", {px + previewSize - 16.0f, cy - 4.0f, 20.0f, 20.0f},
                         {0.5f, 0.15f, 0.15f, 1.0f}).clicked) {
                clearTexturePreview();
                uncheckAllFaces();
            }
            cy += previewSize + 10.0f;

            // Quick assign buttons
            float qaH = readVal("facesTabQaHeight", 30.0f);
            float btnW = (pw - 24.0f) / 5.0f;
            auto qBtn = [&](const char* label, float x, const std::string& partKey, const char* btnId) {
                glm::vec4 col = readBgCol(btnId, {0.15f, 0.3f, 0.5f, 1.0f});
                if (uiButton(win, label, {x, cy, btnW, qaH}, col).clicked) {
                    for (int fi = 0; fi < kFaceCount; ++fi)
                        av.setPartFace(partKey, kFaceKeys[fi], gSelectedTexture);
                    checkAllFaces();
                    liveApply();
                }
            };
            qBtn("Head", px + 4.0f, "head", "facesTabHeadBtn");
            qBtn("Torso", px + 4.0f + btnW + 4.0f, "torso", "facesTabTorsoBtn");
            qBtn("Arms", px + 4.0f + 2 * (btnW + 4.0f), "leftArm", "facesTabArmsBtn");
            qBtn("Legs", px + 4.0f + 3 * (btnW + 4.0f), "leftLeg", "facesTabLegsBtn");
            glm::vec4 allCol = readBgCol("facesTabAllBtn", {0.2f, 0.4f, 0.25f, 1.0f});
            if (uiButton(win, "All", {px + 4.0f + 4 * (btnW + 4.0f), cy, btnW, qaH}, allCol).clicked) {
                for (int pi = 0; pi < kPartCount; ++pi)
                    for (int fi = 0; fi < kFaceCount; ++fi)
                        av.setPartFace(kPartKeys[pi], kFaceKeys[fi], gSelectedTexture);
                checkAllFaces();
                liveApply();
            }
            cy += qaH + 8.0f;

            // Select all / Clear
            glm::vec4 selCol = readBgCol("facesTabSelectAllBtn", {0.15f, 0.25f, 0.4f, 1.0f});
            glm::vec4 clrCol = readBgCol("facesTabClearBtn", {0.35f, 0.15f, 0.15f, 1.0f});
            if (uiButton(win, "Select All", {px + 4.0f, cy, 100.0f, 26.0f}, selCol).clicked)
                checkAllFaces();
            if (uiButton(win, "Clear", {px + 112.0f, cy, 80.0f, 26.0f}, clrCol).clicked)
                uncheckAllFaces();
            cy += 32.0f;

            // Face grid
            contentH = cy - contentArea.y + kPartCount * 100.0f;
            int facesPerRow = (int)readVal("facesTabFacesPerRow", 3.0f);
            float faceH = readVal("facesTabFaceRowHeight", 22.0f);
            glm::vec4 chkOn = readBgCol("facesTabCheckColorOn", {0.2f, 0.7f, 0.3f, 1.0f});
            glm::vec4 chkOff = readBgCol("facesTabCheckColorOff", {0.12f, 0.12f, 0.16f, 1.0f});

            for (int pi = 0; pi < kPartCount; ++pi)
            {
                uiDrawText(kPartLabels[pi], uiScaleX(px + 4.0f), uiScaleY(cy), 0.64f, {0.70f, 0.80f, 0.90f, 1.0f});
                cy += 24.0f;
                float colW = (pw - 12.0f) / 2.0f;

                for (int fi = 0; fi < kFaceCount; ++fi)
                {
                    int col = fi / facesPerRow;
                    int row = fi % facesPerRow;
                    float fx = px + 4.0f + col * colW;
                    float fy = cy + row * (faceH + 2.0f);
                    int idx = faceSlotIndex(pi, fi);
                    bool checked = gCheckedFaces.count(idx) > 0;

                    UIRect chkScreen = {uiScaleX(fx), uiScaleY(fy), uiScaleY(faceH), uiScaleY(faceH)};
                    glm::vec4 cbCol = checked ? chkOn : chkOff;
                    uiDrawRect(chkScreen, cbCol, "face-chk");
                    if (checked)
                        uiDrawText("\u2713", chkScreen.x + 3.0f, chkScreen.y + 2.0f, 0.52f, {1.0f, 1.0f, 1.0f, 1.0f});

                    if (uiButton(win, "", {fx, fy, faceH, faceH}, cbCol).clicked)
                        toggleFaceCheck(idx);

                    uiDrawText(kFaceLabels[fi], uiScaleX(fx + faceH + 4.0f), uiScaleY(fy + 2.0f),
                               0.52f, {0.7f, 0.75f, 0.85f, 1.0f});
                }
                cy += facesPerRow * (faceH + 2.0f) + 6.0f;
            }
        }

        // Apply to checked faces button
        float applyY = cy + 10.0f;
        if (!gCheckedFaces.empty())
        {
            glm::vec4 applyCol = readBgCol("facesTabApplyBtn", {0.2f, 0.55f, 0.3f, 1.0f});
            if (uiButton(win, "APPLY TO CHECKED FACES", {px + 4.0f, applyY, pw - 8.0f, 34.0f},
                         applyCol).clicked) {
                for (int idx : gCheckedFaces) {
                    int pi = idx / kFaceCount;
                    int fi = idx % kFaceCount;
                    av.setPartFace(kPartKeys[pi], kFaceKeys[fi], gSelectedTexture);
                }
                Terminal::instance().addLog("[AVATAR] Applied to " + std::to_string(gCheckedFaces.size()) + " faces");
                liveApply();
            }
        }

        endScroll(contentArea, contentH, cs);
        break;
    }

    case 1: // Colors tab
    {
        float contentH = kPartCount * 100.0f;
        beginScroll(win, contentArea, contentH, cs);

        float swatchSize = readVal("colorsTabSwatchSize", 24.0f);
        float partSpacing = readVal("colorsTabPartSpacing", 90.0f);
        float sliderOffX = readVal("colorsTabSliderOffsetX", 8.0f);
        float sliderWOff = readVal("colorsTabSliderWidthOff", 60.0f);

        for (int pi = 0; pi < kPartCount; ++pi)
        {
            glm::vec3 color = av.partColor(kPartKeys[pi]);
            UIRect swatchScreen = {uiScaleX(px + 4.0f), uiScaleY(cy), uiScaleX(swatchSize), uiScaleY(swatchSize)};
            uiDrawRect(swatchScreen, {color.r, color.g, color.b, 1.0f}, "part-swatch");
            uiDrawText(kPartLabels[pi], uiScaleX(px + 4.0f + swatchSize + 6.0f), uiScaleY(cy),
                       0.68f, {0.8f, 0.85f, 0.95f, 1.0f});

            float sliderX = px + sliderOffX;
            float sliderY = cy + 28.0f;
            float sliderW = pw - sliderWOff;

            float newR = drawSlider(win, sliderX, sliderY, sliderW, color.r, {0, 1, 0.01f}, "R");
            sliderY += 20.0f;
            float newG = drawSlider(win, sliderX, sliderY, sliderW, color.g, {0, 1, 0.01f}, "G");
            sliderY += 20.0f;
            float newB = drawSlider(win, sliderX, sliderY, sliderW, color.b, {0, 1, 0.01f}, "B");

            if (newR != color.r || newG != color.g || newB != color.b) {
                av.setPartColor(kPartKeys[pi], glm::vec3(newR, newG, newB));
                liveApply();
            }
            cy += partSpacing;
        }

        endScroll(contentArea, contentH, cs);
        break;
    }

    case 2: // Cosmetics tab
    {
        uiDrawText("Cosmetics", uiScaleX(px + 4.0f), uiScaleY(cy), 0.72f, {0.8f, 0.9f, 1.0f, 1.0f});
        cy += 30.0f;

        std::vector<std::string> cosmeticItems;
        std::string cosDir = "assets/objects/things/cosmetics";
        if (std::filesystem::exists(cosDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(cosDir))
            {
                if (entry.path().extension() == ".glb")
                    cosmeticItems.push_back(entry.path().filename().string());
            }
            std::sort(cosmeticItems.begin(), cosmeticItems.end());
        }

        const char* kCosmeticSlots[] = {"head", "torso", "arms", "legs"};
        const char* kCosmeticSlotLabels[] = {"Headwear", "Body", "Arms", "Legs"};
        float ddItemH = readVal("cosmeticsTabDropdownItemH", 28.0f);
        float slotSpacing = readVal("cosmeticsTabSlotSpacing", 40.0f);

        for (int si = 0; si < 4; ++si)
        {
            std::string currentChoice = "none";
            for (auto& c : av.current().cosmetics)
            {
                if (c.slot == kCosmeticSlots[si]) { currentChoice = c.choice; break; }
            }

            static DropdownState slotStates[4];
            uiDrawText(kCosmeticSlotLabels[si], uiScaleX(px + 4.0f), uiScaleY(cy), 0.64f, {0.7f, 0.8f, 0.9f, 1.0f});
            cy += 24.0f;

            int sel = drawDropdown(win, slotStates[si], px + 8.0f, cy, pw - 16.0f, ddItemH,
                                    nullptr, cosmeticItems.empty() ? std::vector<std::string>{"Nothing found."} : cosmeticItems);

            if (sel >= 0 && sel < (int)cosmeticItems.size())
            {
                auto& cosmetics = const_cast<std::vector<CosmeticSlot>&>(av.current().cosmetics);
                bool found = false;
                for (auto& c : cosmetics) {
                    if (c.slot == kCosmeticSlots[si]) { c.choice = cosmeticItems[sel]; found = true; break; }
                }
                if (!found)
                    cosmetics.push_back({kCosmeticSlots[si], cosmeticItems[sel]});
                liveApply();
            }
            cy += slotSpacing;
        }
        break;
    }

    case 3: // Presets tab
    {
        uiDrawText("Presets", uiScaleX(px + 4.0f), uiScaleY(cy), 0.72f, {0.8f, 0.9f, 1.0f, 1.0f});
        cy += 30.0f;

        static char presetNameBuf[64] = "";
        static bool presetInputActive = false;

        uiDrawText("Save as:", uiScaleX(px + 4.0f), uiScaleY(cy), 0.60f, {0.5f, 0.6f, 0.7f, 1.0f});
        UIRect inputRect = {uiScaleX(px + 80.0f), uiScaleY(cy), uiScaleX(180.0f), uiScaleY(28.0f)};
        uiDrawRect(inputRect, {0.08f, 0.09f, 0.13f, 1.0f}, "preset-input");
        uiDrawText(presetNameBuf[0] ? presetNameBuf : "name...", inputRect.x + 6.0f, inputRect.y + 4.0f, 0.56f,
                   presetNameBuf[0] ? glm::vec4{1,1,1,1} : glm::vec4{0.4f, 0.4f, 0.5f, 1.0f});
        if (uiButton(win, "", {px + 80.0f, cy, 180.0f, 28.0f}, {0,0,0,0}).clicked)
            presetInputActive = true;

        glm::vec4 saveCol = readBgCol("presetsTabSaveBtn", {0.2f, 0.5f, 0.3f, 1.0f});
        if (uiButton(win, "SAVE", {px + 270.0f, cy, 80.0f, 28.0f}, saveCol).clicked)
        {
            if (presetNameBuf[0]) {
                av.savePreset(presetNameBuf);
                memset(presetNameBuf, 0, sizeof(presetNameBuf));
            }
        }
        cy += 34.0f;

        float entryH = readVal("presetsTabEntryH", 28.0f);
        auto presets = av.listPresets();
        for (auto& p : presets)
        {
            bool active = (p == av.current().activePreset);
            glm::vec4 pCol = active ? readBgCol("outfitsEntrySelected", {0.2f, 0.45f, 0.28f, 1.0f})
                                    : readBgCol("outfitsEntryNormal", {0.08f, 0.09f, 0.14f, 1.0f});
            UIRect pr = {px + 4.0f, cy, pw - 8.0f, entryH};
            UIRect ps = {uiScaleX(px + 4.0f), uiScaleY(cy), uiScaleX(pw - 8.0f), uiScaleY(entryH)};
            uiDrawRect(ps, pCol, "preset-entry");
            if (active)
                uiDrawRectOutline(ps, {0.3f, 0.8f, 0.5f, 1.0f}, "preset-active");
            uiDrawText(p.c_str(), ps.x + 8.0f, ps.y + 4.0f, 0.60f, {1,1,1,1});
            if (uiButton(win, "", pr, pCol).clicked) {
                av.loadPreset(p);
                liveApply();
            }
            cy += 30.0f;
        }

        if (presets.empty())
            uiDrawText("No presets saved yet.", uiScaleX(px + 4.0f), uiScaleY(cy), 0.56f, {0.4f, 0.5f, 0.6f, 1.0f});
        break;
    }
    }
}

} // anonymous namespace

// ── Popup state (definitions, extern in avatar-menu.h) ──────────────
bool gSavePopupOpen = false;
char gSaveNameBuf[64] = "";
bool gRenamePopupOpen = false;
char gRenameBuf[64] = "";
bool gDeleteConfirmOpen = false;
static std::string gRenameTarget;
static std::string gDeleteTarget;

// ── Drag & drop ─────────────────────────────────────────────────────
void avatarMenuHandleDrop(int count, const char** paths)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;
    int imported = 0;
    for (int i = 0; i < count; ++i) {
        std::string p = paths[i];
        std::string ext = std::filesystem::path(p).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png") {
            if (av.importPng(p))
                imported++;
        }
    }
    if (imported > 0) {
        Terminal::instance().addLog("[AVATAR] Imported " + std::to_string(imported) + " PNG(s)");
    }
}

AvatarMenuResult drawAvatarMenu(GLFWwindow* win)
{
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    GuiLayout& layout = getLayout();

    // Helper: read numeric value from layout text element (x field used as value)
    auto readVal = [&](const char* id, float def) -> float {
        const GuiElement* e = layout.get(id);
        return e ? e->x : def;
    };
    // Helper: read background color from layout element
    auto readCol = [&](const char* id, glm::vec4 def) -> glm::vec4 {
        const GuiElement* e = layout.get(id);
        return e ? e->getBackgroundColorVec() : def;
    };
    // Helper: read text color from layout element
    auto readTextCol = [&](const char* id, glm::vec4 def) -> glm::vec4 {
        const GuiElement* e = layout.get(id);
        return e ? e->getTextColorVec() : def;
    };
    // Helper: draw a layout element if it exists and is visible
    auto drawIf = [&](const char* id) {
        const GuiElement* e = layout.get(id);
        if (e && e->visible) drawGuiElement(win, *e);
    };

    // Panel positions from config
    float libX = lx("panelLibrary", 20.0f);
    float libY = ly("panelLibrary", 50.0f);
    float libW = lw("panelLibrary", 280.0f);
    float libH = lh("panelLibrary", 950.0f);

    float editX = lx("panelEditor", 320.0f);
    float editY = ly("panelEditor", 50.0f);
    float editW = lw("panelEditor", 560.0f);
    float editH = lh("panelEditor", 950.0f);

    // ── Draw panels and static text from JSON ──────────────────────
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        // Skip elements drawn manually below
        if (id == "tabFaces" || id == "tabColors" || id == "tabCosmetics" || id == "tabPresets")
            continue;

        // Draw panels and static text through the layout system
        if (elem->type == "panel" || elem->type == "text" || elem->type == "label")
        {
            drawGuiElement(win, *elem);
            continue;
        }
    }

    drawLibraryPanel(win, libX + 4.0f, libY + 30.0f, libW - 8.0f, libH - 35.0f);

    drawEditorPanel(win, editX + 4.0f, editY + 68.0f, editW - 8.0f, editH - 104.0f);

    // ── Tab buttons from JSON (with active state highlight) ──────────
    const char* tabIds[] = {"tabFaces", "tabColors", "tabCosmetics", "tabPresets"};
    for (int ti = 0; ti < 4; ++ti) {
        const GuiElement* te = layout.get(tabIds[ti]);
        if (!te || !te->visible) continue;
        glm::vec4 tabBg = (ti == gEditorTab)
            ? te->getPressedColorVec()  // Use pressedColor for active tab
            : te->getBackgroundColorVec();
        // Fallback if pressedColor is empty
        if (ti == gEditorTab && !te->hasPressedColor())
            tabBg = glm::vec4{0.2f, 0.45f, 0.3f, 1.0f};
        if (uiButton(win, te->text.c_str(), {te->x, te->y, te->w, te->h}, tabBg, te->id.c_str()).clicked)
            gEditorTab = ti;
    }

    // ── Bottom buttons: Save, Apply, Back ────────────────────────────
    {
        const char* btnIds[] = {"saveButton", "applyButton", "backButton"};
        for (int i = 0; i < 3; ++i) {
            const GuiElement* elem = layout.get(btnIds[i]);
            if (!elem || !elem->visible) continue;
            UIButtonState s = uiButton(win, elem->text.c_str(),
                {elem->x, elem->y, elem->w, elem->h},
                elem->getBackgroundColorVec(), elem->id.c_str());
            if (!s.clicked) continue;
            if (elem->id == "saveButton") {
                if (av.hasAvatar()) {
                    // Open save popup instead of saving immediately
                    gSavePopupOpen = true;
                    memset(gSaveNameBuf, 0, sizeof(gSaveNameBuf));
                    strncpy(gSaveNameBuf, av.currentName().c_str(), sizeof(gSaveNameBuf) - 1);
                    r.savePopupOpen = true;
                }
            } else if (elem->id == "applyButton") {
                r.goApply = true;
            } else if (elem->id == "backButton") {
                r.goBack = true;
                clearTexturePreview();
            }
        }
    }

    // ── Save Outfit Popup (from JSON) ────────────────────────────────
    if (gSavePopupOpen) {
        drawIf("savePopupBg");
        drawIf("savePopupTitle");
        drawIf("savePopupNameLabel");
        drawIf("savePopupInput");

        // Text input (dynamic text)
        const GuiElement* si = layout.get("savePopupInput");
        if (si) {
            float inputX = cs.designToScreenX(si->x);
            float inputY = cs.designToScreenY(si->y);
            uiDrawText(gSaveNameBuf[0] ? gSaveNameBuf : "name...",
                       inputX + 6.0f, inputY + 4.0f, 0.32f,
                       gSaveNameBuf[0] ? glm::vec4{1,1,1,1} : glm::vec4{0.4f, 0.4f, 0.5f, 1.0f});
        }

        // Save and Cancel buttons
        const GuiElement* saveBtn = layout.get("savePopupSaveBtn");
        if (saveBtn && saveBtn->visible) {
            if (uiButton(win, saveBtn->text.c_str(), {saveBtn->x, saveBtn->y, saveBtn->w, saveBtn->h},
                         saveBtn->getBackgroundColorVec(), "save-popup-save").clicked) {
                std::string name(gSaveNameBuf);
                name.erase(0, name.find_first_not_of(" \t\r\n"));
                name.erase(name.find_last_not_of(" \t\r\n") + 1);
                if (!name.empty()) {
                    av.saveCurrentOutfit(name);
                    gSavePopupOpen = false;
                    r.goSave = true;
                }
            }
        }
        const GuiElement* cancelBtn = layout.get("savePopupCancelBtn");
        if (cancelBtn && cancelBtn->visible) {
            if (uiButton(win, cancelBtn->text.c_str(), {cancelBtn->x, cancelBtn->y, cancelBtn->w, cancelBtn->h},
                         cancelBtn->getBackgroundColorVec(), "save-popup-cancel").clicked) {
                gSavePopupOpen = false;
            }
        }
    }

    // ── Outfit Browser (right panel) ────────────────────────────────
    {
        float outfitX = lx("panelOutfits", 1100.0f);
        float outfitY = ly("panelOutfits", 50.0f);
        float outfitW = lw("panelOutfits", 800.0f);
        float outfitH = lh("panelOutfits", 950.0f);

        // Draw panel background from JSON
        drawIf("panelOutfits");

        // Title from JSON
        drawIf("outfitsTitle");

        float oy = outfitY + 36.0f;
        float oH = readVal("outfitsEntryHeight", 28.0f);
        float gap = readVal("outfitsEntryGap", 3.0f);

        // New Outfit button from JSON
        {
            const GuiElement* newBtn = layout.get("outfitsNewButton");
            if (newBtn && newBtn->visible) {
                UIRect nRect = {newBtn->x, oy, newBtn->w, oH};
                if (uiButton(win, newBtn->text.c_str(), nRect,
                             newBtn->getBackgroundColorVec(), "outfits-new").clicked) {
            std::string baseName = "New Outfit";
            std::string testName = baseName;
            int counter = 1;
            auto existing = av.listAvatars();
            while (std::find(existing.begin(), existing.end(), testName) != existing.end()) {
                testName = baseName + " " + std::to_string(counter++);
            }
                 av.createOutfit(testName);
            GetPlayerSettings().avatarName = testName;
            SavePlayerSettings();
            clearTexturePreview();
        }
            }
        }
        oy += oH + gap + 4.0f;

        // Separator from JSON
        drawIf("outfitsSeparator");
        oy += 4.0f;

        // Colors from JSON
        glm::vec4 entrySelCol = readCol("outfitsEntrySelected", {0.2f,0.48f,0.28f,1});
        glm::vec4 entryNormCol = readCol("outfitsEntryNormal", {0.08f,0.09f,0.14f,1});
        float btnW2 = readVal("outfitsActionBtnW", 60.0f);
        float btnH2 = readVal("outfitsActionBtnH", 22.0f);
        float btnGap = readVal("outfitsActionBtnGap", 4.0f);

        // List all outfits
        auto outfits = av.listAvatars();
        for (auto& o : outfits) {
            bool isCurrent = (o == av.currentName());
            glm::vec4 col = isCurrent ? entrySelCol : entryNormCol;

            UIRect entryRect = {outfitX + 8.0f, oy, outfitW - 16.0f, oH};
            uiDrawRect(cs.designToScreen(entryRect), col, "outfit-entry");
            if (isCurrent)
                uiDrawRectOutline(cs.designToScreen(entryRect), {0.3f, 0.8f, 0.5f, 1.0f}, "outfit-sel");

            uiDrawText(o.c_str(), cs.designToScreenX(entryRect.x + 6.0f),
                       cs.designToScreenY(entryRect.y + 4.0f), 0.30f, {1,1,1,1});

            if (uiButton(win, "", entryRect, {0,0,0,0}).clicked) {
                if (!isCurrent) {
                    av.loadAvatar(o);
                    GetPlayerSettings().avatarName = o;
                    SavePlayerSettings();
                    clearTexturePreview();
                }
            }

            // Operation buttons (rename, copy, delete) on current outfit
            if (isCurrent) {
                float bY = oy + oH + 2.0f;
                glm::vec4 rnCol = readCol("outfitsRenameBtn", {0.15f,0.25f,0.4f,1});
                glm::vec4 cpCol = readCol("outfitsCopyBtn", {0.2f,0.35f,0.25f,1});
                glm::vec4 dlCol = readCol("outfitsDeleteBtn", {0.4f,0.12f,0.12f,1});

                if (uiButton(win, "Rename", {outfitX + 8.0f, bY, btnW2, btnH2}, rnCol).clicked) {
                    gRenamePopupOpen = true;
                    memset(gRenameBuf, 0, sizeof(gRenameBuf));
                    strncpy(gRenameBuf, av.currentName().c_str(), sizeof(gRenameBuf) - 1);
                    gRenameTarget = av.currentName();
                }
                if (uiButton(win, "Copy", {outfitX + 8.0f + btnW2 + btnGap, bY, btnW2, btnH2}, cpCol).clicked) {
                    std::string base = av.currentName() + " Copy";
                    std::string test = base;
                    int c = 1;
                    auto existing = av.listAvatars();
                    while (std::find(existing.begin(), existing.end(), test) != existing.end())
                        test = base + " " + std::to_string(c++);
                    av.duplicateOutfit(av.currentName(), test);
                }
                if (uiButton(win, "Delete", {outfitX + 8.0f + 2 * (btnW2 + btnGap), bY, btnW2, btnH2}, dlCol).clicked) {
                    gDeleteConfirmOpen = true;
                    gDeleteTarget = av.currentName();
                }
                oy += btnH2 + 6.0f;
            }
            oy += oH + gap;
        }
    }

    // ── Rename Popup (from JSON) ───────────────────────────────────
    if (gRenamePopupOpen) {
        drawIf("renamePopupBg");
        drawIf("renamePopupTitle");
        drawIf("renamePopupInput");

        // Text input (dynamic)
        const GuiElement* ri = layout.get("renamePopupInput");
        if (ri) {
            float rx = cs.designToScreenX(ri->x);
            float ry = cs.designToScreenY(ri->y);
            uiDrawText(gRenameBuf[0] ? gRenameBuf : "name...",
                       rx + 6.0f, ry + 4.0f, 0.32f,
                       gRenameBuf[0] ? glm::vec4{1,1,1,1} : glm::vec4{0.4f, 0.4f, 0.5f, 1.0f});
        }

        const GuiElement* rnBtn = layout.get("renamePopupConfirmBtn");
        if (rnBtn && rnBtn->visible) {
            if (uiButton(win, rnBtn->text.c_str(), {rnBtn->x, rnBtn->y, rnBtn->w, rnBtn->h},
                         rnBtn->getBackgroundColorVec()).clicked) {
                std::string newName(gRenameBuf);
                newName.erase(0, newName.find_first_not_of(" \t\r\n"));
                newName.erase(newName.find_last_not_of(" \t\r\n") + 1);
                if (!newName.empty()) {
                    if (av.renameOutfit(gRenameTarget, newName)) {
                        av.loadAvatar(newName);
                        GetPlayerSettings().avatarName = newName;
                        SavePlayerSettings();
                    }
                    gRenamePopupOpen = false;
                }
            }
        }
        const GuiElement* cnBtn = layout.get("renamePopupCancelBtn");
        if (cnBtn && cnBtn->visible) {
            if (uiButton(win, cnBtn->text.c_str(), {cnBtn->x, cnBtn->y, cnBtn->w, cnBtn->h},
                         cnBtn->getBackgroundColorVec()).clicked) {
                gRenamePopupOpen = false;
            }
        }
    }

    // ── Delete Confirmation ─────────────────────────────────────────
    // ── Delete Confirmation (from JSON) ─────────────────────────────
    if (gDeleteConfirmOpen) {
        drawIf("deletePopupBg");
        drawIf("deletePopupTitle");
        // Target name
        const GuiElement* dtEl = layout.get("deletePopupBg");
        if (dtEl) {
            float dy = cs.designToScreenY(dtEl->y + 54.0f);
            uiDrawText((std::string("\"") + gDeleteTarget + "\"").c_str(),
                       cs.designToScreenX(dtEl->x + 20.0f), dy, 0.32f, {0.6f, 0.7f, 0.8f, 1.0f});
        }

        const GuiElement* yesBtn = layout.get("deletePopupYesBtn");
        if (yesBtn && yesBtn->visible) {
            if (uiButton(win, yesBtn->text.c_str(), {yesBtn->x, yesBtn->y, yesBtn->w, yesBtn->h},
                         yesBtn->getBackgroundColorVec()).clicked) {
                std::string deleted = gDeleteTarget;
                av.deleteOutfit(deleted);
                auto remaining = av.listAvatars();
                if (!remaining.empty()) {
                    av.loadAvatar(remaining[0]);
                    GetPlayerSettings().avatarName = remaining[0];
                    SavePlayerSettings();
                }
                gDeleteConfirmOpen = false;
                clearTexturePreview();
            }
        }
        const GuiElement* noBtn = layout.get("deletePopupNoBtn");
        if (noBtn && noBtn->visible) {
            if (uiButton(win, noBtn->text.c_str(), {noBtn->x, noBtn->y, noBtn->w, noBtn->h},
                         noBtn->getBackgroundColorVec()).clicked) {
                gDeleteConfirmOpen = false;
            }
        }
    }

    // ── Drag & Drop Hover Indicator ─────────────────────────────────
    if (isDropHoverActive()) {
        const std::string& hoverPath = getDropHoverPath();
        float sw = uiScreenW(), sh = uiScreenH();

        // Top banner with file path
        UIRect banner = {0, 0, sw, 36.0f};
        uiDrawRect(banner, {0.1f, 0.5f, 0.25f, 0.9f}, "drop-banner");
        uiDrawRectOutline(banner, {0.2f, 0.8f, 0.4f, 1.0f}, "drop-banner-border");

        std::string display = "Drop PNG: " + hoverPath;
        if (display.size() > 100) display = "Drop PNG: ..." + display.substr(display.size() - 90);
        float tw = uiMeasureText(display.c_str(), 0.32f);
        uiDrawText(display.c_str(), sw * 0.5f - tw * 0.5f, 6.0f, 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});
    }

    return r;
}
