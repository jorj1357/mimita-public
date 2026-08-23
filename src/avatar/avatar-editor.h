// 08 15 2026, 15 30
/* purpose
* Avatar editor public API and popup input state.
* Draws via the JSON layout and small owner files.
* DOES NOT own networking, avatar baking, or cosmetics rendering.
* DOES NOT replace AvatarSystem or the atlas builder.
*/
#pragma once
#include <GLFW/glfw3.h>
#include <string>

struct UITextInputState;

// Result flags returned by drawAvatarEditor each frame.
struct AvatarEditorResult {
    bool goBack = false;
    bool goApply = false;
    bool goSave = false;
    bool savePopupOpen = false;
    bool renamePopupOpen = false;
    bool deleteConfirmOpen = false;
    std::string targetOutfit;
};

// Draw the avatar editor (library / face editor / colors / cosmetics /
// presets / outfits panels + popups). Returns flags for the menu switch.
AvatarEditorResult drawAvatarEditor(GLFWwindow* win);

// Input hooks (text input buffers for popups).
// Returns true if the event was consumed by the avatar editor.
bool avatarEditorHandleChar(unsigned int codepoint);
bool avatarEditorHandleKey(int key, int action, int mods);

// Drag & drop PNG import.
void avatarEditorHandleDrop(int count, const char** paths);

// Popup text-input state (shared with drawAvatarEditor).
extern bool gSavePopupOpen;
extern char gSaveNameBuf[64];
extern bool gRenamePopupOpen;
extern char gRenameBuf[64];
extern bool gDeleteConfirmOpen;
extern bool gPresetInputActive;
extern char gPresetNameBuf[64];
extern UITextInputState gSaveNameState;
