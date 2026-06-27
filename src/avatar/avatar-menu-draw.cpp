#include "avatar-menu.h"
#include "avatar.h"

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
void openPicker(int slot, const std::vector<std::string>& files);
void closePicker();

namespace {

// ── State ───────────────────────────────────────────────────────────
int gOpenPickerSlot = -1;
int gPickerScroll = 0;
std::vector<std::string> gPickerFiles;

std::string gSelectedTexture;
GLuint gSelectedTextureGL = 0;
int gSelectedTexW = 0, gSelectedTexH = 0;
std::unordered_set<int> gCheckedFaces;

int gLibScroll = 0;
int gAssignScroll = 0;
int gEditorTab = 0;

// ── Editor clipboard ────────────────────────────────────────────────
int gCopyPartIdx = -1;
int gCopyFaceIdx = -1;

// ── Color picker state ──────────────────────────────────────────────
int gColorPickerPart = -1;
float gColorPickerHue = 0.0f;

// ── String tables ───────────────────────────────────────────────────
const char* kSimpleLabels[] = {"Face Image", "Shirt Image", "Pants Image", "Skin Image"};
const char* kPartLabels[]   = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"};
const char* kPartKeys[]     = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
const char* kFaceLabels[]   = {"Front", "Back", "Left", "Right", "Top", "Bottom"};
const char* kFaceKeys[]     = {"front", "back", "left", "right", "top", "bottom"};
const char* kTabLabels[]    = {"Faces", "Colors", "Cosmetics", "Presets"};
const char* kStretchLabels[] = {"Stretch", "Fit", "Fill", "Crop", "Tile"};

const char* kCosmeticSlots[] = {"head", "torso", "arms", "legs"};
const char* kCosmeticSlotLabels[] = {"Headwear", "Body", "Arms", "Legs"};
const char* kCosmeticOptions[] = {"none", "halo", "horns", "fedora", "helmet", "mask", "hair",
                                  "wings", "cape", "backpack", "sword_pack", "vest", "bandolier",
                                  "gloves", "shoulder_pads", "armbands",
                                  "shoes", "chains", "pants", "knee_pads"};
const int kCosmeticOptionCount = 20;

// ── Layout helper ────────────────────────────────────────────────────
static GuiLayout& getLayout()
{
    return GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
}

static float lx(const std::string& id, float defaultV = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->x : defaultV;
}

static float ly(const std::string& id, float defaultV = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->y : defaultV;
}

static float lw(const std::string& id, float defaultV = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->w : defaultV;
}

static float lh(const std::string& id, float defaultV = 0.0f)
{
    const GuiElement* e = getLayout().get(id);
    return e ? e->h : defaultV;
}

// Auto-apply current avatar to gpPlayer for live preview
static void liveApply()
{
    if (gpPlayer && AvatarSystem::instance().hasAvatar())
        AvatarSystem::instance().applyToPlayer(*gpPlayer);
}

// ── Helpers ─────────────────────────────────────────────────────────
void stopInput() {}

int faceSlotIndex(int part, int face) { return part * 6 + face; }

void clearTexturePreview() {
    if (gSelectedTextureGL) { glDeleteTextures(1, &gSelectedTextureGL); gSelectedTextureGL = 0; }
    gSelectedTexture.clear();
    gSelectedTexW = gSelectedTexH = 0;
}

void loadTexturePreview(const std::string& avatarName, const std::string& filename) {
    clearTexturePreview();
    gSelectedTexture = filename;
    std::string fullPath = AvatarSystem::instance().avatarPath(avatarName) + "/" + filename;
    gSelectedTextureGL = loadMediaTexture(fullPath.c_str(), &gSelectedTexW, &gSelectedTexH);
}

void checkAllFaces() { for (int i = 0; i < 36; ++i) gCheckedFaces.insert(i); }
void uncheckAllFaces() { gCheckedFaces.clear(); }
void toggleFaceCheck(int idx) {
    if (gCheckedFaces.count(idx)) gCheckedFaces.erase(idx);
    else gCheckedFaces.insert(idx);
}

// ── Slider helper ───────────────────────────────────────────────────
struct SliderRange { float min, max, step; };

static float drawSlider(GLFWwindow* win, float x, float y, float w, float val,
                         const SliderRange& range, const char* label) {
    float sx = uiScaleX(x), sy = uiScaleY(y), sw = uiScaleX(w), sh = uiScaleY(20.0f);
    if (label) uiDrawText(label, sx - uiScaleX(60.0f), sy, 0.25f, {0.7f, 0.8f, 0.9f, 1.0f});

    UIRect track = {sx, sy + sh * 0.3f, sw, sh * 0.4f};
    uiDrawRect(track, {0.12f, 0.13f, 0.18f, 1.0f}, "slider-track");

    float t = (val - range.min) / (range.max - range.min);
    UIRect fill = {sx, sy + sh * 0.3f, sw * std::max(0.001f, std::min(1.0f, t)), sh * 0.4f};
    uiDrawRect(fill, {0.25f, 0.55f, 0.35f, 1.0f}, "slider-fill");

    float hx = sx + sw * t - 4.0f;
    UIRect handle = {hx, sy, 8.0f, sh};
    uiDrawRect(handle, {0.6f, 0.85f, 0.7f, 1.0f}, "slider-handle");

    UIRect clickArea = {x, y, w, 20.0f};
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
    uiDrawText(buf, sx + sw + 4.0f, sy, 0.25f, {1.0f, 1.0f, 1.0f, 1.0f});
    return val;
}

// ── Color picker ────────────────────────────────────────────────────
static void drawColorPicker(GLFWwindow* win, float x, float y, float w, glm::vec3& color) {
    float sectionH = 140.0f;
    UIRect bgScreen = {uiScaleX(x), uiScaleY(y), uiScaleX(w), uiScaleY(sectionH)};
    uiDrawRect(bgScreen, {0.06f, 0.07f, 0.12f, 0.95f}, "colorpicker-bg");
    uiDrawRectOutline(bgScreen, {0.3f, 0.5f, 0.8f, 0.6f}, "colorpicker-border");

    float cy = y + 4.0f;
    uiDrawText("Part Color", uiScaleX(x + 4.0f), uiScaleY(cy), 0.28f, {0.8f, 0.9f, 1.0f, 1.0f});
    cy += 22.0f;

    color.r = drawSlider(win, x + 4.0f, cy, w - 80.0f, color.r, {0, 1, 0.01f}, "R");
    cy += 22.0f;
    color.g = drawSlider(win, x + 4.0f, cy, w - 80.0f, color.g, {0, 1, 0.01f}, "G");
    cy += 22.0f;
    color.b = drawSlider(win, x + 4.0f, cy, w - 80.0f, color.b, {0, 1, 0.01f}, "B");
    cy += 22.0f;

    float swatchX = x + w - 70.0f;
    float swatchY = y + 4.0f;
    UIRect swatchScreen = {uiScaleX(swatchX), uiScaleY(swatchY), uiScaleX(60.0f), uiScaleY(60.0f)};
    uiDrawRect(swatchScreen, {color.r, color.g, color.b, 1.0f}, "colorpicker-swatch");
    uiDrawRectOutline(swatchScreen, {0.5f, 0.5f, 0.5f, 0.8f}, "colorpicker-border");

    cy += 4.0f;
    if (uiButton(win, "RESET", {x + 4.0f, cy, 80.0f, 22.0f}, {0.3f, 0.15f, 0.15f, 1.0f}).clicked)
        color = glm::vec3(1.0f);
}

// ── Draw PNG library thumbnail ──────────────────────────────────────
static void drawLibraryPanel(GLFWwindow* win, float panelX, float panelY, float panelW, float panelH) {
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;

    std::string avatarName = av.currentName();
    auto pngs = av.listPngs(avatarName);

    uiDrawText("PNG Library", uiScaleX(panelX), uiScaleY(panelY), 0.40f, {0.9f, 0.95f, 1.0f, 1.0f});
    float cy = panelY + 30.0f;

    uiDrawText("Drag PNGs from Explorer into the window", uiScaleX(panelX), uiScaleY(cy), 0.20f, {0.4f, 0.5f, 0.6f, 1.0f});
    cy += 18.0f;
    uiDrawText("to add them to your library.", uiScaleX(panelX), uiScaleY(cy), 0.20f, {0.4f, 0.5f, 0.6f, 1.0f});
    cy += 24.0f;

    int cols = 3;
    float thumbSize = (panelW - 8.0f - (cols - 1) * 4.0f) / cols;
    float thumbTotalH = thumbSize + 18.0f;
    int visibleRows = std::max(1, (int)((panelY + panelH - cy - 10.0f) / thumbTotalH));
    int visibleCount = visibleRows * cols;

    if (gLibScroll > 0) {
        if (uiButton(win, "^", {lx("libScrollUp", panelX + panelW - 24.0f), cy - 16.0f, 22.0f, 16.0f}, {0.2f, 0.3f, 0.45f, 1.0f}).clicked)
            gLibScroll = std::max(0, gLibScroll - cols);
    }

    int startIdx = gLibScroll;
    int endIdx = std::min((int)pngs.size(), startIdx + visibleCount);

    float tx = panelX + 4.0f;
    float ty = cy;
    for (int i = startIdx; i < endIdx; ++i) {
        int col = (i - startIdx) % cols;
        int row = (i - startIdx) / cols;
        float ix = tx + col * (thumbSize + 4.0f);
        float iy = ty + row * thumbTotalH;

        std::string fullPath = av.avatarPath(avatarName) + "/" + pngs[i];
        UIRect thumbRect = {ix, iy, thumbSize, thumbSize};
        UIRect thumbScreen = {uiScaleX(ix), uiScaleY(iy), uiScaleX(thumbSize), uiScaleY(thumbSize)};
        bool isSelected = (pngs[i] == gSelectedTexture);
        glm::vec4 bgCol = isSelected ? glm::vec4{0.2f, 0.5f, 0.3f, 1.0f} : glm::vec4{0.07f, 0.08f, 0.12f, 1.0f};
        uiDrawRect(thumbScreen, bgCol, "thumb-bg");
        if (isSelected) uiDrawRectOutline(thumbScreen, {0.3f, 0.8f, 0.5f, 1.0f}, "thumb-sel");

        uiDrawImage(fullPath.c_str(), thumbRect);
        if (uiButton(win, "", thumbRect, bgCol).clicked) {
            loadTexturePreview(avatarName, pngs[i]);
        }

        std::string label = pngs[i];
        if (label.size() > 15) label = label.substr(0, 12) + "...";
        uiDrawText(label.c_str(), uiScaleX(ix), uiScaleY(iy + thumbSize + 2.0f), 0.20f, {0.6f, 0.7f, 0.8f, 1.0f});
    }

    if (endIdx < (int)pngs.size()) {
        float scrollBtnY = ty + visibleRows * thumbTotalH + 2.0f;
        if (uiButton(win, "v", {lx("libScrollDown", panelX + panelW - 24.0f), scrollBtnY, 22.0f, 16.0f}, {0.2f, 0.3f, 0.45f, 1.0f}).clicked)
            gLibScroll = std::min(gLibScroll + cols, (int)pngs.size() - cols);
    }
}

// ── Draw apply & edit panel ─────────────────────────────────────────
static void drawEditorPanel(GLFWwindow* win, float panelX, float panelY, float panelW, float panelH) {
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;

    float tabH = 26.0f;
    float tabW = panelW / 4.0f;
    for (int i = 0; i < 4; ++i) {
        glm::vec4 tabCol = (i == gEditorTab)
            ? glm::vec4{0.2f, 0.45f, 0.3f, 1.0f}
            : glm::vec4{0.08f, 0.09f, 0.14f, 1.0f};
        UIRect tabRect = {panelX + i * tabW, panelY, tabW - 2.0f, tabH};
        if (uiButton(win, kTabLabels[i], tabRect, tabCol).clicked)
            gEditorTab = i;
    }
    float cy = panelY + tabH + 4.0f;
    float remainingH = panelY + panelH - cy;

    switch (gEditorTab) {
    case 0: { // Faces tab
        if (gSelectedTexture.empty()) {
            uiDrawText("Select a PNG from the library to begin.", uiScaleX(panelX), uiScaleY(cy), 0.24f, {0.4f, 0.5f, 0.6f, 1.0f});
            cy += 20.0f;
            uiDrawText("Then choose which body parts and faces to apply it to.", uiScaleX(panelX), uiScaleY(cy), 0.24f, {0.4f, 0.5f, 0.6f, 1.0f});
            break;
        }

        if (gSelectedTextureGL) {
            float previewSize = 80.0f;
            UIRect previewRect = {panelX, cy, previewSize, previewSize};
            std::string texPath = av.avatarPath(av.currentName()) + "/" + gSelectedTexture;
            uiDrawImage(texPath.c_str(), previewRect);
            uiDrawText(gSelectedTexture.c_str(), uiScaleX(panelX + previewSize + 6.0f), uiScaleY(cy + 4.0f),
                       0.22f, {0.6f, 0.9f, 0.6f, 1.0f});
            if (uiButton(win, "X", {panelX + previewSize - 14.0f, cy - 2.0f, 16.0f, 16.0f},
                         {0.5f, 0.15f, 0.15f, 1.0f}).clicked) {
                clearTexturePreview();
                uncheckAllFaces();
                break;
            }
            cy += previewSize + 6.0f;

            float qaH = 24.0f;
            float btnW = (panelW - 12.0f) / 4.0f;
            auto qBtn = [&](const char* label, float x, const std::string& partKey) {
                if (uiButton(win, label, {x, cy, btnW, qaH}, {0.15f, 0.3f, 0.5f, 1.0f}).clicked) {
                    for (int fi = 0; fi < 6; ++fi)
                        av.setPartFace(partKey, kFaceKeys[fi], gSelectedTexture);
                    checkAllFaces();
                    liveApply();
                }
            };
            qBtn("Head", panelX, "head");
            qBtn("Torso", panelX + btnW + 4.0f, "torso");
            qBtn("Arms", panelX + 2 * (btnW + 4.0f), "leftArm");
            cy += qaH + 2.0f;
            qBtn("Legs", panelX, "legs");
            if (uiButton(win, "All Body", {panelX + btnW + 4.0f, cy, panelW - btnW - 4.0f, qaH}, {0.2f, 0.4f, 0.25f, 1.0f}).clicked) {
                for (int pi = 0; pi < 6; ++pi)
                    for (int fi = 0; fi < 6; ++fi)
                        av.setPartFace(kPartKeys[pi], kFaceKeys[fi], gSelectedTexture);
                checkAllFaces();
                liveApply();
            }
            cy += qaH + 6.0f;

            if (uiButton(win, "Select All", {panelX, cy, 80.0f, 20.0f}, {0.15f, 0.25f, 0.4f, 1.0f}).clicked)
                checkAllFaces();
            if (uiButton(win, "Clear", {panelX + 86.0f, cy, 60.0f, 20.0f}, {0.35f, 0.15f, 0.15f, 1.0f}).clicked)
                uncheckAllFaces();
            cy += 24.0f;

            float faceListH = remainingH - (cy - panelY - tabH - 4.0f) - 40.0f;
            if (faceListH < 50.0f) faceListH = 50.0f;

            int totalFaceRows = 6;
            float faceRowH = 20.0f;
            float faceRowGap = 1.0f;
            float faceSectionH = 22.0f + 6.0f * (faceRowH + faceRowGap);
            int visibleParts = std::max(1, (int)(faceListH / faceSectionH));

            if (gAssignScroll > 0) {
                if (uiButton(win, "u", {panelX + panelW - 20.0f, cy, 18.0f, 14.0f}, {0.2f, 0.3f, 0.45f, 1.0f}).clicked)
                    gAssignScroll = std::max(0, gAssignScroll - 1);
            }

            float partY = cy;
            for (int pi = gAssignScroll; pi < 6 && pi < gAssignScroll + visibleParts; ++pi) {
                uiDrawText(kPartLabels[pi], uiScaleX(panelX), uiScaleY(partY), 0.26f, {0.70f, 0.80f, 0.90f, 1.0f});
                partY += 20.0f;

                int facesPerCol = 3;
                float colW = (panelW - 6.0f) / 2.0f;
                for (int fi = 0; fi < 6; ++fi) {
                    int col = fi / facesPerCol;
                    int row = fi % facesPerCol;
                    float fx = panelX + col * colW;
                    float fy = partY + row * (faceRowH + faceRowGap);
                    int idx = faceSlotIndex(pi, fi);
                    bool checked = gCheckedFaces.count(idx) > 0;

                    UIRect chkScreen = {uiScaleX(fx), uiScaleY(fy), uiScaleY(faceRowH), uiScaleY(faceRowH)};
                    glm::vec4 cbCol = checked ? glm::vec4{0.2f, 0.7f, 0.3f, 1.0f} : glm::vec4{0.12f, 0.12f, 0.16f, 1.0f};
                    uiDrawRect(chkScreen, cbCol, "chk");
                    if (checked)
                        uiDrawText("\u2713", chkScreen.x + 2.0f, chkScreen.y + 1.0f, 0.22f, {1.0f, 1.0f, 1.0f, 1.0f});
                    if (uiButton(win, "", {fx, fy, faceRowH, faceRowH}, cbCol).clicked)
                        toggleFaceCheck(idx);
                    uiDrawText(kFaceLabels[fi], uiScaleX(fx + faceRowH + 3.0f), uiScaleY(fy + 2.0f),
                               0.22f, {0.7f, 0.75f, 0.85f, 1.0f});
                }
                partY += facesPerCol * (faceRowH + faceRowGap) + 2.0f;
            }

            float applyY = panelY + panelH - 36.0f;
            if (!gCheckedFaces.empty()) {
                if (uiButton(win, "APPLY TO CHECKED FACES", {panelX, applyY, panelW, 30.0f},
                             {0.2f, 0.55f, 0.3f, 1.0f}).clicked) {
                    for (int idx : gCheckedFaces) {
                        int pi = idx / 6;
                        int fi = idx % 6;
                        av.setPartFace(kPartKeys[pi], kFaceKeys[fi], gSelectedTexture);
                    }
                    Terminal::instance().addLog("[AVATAR] Applied to " + std::to_string(gCheckedFaces.size()) + " faces");
                    liveApply();
                }
            }
        }
        break;
    }
    case 1: { // Colors tab
        float colPickerH = 130.0f;
        int partsPerCol = 3;
        float colW = (panelW - 8.0f) / 2.0f;
        float pickerCy = cy;

        for (int pi = 0; pi < 6; ++pi) {
            int col = pi / partsPerCol;
            int row = pi % partsPerCol;
            float px = panelX + col * (colW + 8.0f);
            float py = pickerCy + row * (colPickerH + 4.0f);

            glm::vec3 color = av.partColor(kPartKeys[pi]);
            UIRect swatchScreen = {uiScaleX(px), uiScaleY(py), uiScaleX(20.0f), uiScaleY(20.0f)};
            uiDrawRect(swatchScreen, {color.r, color.g, color.b, 1.0f}, "part-swatch");
            uiDrawText(kPartLabels[pi], uiScaleX(px + 24.0f), uiScaleY(py), 0.26f, {0.8f, 0.85f, 0.95f, 1.0f});

            float sliderX = px + 4.0f;
            float sliderY = py + 22.0f;
            float sliderW = colW - 8.0f;

            float newR = drawSlider(win, sliderX, sliderY, sliderW - 50.0f, color.r, {0, 1, 0.01f}, "R");
            sliderY += 16.0f;
            float newG = drawSlider(win, sliderX, sliderY, sliderW - 50.0f, color.g, {0, 1, 0.01f}, "G");
            sliderY += 16.0f;
            float newB = drawSlider(win, sliderX, sliderY, sliderW - 50.0f, color.b, {0, 1, 0.01f}, "B");

            if (newR != color.r || newG != color.g || newB != color.b) {
                av.setPartColor(kPartKeys[pi], glm::vec3(newR, newG, newB));
                liveApply();
            }
        }
        break;
    }
    case 2: { // Cosmetics tab
        uiDrawText("Cosmetics", uiScaleX(panelX), uiScaleY(cy), 0.32f, {0.8f, 0.9f, 1.0f, 1.0f});
        cy += 26.0f;

        for (int si = 0; si < 4; ++si) {
            uiDrawText(kCosmeticSlotLabels[si], uiScaleX(panelX), uiScaleY(cy), 0.26f, {0.7f, 0.8f, 0.9f, 1.0f});
            cy += 20.0f;

            std::string currentChoice = "none";
            for (auto& c : av.current().cosmetics) {
                if (c.slot == kCosmeticSlots[si]) {
                    currentChoice = c.choice;
                    break;
                }
            }

            float optionW = (panelW - 16.0f) / 5.0f;
            for (int oi = 0; oi < kCosmeticOptionCount; ++oi) {
                int col = oi % 5;
                int row = oi / 5;
                float ox = panelX + col * (optionW + 4.0f);
                float oy = cy + row * 18.0f;

                bool active = (currentChoice == kCosmeticOptions[oi]);
                glm::vec4 btnCol = active ? glm::vec4{0.2f, 0.5f, 0.3f, 1.0f} : glm::vec4{0.08f, 0.09f, 0.13f, 1.0f};
                if (uiButton(win, kCosmeticOptions[oi], {ox, oy, optionW, 16.0f}, btnCol).clicked) {
                    auto& cosmetics = const_cast<std::vector<CosmeticSlot>&>(av.current().cosmetics);
                    bool found = false;
                    for (auto& c : cosmetics) {
                        if (c.slot == kCosmeticSlots[si]) {
                            c.choice = kCosmeticOptions[oi];
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        cosmetics.push_back({kCosmeticSlots[si], kCosmeticOptions[oi]});
                    liveApply();
                }
            }
            cy += ((kCosmeticOptionCount + 4) / 5) * 18.0f + 8.0f;
        }
        break;
    }
    case 3: { // Presets tab
        uiDrawText("Presets", uiScaleX(panelX), uiScaleY(cy), 0.32f, {0.8f, 0.9f, 1.0f, 1.0f});
        cy += 26.0f;

        static char presetNameBuf[64] = "";
        static bool presetInputActive = false;

        uiDrawText("Save as:", uiScaleX(panelX), uiScaleY(cy), 0.24f, {0.5f, 0.6f, 0.7f, 1.0f});
        UIRect inputRect = {uiScaleX(panelX + 60.0f), uiScaleY(cy), uiScaleX(160.0f), uiScaleY(22.0f)};
        uiDrawRect(inputRect, {0.08f, 0.09f, 0.13f, 1.0f}, "preset-input");
        uiDrawText(presetNameBuf[0] ? presetNameBuf : "name...", inputRect.x + 4.0f, inputRect.y + 3.0f, 0.22f,
                   presetNameBuf[0] ? glm::vec4{1,1,1,1} : glm::vec4{0.4f, 0.4f, 0.5f, 1.0f});
        if (uiButton(win, "", {panelX + 60.0f, cy, 160.0f, 22.0f}, {0,0,0,0}).clicked)
            presetInputActive = true;

        if (uiButton(win, "SAVE", {panelX + 230.0f, cy, 70.0f, 22.0f}, {0.2f, 0.5f, 0.3f, 1.0f}).clicked) {
            if (presetNameBuf[0]) {
                av.savePreset(presetNameBuf);
                memset(presetNameBuf, 0, sizeof(presetNameBuf));
            }
        }
        cy += 28.0f;

        auto presets = av.listPresets();
        for (auto& p : presets) {
            bool active = (p == av.current().activePreset);
            glm::vec4 pCol = active ? glm::vec4{0.2f, 0.45f, 0.28f, 1.0f} : glm::vec4{0.08f, 0.09f, 0.14f, 1.0f};
            UIRect pr = {panelX, cy, panelW, 24.0f};
            UIRect ps = {uiScaleX(panelX), uiScaleY(cy), uiScaleX(panelW), uiScaleY(24.0f)};
            uiDrawRect(ps, pCol, "preset-entry");
            if (active) uiDrawRectOutline(ps, {0.3f, 0.8f, 0.5f, 1.0f}, "preset-active");
            uiDrawText(p.c_str(), ps.x + 6.0f, ps.y + 4.0f, 0.26f, {1,1,1,1});
            if (uiButton(win, "", pr, pCol).clicked) {
                av.loadPreset(p);
                liveApply();
            }
            cy += 26.0f;
        }

        if (presets.empty()) {
            uiDrawText("No presets saved yet.", uiScaleX(panelX), uiScaleY(cy), 0.22f, {0.4f, 0.5f, 0.6f, 1.0f});
        }
        break;
    }
    }

}

} // anonymous namespace

AvatarMenuResult drawAvatarMenu(GLFWwindow* win) {
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    // Background
    uiDrawRect({0, 0, (float)fbW, (float)fbH}, {0.028f, 0.032f, 0.045f, 1.0f}, "avatar-bg");

    GuiLayout& layout = getLayout();

    // ── Layout-based panel dimensions ────────────────────────────────
    float libX = lx("panelLibrary", 20.0f);
    float libY = ly("panelLibrary", 50.0f);
    float libW = lw("panelLibrary", 310.0f);
    float libH = lh("panelLibrary", 950.0f);

    float editX = lx("panelEditor", 360.0f);
    float editY = ly("panelEditor", 50.0f);
    float editW = lw("panelEditor", 730.0f);
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
    drawLibraryPanel(win, libX, libY + 20.0f, libW, libH - 20.0f);

    // ── CENTER PANEL: Apply & Edit ──────────────────────────────────
    {
        const GuiElement* panel = layout.get("panelEditor");
        if (panel) {
            UIRect bg = cs.designToScreen({editX, editY, editW, editH});
            uiDrawRect(bg, panel->getBackgroundColorVec(), "edit-bg");
            if (panel->hasOutlineColor())
                uiDrawRectOutline(bg, panel->getOutlineColorVec(), "edit-border");
        }
    }

    // Title
    const GuiElement* titleEl = layout.get("editorTitle");
    if (titleEl) {
        float tx = cs.designToScreenX(titleEl->x);
        float ty = cs.designToScreenY(titleEl->y);
        uiDrawText(titleEl->text.c_str(), tx, ty, titleEl->fontSize, titleEl->getTextColorVec());
    }

    drawEditorPanel(win, editX + 2.0f, editY + 28.0f, editW - 4.0f, editH - 68.0f);

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
    float selX = lx("avatarSelectorLabel", libX) + 52.0f;
    float selY = ly("avatarSelectorLabel", 4.0f);
    float selH = 22.0f;
    {
        const GuiElement* labelEl = layout.get("avatarSelectorLabel");
        if (labelEl) {
            uiDrawText(labelEl->text.c_str(), cs.designToScreenX(labelEl->x), cs.designToScreenY(labelEl->y),
                       labelEl->fontSize, labelEl->getTextColorVec());
        }
    }
    for (auto& a : avatars) {
        if (uiButton(win, a.c_str(), {selX, selY, 80.0f, selH},
                     a == av.currentName() ? glm::vec4{0.2f, 0.48f, 0.28f, 1.0f} : glm::vec4{0.09f, 0.11f, 0.16f, 1.0f}).clicked) {
            if (a != av.currentName()) {
                av.loadAvatar(a);
                GetPlayerSettings().avatarName = a;
                SavePlayerSettings();
                clearTexturePreview();
            }
        }
        selY += selH + 2.0f;
    }

    return r;
}
