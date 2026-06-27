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

namespace {

// ─── Picker state ──────────────────────────────────────────────────
int gOpenPickerSlot = -1;
int gPickerScroll = 0;
std::vector<std::string> gPickerFiles;

// ─── Multi-assign state ────────────────────────────────────────────
std::string gSelectedTexture;
GLuint gSelectedTextureGL = 0;
int gSelectedTexW = 0, gSelectedTexH = 0;
std::unordered_set<int> gCheckedFaces;

// ─── Scroll state ──────────────────────────────────────────────────
int gAssignScroll = 0;
int gLibScroll = 0;
int gEditorTab = 0;

// ─── Color picker state ────────────────────────────────────────────
int gColorPickerPart = -1;
float gColorPickerHue = 0.0f;

// ─── String tables ─────────────────────────────────────────────────
const char* kPartKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
const char* kFaceKeys[] = {"front", "back", "left", "right", "top", "bottom"};

// ─── Helpers ───────────────────────────────────────────────────────
int faceSlotIndex(int part, int face) { return part * 6 + face; }

void stopInput() {}

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

} // anonymous namespace

// ─── Public picker interface ───────────────────────────────────────
void openPicker(int slot, const std::vector<std::string>& files) {
    gOpenPickerSlot = slot;
    gPickerFiles = files;
    gPickerScroll = 0;
}

void closePicker() { gOpenPickerSlot = -1; gPickerFiles.clear(); }

// ─── Public input handling ─────────────────────────────────────────
void avatarMenuHandleChar(unsigned int codepoint) {
    // Char input handled per-component when needed
}

void avatarMenuHandleKey(int key, int action) {
    // Key input handled per-component when needed
}
