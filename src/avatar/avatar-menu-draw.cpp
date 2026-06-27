#include "avatar-menu.h"
#include "avatar.h"
#include "avatar-editor-scroll.h"
#include "avatar-editor-dropdown.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_set>

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
    for (int i = 0; i < (int)pngs.size(); ++i)
    {
        int col = i % cols;
        int row = i / cols;
        float ix = tx + col * (thumbSize + gap);
        float iy = py + row * itemH;

        std::string fullPath = av.avatarPath(avatarName) + "/" + pngs[i];
        UIRect thumbRect = {ix, iy, thumbSize, thumbSize};
        UIRect thumbScreen = {uiScaleX(ix), uiScaleY(iy), uiScaleX(thumbSize), uiScaleY(thumbSize)};
        bool isSelected = (pngs[i] == gSelectedTexture);
        glm::vec4 bgCol = isSelected ? bgSelected : bgNormal;
        uiDrawRect(thumbScreen, bgCol, "thumb-bg");
        if (isSelected)
            uiDrawRectOutline(thumbScreen, outlineSel, "thumb-sel");

        uiDrawImage(fullPath.c_str(), thumbRect);
        if (uiButton(win, "", thumbRect, bgCol).clicked)
            loadTexturePreview(avatarName, pngs[i]);

        std::string label = pngs[i];
        if (label.size() > 12) label = label.substr(0, 10) + "...";
        uiDrawText(label.c_str(), uiScaleX(ix), uiScaleY(iy + thumbSize + 4.0f), 0.48f, {0.6f, 0.7f, 0.8f, 1.0f});
    }

    endScroll(scrollArea, contentH, ss);
}

