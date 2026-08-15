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

// ── Font size constants (Avatar Editor only) ────────────────────────
// All sizes are design-space (1920x1080). They are multiplied by
// avatarEditorFontScale() at draw time so text shrinks together with the
// panels on low resolutions (e.g. 1024x768) instead of overflowing the
// controls. Tweak these to fine-tune the editor's text sizes.
constexpr float avatarEditorTabFontSize          = 0.30f;  // top tabs: Faces/Colors/Cosmetics/Presets
constexpr float avatarEditorSmallTabFontSize     = 0.26f;  // part + side rows (Head/.../Front/...)
constexpr float avatarEditorButtonFontSize       = 0.26f;  // regular buttons (assign/save/apply)
constexpr float avatarEditorSmallButtonFontSize  = 0.24f;  // tiny buttons (copy/paste/stretch/crop)
constexpr float avatarEditorSliderLabelFontSize  = 0.26f;  // slider row labels
constexpr float avatarEditorSliderValueFontSize  = 0.24f;  // slider value text
constexpr float avatarEditorSectionFontSize      = 0.26f;  // section labels (PART/SIDE/FIT MODE/...)
constexpr float avatarEditorSummaryFontSize      = 0.32f;  // "Head / Front" selected-face summary
constexpr float avatarEditorHintFontSize         = 0.24f;  // dim hint / empty-state text
constexpr float avatarEditorOutfitListFontSize   = 0.24f;  // outfit names in the right panel
constexpr float avatarEditorBulkButtonFontSize   = 0.24f;  // long "Apply to ..." buttons

// Resolution shrink factor for editor text (1.0 at 1080p and above).
float avatarEditorFontScale();
// Design-space font size -> screen font size (base * avatarEditorFontScale()).
float avatarEditorFont(float baseSize);
// Editor button font size: JSON element's fontSize wins when set (hot-reloadable),
// otherwise the passed C++ constant is used as the default.
float editorFontSize(const GuiElement* e, float defBase);

// Button with an explicit editor font size (keeps call sites one line).
UIButtonState editorButton(GLFWwindow* win, const char* text, UIRect r,
                           glm::vec4 color, float fontSize);

// Draw a labelled slider row (drag to change). Returns true if value changed.
// Positions are design-space (1920x1080). Label sits on the left of the track.
bool drawEditorSlider(GLFWwindow* win, const char* label, float x, float y,
                      float w, float& value, float minVal, float maxVal);

// Request an async atlas rebuild for the live preview (applies next frame).
void avatarEditorRefreshPreview();

// The player used for the editor's live 3D preview.
class Player;
Player* avatarEditorPreviewPlayer();

// Push the current avatar's cosmetics onto the preview player (loads GLBs).
void avatarEditorApplyCosmeticsToPlayer();

// Load an outfit: loadAvatar + apply to the preview player + sync settings.
void avatarEditorLoadOutfit(const std::string& name);
