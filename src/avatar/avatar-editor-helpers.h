// 08 15 2026, 15 30
/* purpose
* Shared editor state, string tables, layout readers, slider helper.
* Declares the live-preview refresh and outfit/cosmetic apply helpers.
* DOES NOT draw editor panels or own avatar data.
*/
#pragma once
#include <string>

#include "gui/gui-layout.h"
#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"
#include "gui/gui-coord.h"

// ── Shared editor state ─────────────────────────────────────────────
extern int gEditorTab;              // 0=Faces 1=Colors 2=Cosmetics 3=Presets
extern int gSelectedPart;           // 0-5 body part index
extern int gSelectedFace;           // 0-5 face side index
extern std::string gSelectedTexture;// PNG filename selected in the library

// ── String tables ───────────────────────────────────────────────────
constexpr int kPartCount = 6;
constexpr int kFaceCount = 6;
const char* partLabel(int idx);
const char* partKey(int idx);
const char* faceLabel(int idx);
const char* faceKey(int idx);

// ── Layout helpers ──────────────────────────────────────────────────
GuiLayout& avatarEditorLayout();
float layoutVal(const GuiElement* e, float def);
glm::vec4 layoutBg(const GuiElement* e, glm::vec4 def);

// Draw a labelled slider row (drag to change). Returns true if value changed.
// Positions are design-space (1920x1080). Label sits on the left of the track.
bool drawEditorSlider(GLFWwindow* win, const char* label, float x, float y,
                      float w, float& value, float minVal, float maxVal);

// Request an async atlas rebuild for the live preview (applies next frame).
void avatarEditorRefreshPreview();

// Push the current avatar's cosmetics onto the local player (loads GLBs).
void avatarEditorApplyCosmeticsToPlayer();

// Load an outfit: loadAvatar + apply to player + sync player settings.
void avatarEditorLoadOutfit(const std::string& name);