// ── Draw CENTER panel: Editor tabs + content ────────────────────────
static void drawEditorPanel(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;

    float tabH = 32.0f;
    float tabW = (pw - 6.0f) / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        glm::vec4 tabCol = (i == gEditorTab)
            ? glm::vec4{0.2f, 0.45f, 0.3f, 1.0f}
            : glm::vec4{0.08f, 0.09f, 0.14f, 1.0f};
        UIRect tabRect = {px + i * (tabW + 2.0f), py, tabW, tabH};
        if (uiButton(win, kTabLabels[i], tabRect, tabCol, "editor-tab").clicked)
            gEditorTab = i;
    }

    float cy = py + tabH + 8.0f;
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
            uiDrawText("Select a PNG from the library.", uiScaleX(px + 4.0f), uiScaleY(cy), 0.64f, {0.5f, 0.6f, 0.7f, 1.0f});
            cy += 24.0f;
            uiDrawText("Then assign it to body part faces below.", uiScaleX(px + 4.0f), uiScaleY(cy), 0.64f, {0.5f, 0.6f, 0.7f, 1.0f});
            cy += 28.0f;
        }
        else
        {
            // Texture preview
            float previewSize = 80.0f;
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
            float qaH = 30.0f;
            float btnW = (pw - 24.0f) / 5.0f;
            auto qBtn = [&](const char* label, float x, const std::string& partKey) {
                if (uiButton(win, label, {x, cy, btnW, qaH}, {0.15f, 0.3f, 0.5f, 1.0f}).clicked) {
                    for (int fi = 0; fi < kFaceCount; ++fi)
                        av.setPartFace(partKey, kFaceKeys[fi], gSelectedTexture);
                    checkAllFaces();
                    liveApply();
                }
            };
            qBtn("Head", px + 4.0f, "head");
            qBtn("Torso", px + 4.0f + btnW + 4.0f, "torso");
            qBtn("Arms", px + 4.0f + 2 * (btnW + 4.0f), "leftArm");
            qBtn("Legs", px + 4.0f + 3 * (btnW + 4.0f), "leftLeg");
            if (uiButton(win, "All", {px + 4.0f + 4 * (btnW + 4.0f), cy, btnW, qaH}, {0.2f, 0.4f, 0.25f, 1.0f}).clicked) {
                for (int pi = 0; pi < kPartCount; ++pi)
                    for (int fi = 0; fi < kFaceCount; ++fi)
                        av.setPartFace(kPartKeys[pi], kFaceKeys[fi], gSelectedTexture);
                checkAllFaces();
                liveApply();
            }
            cy += qaH + 8.0f;

            // Select all / Clear
            if (uiButton(win, "Select All", {px + 4.0f, cy, 100.0f, 26.0f}, {0.15f, 0.25f, 0.4f, 1.0f}).clicked)
                checkAllFaces();
            if (uiButton(win, "Clear", {px + 112.0f, cy, 80.0f, 26.0f}, {0.35f, 0.15f, 0.15f, 1.0f}).clicked)
                uncheckAllFaces();
            cy += 32.0f;

            // Face grid
            contentH = cy - contentArea.y + kPartCount * 100.0f;

            for (int pi = 0; pi < kPartCount; ++pi)
            {
                uiDrawText(kPartLabels[pi], uiScaleX(px + 4.0f), uiScaleY(cy), 0.64f, {0.70f, 0.80f, 0.90f, 1.0f});
                cy += 24.0f;

                int facesPerRow = 3;
                float colW = (pw - 12.0f) / 2.0f;
                float faceH = 22.0f;
                for (int fi = 0; fi < kFaceCount; ++fi)
                {
                    int col = fi / facesPerRow;
                    int row = fi % facesPerRow;
                    float fx = px + 4.0f + col * colW;
                    float fy = cy + row * (faceH + 2.0f);
                    int idx = faceSlotIndex(pi, fi);
                    bool checked = gCheckedFaces.count(idx) > 0;

                    UIRect chkScreen = {uiScaleX(fx), uiScaleY(fy), uiScaleY(faceH), uiScaleY(faceH)};
                    glm::vec4 cbCol = checked ? glm::vec4{0.2f, 0.7f, 0.3f, 1.0f} : glm::vec4{0.12f, 0.12f, 0.16f, 1.0f};
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
            if (uiButton(win, "APPLY TO CHECKED FACES", {px + 4.0f, applyY, pw - 8.0f, 34.0f},
                         {0.2f, 0.55f, 0.3f, 1.0f}).clicked) {
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

        for (int pi = 0; pi < kPartCount; ++pi)
        {
            glm::vec3 color = av.partColor(kPartKeys[pi]);
            UIRect swatchScreen = {uiScaleX(px + 4.0f), uiScaleY(cy), uiScaleX(24.0f), uiScaleY(24.0f)};
            uiDrawRect(swatchScreen, {color.r, color.g, color.b, 1.0f}, "part-swatch");
            uiDrawText(kPartLabels[pi], uiScaleX(px + 34.0f), uiScaleY(cy), 0.68f, {0.8f, 0.85f, 0.95f, 1.0f});

            float sliderX = px + 8.0f;
            float sliderY = cy + 28.0f;
            float sliderW = pw - 60.0f;

            float newR = drawSlider(win, sliderX, sliderY, sliderW, color.r, {0, 1, 0.01f}, "R");
            sliderY += 20.0f;
            float newG = drawSlider(win, sliderX, sliderY, sliderW, color.g, {0, 1, 0.01f}, "G");
            sliderY += 20.0f;
            float newB = drawSlider(win, sliderX, sliderY, sliderW, color.b, {0, 1, 0.01f}, "B");

            if (newR != color.r || newG != color.g || newB != color.b) {
                av.setPartColor(kPartKeys[pi], glm::vec3(newR, newG, newB));
                liveApply();
            }
            cy += 90.0f;
        }

        endScroll(contentArea, contentH, cs);
        break;
    }

    case 2: // Cosmetics tab
    {
        uiDrawText("Cosmetics", uiScaleX(px + 4.0f), uiScaleY(cy), 0.72f, {0.8f, 0.9f, 1.0f, 1.0f});
        cy += 30.0f;

        // Scan cosmetics folder
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
        float ddItemH = 28.0f;

        for (int si = 0; si < 4; ++si)
        {
            std::string currentChoice = "none";
            for (auto& c : av.current().cosmetics)
            {
                if (c.slot == kCosmeticSlots[si])
                {
                    currentChoice = c.choice;
                    break;
                }
            }

            // Find index for dropdown
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
                    if (c.slot == kCosmeticSlots[si]) {
                        c.choice = cosmeticItems[sel];
                        found = true;
                        break;
                    }
                }
                if (!found)
                    cosmetics.push_back({kCosmeticSlots[si], cosmeticItems[sel]});
                liveApply();
            }

            cy += ddItemH + 12.0f;
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

        if (uiButton(win, "SAVE", {px + 270.0f, cy, 80.0f, 28.0f}, {0.2f, 0.5f, 0.3f, 1.0f}).clicked)
        {
            if (presetNameBuf[0]) {
                av.savePreset(presetNameBuf);
                memset(presetNameBuf, 0, sizeof(presetNameBuf));
            }
        }
        cy += 34.0f;

        auto presets = av.listPresets();
        for (auto& p : presets)
        {
            bool active = (p == av.current().activePreset);
            glm::vec4 pCol = active ? glm::vec4{0.2f, 0.45f, 0.28f, 1.0f} : glm::vec4{0.08f, 0.09f, 0.14f, 1.0f};
            UIRect pr = {px + 4.0f, cy, pw - 8.0f, 28.0f};
            UIRect ps = {uiScaleX(px + 4.0f), uiScaleY(cy), uiScaleX(pw - 8.0f), uiScaleY(28.0f)};
            uiDrawRect(ps, pCol, "preset-entry");
            if (active)
                uiDrawRectOutline(ps, {0.3f, 0.8f, 0.5f, 1.0f}, "preset-active");
            uiDrawText(p.c_str(), ps.x + 8.0f, ps.y + 4.0f, 0.60f, {1,1,1,1});
            if (uiButton(win, "", pr, pCol).clicked)
            {
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

AvatarMenuResult drawAvatarMenu(GLFWwindow* win)
{
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    GuiLayout& layout = getLayout();

    // Panel positions from config
    float libX = lx("panelLibrary", 20.0f);
    float libY = ly("panelLibrary", 50.0f);
    float libW = lw("panelLibrary", 280.0f);
    float libH = lh("panelLibrary", 950.0f);

    float editX = lx("panelEditor", 320.0f);
    float editY = ly("panelEditor", 50.0f);
    float editW = lw("panelEditor", 560.0f);
    float editH = lh("panelEditor", 950.0f);

    // ── LEFT PANEL: PNG Library ─────────────────────────────────────
    {
        const GuiElement* panel = layout.get("panelLibrary");
        if (panel) {
            UIRect bg = cs.designToScreen({libX, libY, libW, libH});
            uiDrawRect(bg, panel->getBackgroundColorVec(), "lib-bg");
            if (panel->hasOutlineColor())
                uiDrawRectOutline(bg, panel->getOutlineColorVec(), "lib-border");
        }
    }

    // Lib title + hints
    const GuiElement* lt = layout.get("libTitle");
    if (lt) uiDrawText(lt->text.c_str(), cs.designToScreenX(lt->x), cs.designToScreenY(lt->y),
                        lt->fontSize, lt->getTextColorVec());
    const GuiElement* lh1 = layout.get("libImportHint");
    if (lh1) uiDrawText(lh1->text.c_str(), cs.designToScreenX(lh1->x), cs.designToScreenY(lh1->y),
                         lh1->fontSize, lh1->getTextColorVec());
    const GuiElement* lh2 = layout.get("libImportHint2");
    if (lh2) uiDrawText(lh2->text.c_str(), cs.designToScreenX(lh2->x), cs.designToScreenY(lh2->y),
                         lh2->fontSize, lh2->getTextColorVec());

    drawLibraryPanel(win, libX + 4.0f, libY + 30.0f, libW - 8.0f, libH - 35.0f);

    // ── CENTER PANEL: Editor ────────────────────────────────────────
    {
        const GuiElement* panel = layout.get("panelEditor");
        if (panel) {
            UIRect bg = cs.designToScreen({editX, editY, editW, editH});
            uiDrawRect(bg, panel->getBackgroundColorVec(), "edit-bg");
            if (panel->hasOutlineColor())
                uiDrawRectOutline(bg, panel->getOutlineColorVec(), "edit-border");
        }
    }

    const GuiElement* titleEl = layout.get("editorTitle");
    if (titleEl)
        uiDrawText(titleEl->text.c_str(), cs.designToScreenX(titleEl->x), cs.designToScreenY(titleEl->y),
                   titleEl->fontSize, titleEl->getTextColorVec());

    drawEditorPanel(win, editX + 4.0f, editY + 34.0f, editW - 8.0f, editH - 70.0f);

    // ── RIGHT PANEL: No UI drawn here — 3D preview renders behind ─────

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
                    av.saveAdvanced(av.currentName(), av.current());
                    av.loadAvatar(av.currentName());
                    Terminal::instance().addLog("[AVATAR] Saved: " + av.currentName());
                    r.goSave = true;
                }
            } else if (elem->id == "applyButton") {
                r.goApply = true;
            } else if (elem->id == "backButton") {
                r.goBack = true;
                clearTexturePreview();
            }
        }
    }

    // ── Avatar selector ─────────────────────────────────────────────
    auto avatars = av.listAvatars();
    float selX = lx("avatarSelectorLabel", libX) + 60.0f;
    float selY = ly("avatarSelectorLabel", 4.0f);
    float selH = 26.0f;

    const GuiElement* labelEl = layout.get("avatarSelectorLabel");
    if (labelEl)
        uiDrawText(labelEl->text.c_str(), cs.designToScreenX(labelEl->x), cs.designToScreenY(labelEl->y),
                   labelEl->fontSize, labelEl->getTextColorVec());

    for (auto& a : avatars) {
        if (uiButton(win, a.c_str(), {selX, selY, 100.0f, selH},
                     a == av.currentName() ? glm::vec4{0.2f, 0.48f, 0.28f, 1.0f} : glm::vec4{0.09f, 0.11f, 0.16f, 1.0f}).clicked) {
            if (a != av.currentName()) {
                av.loadAvatar(a);
                GetPlayerSettings().avatarName = a;
                SavePlayerSettings();
                clearTexturePreview();
            }
        }
        selY += selH + 3.0f;
    }

    return r;
}
