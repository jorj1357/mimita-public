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

