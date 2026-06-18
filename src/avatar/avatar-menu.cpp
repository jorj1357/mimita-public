#include "avatar-menu.h"
#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_set>

#include "../gui/ui-system.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-coord.h"
#include "../gui/gui-media.h"
#include "../devtools/terminal.h"
#include "../config/player-settings.h"

// ─── Forward declarations ──────────────────────────────────────────
class Player;
extern Player* gpPlayer;
void openPicker(int slot, const std::vector<std::string>& files);
void closePicker();

namespace {

// ─── Layout constants ──────────────────────────────────────────────
constexpr float PANEL_LEFT_X = 20.0f;
constexpr float PANEL_LEFT_W = 230.0f;
constexpr float PANEL_RIGHT_X = 1450.0f;
constexpr float PANEL_RIGHT_W = 450.0f;
constexpr float COL2X = 420.0f;

// ─── Input state ───────────────────────────────────────────────────
struct AvatarInput {
    bool active = false;
    char buffer[128]{};
    int cursor = 0;
    std::string label, targetPart, targetFace;
};
AvatarInput gInput;

// ─── Picker state ──────────────────────────────────────────────────
int gOpenPickerSlot = -1;
int gPickerScroll = 0;
std::vector<std::string> gPickerFiles;

// ─── Multi-assign state ────────────────────────────────────────────
std::string gSelectedTexture;            // currently selected texture path
GLuint gSelectedTextureGL = 0;           // cached GL texture for preview
int gSelectedTexW = 0, gSelectedTexH = 0;
std::unordered_set<int> gCheckedFaces;   // set of (part*6 + face) indices

// ─── Scroll state for the assign list ──────────────────────────────
int gAssignScroll = 0;

// ─── String tables ─────────────────────────────────────────────────
const char* kSimpleLabels[] = {"Face Image", "Shirt Image", "Pants Image", "Skin Image"};
const char* kSimpleKeys[]   = {"face", "shirt", "pants", "skin"};
const char* kPartLabels[]   = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"};
const char* kPartKeys[]     = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
const char* kFaceLabels[]   = {"Front", "Back", "Left", "Right", "Top", "Bottom"};
const char* kFaceKeys[]     = {"front", "back", "left", "right", "top", "bottom"};

// ─── Helpers ───────────────────────────────────────────────────────
void stopInput() { gInput.active = false; gInput.label.clear(); }

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

void checkAllFaces() {
    for (int i = 0; i < 36; ++i) gCheckedFaces.insert(i);
}

void uncheckAllFaces() { gCheckedFaces.clear(); }

void toggleFaceCheck(int idx) {
    if (gCheckedFaces.count(idx)) gCheckedFaces.erase(idx);
    else gCheckedFaces.insert(idx);
}

// ─── Draw file picker overlay ──────────────────────────────────────
static void drawPicker(GLFWwindow* win, GuiLayout& layout) {
    if (gOpenPickerSlot < 0 || gPickerFiles.empty()) return;
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const GuiElement* panelEl = layout.get("pickerPanel");
    float pdx = panelEl ? panelEl->x : 400.0f, pdy = panelEl ? panelEl->y : 200.0f;
    float pdw = panelEl ? panelEl->w : 700.0f, pdh = panelEl ? panelEl->h : 500.0f;
    UIRect bg = cs.designToScreen({pdx, pdy, pdw, pdh});
    uiDrawRect(bg, {0.08f, 0.09f, 0.13f, 0.96f}, "picker-bg");
    uiDrawRectOutline(bg, {0.3f, 0.5f, 0.7f, 1.0f}, "picker-border");
    const GuiElement* titleEl = layout.get("pickerTitle");
    float tdx = titleEl ? titleEl->x : (pdx + 14.0f), tdy = titleEl ? titleEl->y : (pdy + 12.0f);
    float tsize = (titleEl && titleEl->fontSize > 0.0f) ? titleEl->fontSize : 0.40f;
    uiDrawText("SELECT TEXTURE", uiScaleX(tdx), uiScaleY(tdy), tsize, {0.8f, 0.85f, 0.95f, 1.0f});
    const GuiElement* closeEl = layout.get("pickerClose");
    float clx = closeEl ? closeEl->x : (pdx + pdw - 36.0f), cly = closeEl ? closeEl->y : (pdy + 8.0f);
    float clw = (closeEl && closeEl->w > 0) ? closeEl->w : 28.0f, clh = (closeEl && closeEl->h > 0) ? closeEl->h : 28.0f;
    if (uiButton(win, "X", {clx, cly, clw, clh}, {0.5f, 0.15f, 0.15f, 1.0f}).clicked) closePicker();
    const GuiElement* rowEl = layout.get("pickerRow");
    float rx = rowEl ? rowEl->x : (pdx + 14.0f), ry = rowEl ? rowEl->y : (pdy + 58.0f);
    float rw = rowEl ? rowEl->w : (pdw - 28.0f), rh = rowEl ? rowEl->h : 32.0f;
    int visible = (int)((pdy + pdh - 10.0f - ry) / rh);
    int count = (int)gPickerFiles.size();
    const GuiElement* suEl = layout.get("pickerScrollUp");
    float sux = suEl ? suEl->x : (pdx + pdw - 46.0f), suy = suEl ? suEl->y : (ry - 26.0f);
    float suw = (suEl && suEl->w > 0) ? suEl->w : 36.0f, suh = (suEl && suEl->h > 0) ? suEl->h : 20.0f;
    if (gPickerScroll > 0 && uiButton(win, "^", {sux, suy, suw, suh}, {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
        gPickerScroll = std::max(0, gPickerScroll - 1);
    float itemY = ry;
    for (int i = gPickerScroll; i < count && i < gPickerScroll + visible; ++i) {
        UIRect rowRect = {rx, itemY, rw, rh - 2.0f};
        UIRect sr = cs.designToScreen(rowRect);
        glm::vec4 fb = (i % 2 == 0) ? glm::vec4{0.12f, 0.13f, 0.17f, 1.0f} : glm::vec4{0.10f, 0.11f, 0.15f, 1.0f};
        uiDrawRect(sr, fb, "picker-file");
        float fsize = (rowEl && rowEl->fontSize > 0.0f) ? rowEl->fontSize : 0.28f;
        uiDrawText(gPickerFiles[i].c_str(), sr.x + 6.0f, sr.y + 4.0f, fsize, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, "", rowRect, fb).clicked) {
            // Load texture and set it as the selected texture for multi-assign
            std::string avatarName = AvatarSystem::instance().currentName();
            if (!avatarName.empty()) loadTexturePreview(avatarName, gPickerFiles[i]);
            closePicker();
        }
    }
    if (gPickerScroll + visible < count) {
        const GuiElement* sdEl = layout.get("pickerScrollDown");
        float sdx = sdEl ? sdEl->x : (pdx + pdw - 46.0f), sdy = sdEl ? sdEl->y : itemY;
        float sdw = (sdEl && sdEl->w > 0) ? sdEl->w : 36.0f, sdh = (sdEl && sdEl->h > 0) ? sdEl->h : 20.0f;
        if (uiButton(win, "v", {sdx, sdy, sdw, sdh}, {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
            gPickerScroll = std::min(gPickerScroll + 1, count - visible);
    }
}

// ─── Draw a checkbox row for one face slot ─────────────────────────
static void drawFaceCheckRow(GLFWwindow* win, float x, float y, float w, float h,
                             const char* label, int slotIndex, bool checked) {
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect sr = cs.designToScreen({x, y, h, h}); // checkbox is square
    glm::vec4 cbCol = checked ? glm::vec4{0.2f, 0.7f, 0.3f, 1.0f} : glm::vec4{0.12f, 0.12f, 0.16f, 1.0f};
    uiDrawRect(sr, cbCol, "chk");
    if (checked) {
        // Draw checkmark
        float cx = sr.x + sr.w * 0.25f, cy = sr.y + sr.h * 0.2f;
        float csize = sr.w * 0.5f;
        uiDrawText("\u2713", cx, cy, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});
    }
    if (uiButton(win, "", {x, y, h, h}, cbCol).clicked) toggleFaceCheck(slotIndex);
    float lx = x + h + 4.0f;
    float lw = w - h - 4.0f;
    uiDrawText(label, uiScaleX(lx), uiScaleY(y + 2.0f), 0.26f, {0.8f, 0.85f, 0.95f, 1.0f});
}

} // anonymous namespace

// ─── Apply functions (sets avatar faces, player apply happens via goApply result) ──
static void applySelectedTextureToCheckedFaces(const std::string& texturePath, AvatarMenuResult& result) {
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar() || texturePath.empty()) return;
    for (int idx : gCheckedFaces) {
        int pi = idx / 6;
        int fi = idx % 6;
        if (pi >= 0 && pi < 6)
            av.setPartFace(kPartKeys[pi], kFaceKeys[fi], texturePath);
    }
    result.goApply = true;
}

static void applyTextureToPart(const std::string& texturePath, const std::string& partKey, AvatarMenuResult& result) {
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar() || texturePath.empty()) return;
    for (int fi = 0; fi < 6; ++fi)
        av.setPartFace(partKey, kFaceKeys[fi], texturePath);
    result.goApply = true;
}

static void applyTextureToAll(const std::string& texturePath, AvatarMenuResult& result) {
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar() || texturePath.empty()) return;
    for (int pi = 0; pi < 6; ++pi)
        for (int fi = 0; fi < 6; ++fi)
            av.setPartFace(kPartKeys[pi], kFaceKeys[fi], texturePath);
    result.goApply = true;
}

// ─── Picker implementations ──
void openPicker(int slot, const std::vector<std::string>& files) {
    gOpenPickerSlot = slot;
    gPickerFiles = files;
    gPickerScroll = 0;
}

void closePicker() { gOpenPickerSlot = -1; gPickerFiles.clear(); }

// ─── Public input handling ─────────────────────────────────────────
void avatarMenuHandleChar(unsigned int codepoint) {
    if (!gInput.active) return;
    if (codepoint >= 32 && codepoint <= 126 && gInput.cursor < (int)sizeof(gInput.buffer) - 1) {
        gInput.buffer[gInput.cursor++] = (char)codepoint;
        gInput.buffer[gInput.cursor] = '\0';
    }
}

void avatarMenuHandleKey(int key, int action) {
    if (!gInput.active || (action != GLFW_PRESS && action != GLFW_REPEAT)) return;
    if (key == GLFW_KEY_BACKSPACE && gInput.cursor > 0) gInput.buffer[--gInput.cursor] = '\0';
    else if (key == GLFW_KEY_ENTER && gInput.cursor > 0) {
        AvatarSystem& av = AvatarSystem::instance();
        if (!gInput.targetFace.empty())
            av.setPartFace(gInput.targetPart, gInput.targetFace, std::string(gInput.buffer));
        else if (!gInput.targetPart.empty()) {
            SimpleAvatar s = av.current().simple;
            if (gInput.targetPart == "face") s.face = gInput.buffer;
            else if (gInput.targetPart == "shirt") s.shirt = gInput.buffer;
            else if (gInput.targetPart == "pants") s.pants = gInput.buffer;
            else if (gInput.targetPart == "skin") s.skin = gInput.buffer;
            av.setSimple(s);
        }
        stopInput();
    } else if (key == GLFW_KEY_ESCAPE) stopInput();
}

// ─── Simple mode row ───────────────────────────────────────────────
struct SlotRow { float x, y, w, h; const char* label; std::string value; int slotIndex; };

static void drawSlotRow(GLFWwindow* win, const SlotRow& s, GuiLayout& layout) {
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    float xs = uiScaleX(s.x), ys = uiScaleY(s.y), ws = uiScaleX(s.w), hs = uiScaleY(s.h);
    float txtSize = 0.32f;
    uiDrawText(s.label, xs, ys, txtSize, {0.75f, 0.85f, 0.95f, 1.0f});
    float tw = uiMeasureText(s.label, txtSize);
    const char* texLabel = s.value.empty() ? "<none>" : s.value.c_str();
    float vx = xs + tw + 8.0f;
    float vw = ws - tw - 8.0f - uiScaleX(90.0f) - uiScaleX(4.0f);
    glm::vec4 vc = s.value.empty() ? glm::vec4{0.12f, 0.12f, 0.16f, 1.0f} : glm::vec4{0.16f, 0.28f, 0.16f, 1.0f};
    uiDrawRect({vx, ys, vw, hs}, vc, "slot-val");
    uiDrawText(texLabel, vx + 6.0f, ys + 4.0f, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});
    if (uiButton(win, "", cs.screenToDesign({vx, ys, vw, hs}), vc).clicked) {
        auto files = AvatarSystem::instance().listPngs(AvatarSystem::instance().currentName());
        if (!files.empty()) openPicker(s.slotIndex, files);
    }
    float bx = vx + vw + uiScaleX(4.0f);
    float bw = uiScaleX(90.0f);
    UIRect browseRect = cs.screenToDesign({bx, ys, bw, hs});
    if (uiButton(win, "BROWSE", browseRect, {0.25f, 0.35f, 0.5f, 1.0f}).clicked) {
        auto files = AvatarSystem::instance().listPngs(AvatarSystem::instance().currentName());
        if (!files.empty()) openPicker(0, files);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  PUBLIC — drawAvatarMenu
// ═══════════════════════════════════════════════════════════════════
AvatarMenuResult drawAvatarMenu(GLFWwindow* win) {
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    // Dark background
    uiDrawRect({0, 0, (float)fbW, (float)fbH}, {0.028f, 0.032f, 0.045f, 1.0f}, "avatar-bg");

    // ═══════════════════════════════════════════════════════════════
    // LEFT PANEL — Avatar List
    // ═══════════════════════════════════════════════════════════════
    const GuiElement* te = layout.get("title");
    float titleY = te ? te->y : 20.0f;
    uiDrawText("AVATARS", uiScaleX(PANEL_LEFT_X), uiScaleY(titleY), 0.55f, {0.95f, 0.98f, 1.0f, 1.0f});

    const GuiElement* ah = layout.get("avatarHeading");
    float headingY = (ah && ah->y > 0) ? ah->y : 70.0f;
    uiDrawText("My Avatars", uiScaleX(PANEL_LEFT_X), uiScaleY(headingY), 0.32f, {0.55f, 0.65f, 0.80f, 1.0f});

    const GuiElement* ae = layout.get("avatarEntry");
    float entryH = (ae && ae->h > 0) ? ae->h : 36.0f;
    float entryW = (ae && ae->w > 0) ? ae->w : PANEL_LEFT_W;
    const GuiElement* asp = layout.get("avatarEntrySpacing");
    float avatarSpacing = (asp && asp->h > 0) ? asp->h : 4.0f;
    float avatarStep = entryH + avatarSpacing;
    float listY = headingY + 32.0f;
    float listBottom = fbH > 0 ? (float)fbH / cs.scaleY() - 160.0f : 700.0f;

    std::vector<std::string> avatars = av.listAvatars();
    int maxVisible = std::max(1, (int)((listBottom - listY) / avatarStep));
    float curY = listY;
    for (size_t i = 0; i < (size_t)maxVisible && i < avatars.size(); ++i) {
        UIRect ar = {PANEL_LEFT_X, curY, entryW, entryH};
        UIRect sr = cs.designToScreen(ar);
        bool active = avatars[i] == av.currentName();
        glm::vec4 ab = active ? glm::vec4{0.20f, 0.48f, 0.28f, 1.0f} : glm::vec4{0.09f, 0.11f, 0.16f, 1.0f};
        uiDrawRect(sr, ab, "avatar-entry");
        if (active) uiDrawRectOutline(sr, {0.3f, 0.8f, 0.5f, 1.0f}, "avatar-active");
        float esize = (ae && ae->fontSize > 0.0f) ? ae->fontSize : 0.30f;
        uiDrawText(avatars[i].c_str(), sr.x + 8.0f, sr.y + 5.0f, esize, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, avatars[i].c_str(), ar, ab).clicked && !active) {
            av.loadAvatar(avatars[i]);
            GetPlayerSettings().avatarName = avatars[i];
            SavePlayerSettings();
            clearTexturePreview();
        }
        curY += avatarStep;
    }

    // Active avatar name
    if (av.hasAvatar()) {
        float aty = listY + (float)std::min((size_t)maxVisible, avatars.size()) * avatarStep + 6.0f;
        std::string cur = "Active: " + av.currentName();
        uiDrawText(cur.c_str(), uiScaleX(PANEL_LEFT_X), uiScaleY(aty), 0.28f, {0.5f, 0.9f, 0.5f, 1.0f});
    }

    // ═══════════════════════════════════════════════════════════════
    // RIGHT PANEL — Texture Assignment
    // ═══════════════════════════════════════════════════════════════
    float rightX = PANEL_RIGHT_X;
    float rightW = PANEL_RIGHT_W;

    uiDrawText("TEXTURES", uiScaleX(rightX), uiScaleY(titleY), 0.55f, {0.95f, 0.98f, 1.0f, 1.0f});

    // ── Texture browser ──────────────────────────────────────────
    float secY = headingY;
    uiDrawText("Selected Texture", uiScaleX(rightX), uiScaleY(secY), 0.32f, {0.55f, 0.65f, 0.80f, 1.0f});
    secY += 30.0f;

    // Browse button
    UIRect browseBtn = {rightX, secY, 180.0f, 38.0f};
    if (uiButton(win, "BROWSE PNG", browseBtn, {0.22f, 0.35f, 0.50f, 1.0f}).clicked) {
        auto files = av.listPngs(av.currentName());
        if (!files.empty()) openPicker(0, files);
    }
    // Show current path if a texture is selected
    if (!gSelectedTexture.empty()) {
        std::string pathLabel = gSelectedTexture;
        if (pathLabel.size() > 40) pathLabel = "..." + pathLabel.substr(pathLabel.size() - 37);
        uiDrawText(pathLabel.c_str(), uiScaleX(rightX + 190.0f), uiScaleY(secY + 6.0f), 0.26f,
                   {0.6f, 0.9f, 0.6f, 1.0f});
    }
    secY += 44.0f;

    // Texture preview (large thumbnail)
    if (!gSelectedTexture.empty() && gSelectedTextureGL) {
        float previewX = rightX;
        float previewY = secY;
        float previewSize = 120.0f;
        UIRect previewRect = {previewX, previewY, previewSize + 40.0f, previewSize + 8.0f};
        UIRect sr = cs.designToScreen(previewRect);
        uiDrawRect(sr, {0.04f, 0.05f, 0.08f, 1.0f}, "tex-preview-bg");
        // Draw the actual texture
        std::string texPath = av.avatarPath(av.currentName()) + "/" + gSelectedTexture;
        uiDrawImage(texPath.c_str(), {previewX + 4.0f, previewY + 4.0f, previewSize, previewSize});
        // Clear selection button
        if (uiButton(win, "X", {previewX + previewSize + 44.0f, previewY + 4.0f, 24.0f, 24.0f},
                     {0.5f, 0.15f, 0.15f, 1.0f}).clicked) {
            clearTexturePreview();
            uncheckAllFaces();
        }
        secY += previewSize + 12.0f;
    } else {
        // No texture selected hint
        uiDrawText("No texture selected. Browse to choose one,", uiScaleX(rightX), uiScaleY(secY),
                   0.24f, {0.4f, 0.5f, 0.6f, 1.0f});
        secY += 20.0f;
        uiDrawText("then check which faces to apply it to.", uiScaleX(rightX), uiScaleY(secY),
                   0.24f, {0.4f, 0.5f, 0.6f, 1.0f});
        secY += 26.0f;
    }

    // ── Assign checkboxes ────────────────────────────────────────
    if (!gSelectedTexture.empty()) {
        uiDrawText("Apply To:", uiScaleX(rightX), uiScaleY(secY), 0.32f, {0.75f, 0.85f, 0.95f, 1.0f});
        secY += 28.0f;

        // Quick action buttons
        float qaY = secY;
        float qaH = 28.0f;
        if (uiButton(win, "All Head", {rightX, qaY, 100.0f, qaH}, {0.18f, 0.30f, 0.40f, 1.0f}).clicked)
            applyTextureToPart(gSelectedTexture, "head", r);
        if (uiButton(win, "All Torso", {rightX + 106.0f, qaY, 105.0f, qaH}, {0.18f, 0.30f, 0.40f, 1.0f}).clicked)
            applyTextureToPart(gSelectedTexture, "torso", r);
        if (uiButton(win, "Arms", {rightX + 217.0f, qaY, 100.0f, qaH}, {0.18f, 0.30f, 0.40f, 1.0f}).clicked) {
            applyTextureToPart(gSelectedTexture, "leftArm", r);
            applyTextureToPart(gSelectedTexture, "rightArm", r);
        }
        secY += qaH + 2.0f;
        if (uiButton(win, "Legs", {rightX, secY, 100.0f, qaH}, {0.18f, 0.30f, 0.40f, 1.0f}).clicked) {
            applyTextureToPart(gSelectedTexture, "leftLeg", r);
            applyTextureToPart(gSelectedTexture, "rightLeg", r);
        }
        if (uiButton(win, "Entire Body", {rightX + 106.0f, secY, 200.0f, qaH}, {0.22f, 0.40f, 0.30f, 1.0f}).clicked)
            applyTextureToAll(gSelectedTexture, r);
        secY += qaH + 8.0f;

        // "Select All / Clear" buttons
        if (uiButton(win, "Select All", {rightX, secY, 90.0f, 24.0f}, {0.15f, 0.25f, 0.35f, 1.0f}).clicked)
            checkAllFaces();
        if (uiButton(win, "Clear", {rightX + 96.0f, secY, 70.0f, 24.0f}, {0.15f, 0.15f, 0.20f, 1.0f}).clicked)
            uncheckAllFaces();
        secY += 30.0f;

        // Checkbox list: 6 body parts x 6 faces
        float checkboxStartY = secY;
        float listMaxY = fbH > 0 ? (float)fbH / cs.scaleY() - 90.0f : 800.0f;
        float listH = listMaxY - checkboxStartY;
        if (listH < 60.0f) listH = 60.0f;

        // Scroll buttons
        int totalFaceRows = 6; // 6 body part rows
        float faceRowH = 24.0f;
        float faceRowGap = 2.0f;
        float faceSectionH = 30.0f + 6.0f * (faceRowH + faceRowGap);
        int visibleParts = std::max(1, (int)(listH / faceSectionH));

        if (gAssignScroll > 0) {
            if (uiButton(win, "^", {rightX + rightW - 30.0f, checkboxStartY, 28.0f, 20.0f},
                         {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
                gAssignScroll = std::max(0, gAssignScroll - 1);
        }

        float partY = checkboxStartY;
        for (int pi = gAssignScroll; pi < 6 && pi < gAssignScroll + visibleParts; ++pi) {
            uiDrawText(kPartLabels[pi], uiScaleX(rightX), uiScaleY(partY), 0.30f,
                       {0.70f, 0.80f, 0.90f, 1.0f});
            partY += 24.0f;

            // Arrange faces in 2 columns of 3
            int facesPerCol = 3;
            float colW = (rightW - 10.0f) / 2.0f;
            for (int fi = 0; fi < 6; ++fi) {
                int col = fi / facesPerCol;
                int row = fi % facesPerCol;
                float fx = rightX + col * colW;
                float fy = partY + row * (faceRowH + faceRowGap);
                int idx = faceSlotIndex(pi, fi);
                bool checked = gCheckedFaces.count(idx) > 0;
                drawFaceCheckRow(win, fx, fy, colW - 4.0f, faceRowH, kFaceLabels[fi], idx, checked);
            }
            partY += facesPerCol * (faceRowH + faceRowGap) + 4.0f;
        }

        // Apply Selected button
        float applyY = listMaxY + 4.0f;
        if (!gCheckedFaces.empty()) {
            if (uiButton(win, "Apply To Selected Faces", {rightX, applyY, rightW, 36.0f},
                         {0.22f, 0.50f, 0.30f, 1.0f}).clicked) {
                applySelectedTextureToCheckedFaces(gSelectedTexture, r);
                printf("[AVATAR] Applied %s to %zu faces\n", gSelectedTexture.c_str(), gCheckedFaces.size());
            }
            applyY += 42.0f;
        }

        // Scroll down
        if (gAssignScroll + visibleParts < totalFaceRows) {
            if (uiButton(win, "v", {rightX + rightW - 30.0f, listMaxY - 22.0f, 28.0f, 20.0f},
                         {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
                gAssignScroll = std::min(gAssignScroll + 1, totalFaceRows - visibleParts);
        }

        // Save/Apply/Back
        float btnY = fbH > 0 ? (float)fbH / cs.scaleY() - 48.0f : 700.0f;
        if (uiButton(win, "SAVE", {rightX, btnY, 130.0f, 38.0f},
                     {0.18f, 0.50f, 0.26f, 1.0f}).clicked) {
            if (av.hasAvatar()) {
                av.saveAdvanced(av.currentName(), av.current());
                av.loadAvatar(av.currentName());
                Terminal::instance().addLog("[AVATAR] Saved: " + av.currentName());
                r.goSave = true;
            }
        }
        if (uiButton(win, "APPLY", {rightX + 140.0f, btnY, 130.0f, 38.0f},
                     {0.22f, 0.38f, 0.55f, 1.0f}).clicked)
            r.goApply = true;
        if (uiButton(win, "BACK", {rightX + 280.0f, btnY, 130.0f, 38.0f},
                     {0.50f, 0.18f, 0.18f, 1.0f}).clicked) {
            r.goBack = true;
            clearTexturePreview();
        }
    } else {
        // No texture selected — show simple mode as fallback overview
        float simpleY = secY + 8.0f;
        if (av.hasAvatar()) {
            const SimpleAvatar& sa = av.current().simple;
            float slotW = rightW;
            float slotH = 40.0f;
            float slotGap = 6.0f;
            for (int i = 0; i < 4; ++i) {
                std::string val;
                switch (i) { case 0: val = sa.face; break; case 1: val = sa.shirt; break; case 2: val = sa.pants; break; case 3: val = sa.skin; break; }
                drawSlotRow(win, {rightX, simpleY, slotW, slotH, kSimpleLabels[i], val, i}, layout);
                simpleY += slotH + slotGap;
            }
            // Mapping hints
            simpleY += 4.0f;
            const char* hints[] = {"face \u2192 head front", "shirt \u2192 torso + arms", "pants \u2192 legs", "skin \u2192 head sides/top/back"};
            for (int i = 0; i < 4; ++i) {
                uiDrawText(hints[i], uiScaleX(rightX), uiScaleY(simpleY), 0.22f, {0.45f, 0.55f, 0.65f, 1.0f});
                simpleY += 18.0f;
            }
        }
        // Save/Apply/Back (also shown when no texture selected)
        float btnY = fbH > 0 ? (float)fbH / cs.scaleY() - 48.0f : 700.0f;
        if (uiButton(win, "SAVE", {rightX, btnY, 130.0f, 38.0f},
                     {0.18f, 0.50f, 0.26f, 1.0f}).clicked) {
            if (av.hasAvatar()) {
                if (av.current().advancedMode) av.saveAdvanced(av.currentName(), av.current());
                else av.saveSimple(av.currentName(), av.current().simple);
                av.loadAvatar(av.currentName());
                r.goSave = true;
            }
        }
        if (uiButton(win, "APPLY", {rightX + 140.0f, btnY, 130.0f, 38.0f},
                     {0.22f, 0.38f, 0.55f, 1.0f}).clicked)
            r.goApply = true;
        if (uiButton(win, "BACK", {rightX + 280.0f, btnY, 130.0f, 38.0f},
                     {0.50f, 0.18f, 0.18f, 1.0f}).clicked) {
            r.goBack = true;
        }
    }

    // ── File picker overlay ───────────────────────────────────────
    drawPicker(win, layout);

    return r;
}
