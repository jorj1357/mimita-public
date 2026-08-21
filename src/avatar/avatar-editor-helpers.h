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
constexpr int kPartCount = 6;
constexpr int kFaceCount = 6;
extern int gEditorTab;              // 0=PNG 1=Parts 2=Image 3=Cosmetics 4=SaveLoad
extern int gSelectedPart;           // 0-5 body part index
extern int gSelectedFace;           // 0-5 face side index
extern std::string gSelectedTexture;// PNG filename selected in the library
extern bool gSelectedFaces[kPartCount][kFaceCount];
extern int gSelectedCosmetic;
extern bool gEditingCosmeticImage;

void ensureAvatarEditorSelection();
void setSelectedPartFaces(int part, bool selected);
void setAllSelectedFaces(bool selected);
bool anySelectedFace();
int selectedFaceCount();

// ── String tables ───────────────────────────────────────────────────
const char* partLabel(int idx);
const char* partKey(int idx);
const char* faceLabel(int idx);
const char* faceKey(int idx);

// ── Layout helpers ──────────────────────────────────────────────────
GuiLayout& avatarEditorLayout();
float layoutVal(const GuiElement* e, float def);
glm::vec4 layoutBg(const GuiElement* e, glm::vec4 def);
std::string editorLabelText(const char* id, const char* fallback);
float editorLabelFontSize(const char* id, float fallback);

// ── Font sizes (Avatar Editor JSON style owner) ────────────────────
// Compatibility names resolve through avatar-creator.json on every draw.
float editorStyleFontSize(const char* styleId, float fallback);
#define avatarEditorTabFontSize         editorStyleFontSize("editorStyleTab", 0.30f)
#define avatarEditorSmallTabFontSize    editorStyleFontSize("editorStyleSmallTab", 0.30f)
#define avatarEditorButtonFontSize      editorStyleFontSize("editorStyleButton", 0.30f)
#define avatarEditorSmallButtonFontSize editorStyleFontSize("editorStyleSmallButton", 0.30f)
#define avatarEditorSliderLabelFontSize editorStyleFontSize("editorStyleSliderLabel", 0.30f)
#define avatarEditorSliderValueFontSize editorStyleFontSize("editorStyleSliderValue", 0.30f)
#define avatarEditorSectionFontSize     editorStyleFontSize("editorStyleSection", 0.32f)
#define avatarEditorSummaryFontSize     editorStyleFontSize("editorStyleSummary", 0.34f)
#define avatarEditorHintFontSize        editorStyleFontSize("editorStyleHint", 0.30f)
#define avatarEditorOutfitListFontSize  editorStyleFontSize("editorStyleOutfitList", 0.30f)
#define avatarEditorBulkButtonFontSize  editorStyleFontSize("editorStyleBulkButton", 0.30f)

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
struct CosmeticSlot;
CosmeticSlot* avatarEditorSelectedCosmetic();

// Push the current avatar's cosmetics onto the preview player (loads GLBs).
void avatarEditorApplyCosmeticsToPlayer();

// Load an outfit: loadAvatar + apply to the preview player + sync settings.
void avatarEditorLoadOutfit(const std::string& name);
