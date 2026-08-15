// 08 15 2026, 15 30
/* purpose
* Avatar editor orchestrator: draws panels, tabs, bottom bar, popups.
* Dispatches tab content to the sibling avatar-editor-*.cpp files.
* DOES NOT implement face/color/cosmetic/preset editing logic.
* DOES NOT own avatar data or atlas baking.
*/
// Avatar editor orchestrator: draws the four panels, tabs, bottom bar and
// popups using the JSON hot-reload layout (config/gui/avatar-creator.json).
// Tab content lives in sibling files (face/colors/cosmetics/presets/outfits).
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-library.h"
#include "avatar-editor-face.h"
#include "avatar-editor-colors.h"
#include "avatar-editor-cosmetics.h"
#include "avatar-editor-presets.h"
#include "avatar-editor-outfits.h"
#include "avatar-editor-popups.h"
#include "avatar-drop-target.h"
#include "avatar.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

#include "gui/gui-element-render.h"
#include "devtools/terminal.h"

// ── Popup / input state ─────────────────────────────────────────────
bool gSavePopupOpen = false;
char gSaveNameBuf[64] = "";
bool gRenamePopupOpen = false;
char gRenameBuf[64] = "";
bool gDeleteConfirmOpen = false;
bool gPresetInputActive = false;

void avatarEditorHandleChar(unsigned int codepoint)
{
    if (gPresetInputActive) {
        size_t len = strlen(gPresetNameBuf);
        if (codepoint == '\b' || codepoint == 127) {
            if (len > 0) gPresetNameBuf[len - 1] = '\0';
        } else if (codepoint >= 32 && codepoint < 127 && len < sizeof(gPresetNameBuf) - 1) {
            gPresetNameBuf[len] = (char)codepoint;
            gPresetNameBuf[len + 1] = '\0';
        }
        return;
    }
    if (gSavePopupOpen) {
        size_t len = strlen(gSaveNameBuf);
        if (codepoint == '\b' || codepoint == 127) {
            if (len > 0) gSaveNameBuf[len - 1] = '\0';
        } else if (codepoint >= 32 && codepoint < 127 && len < sizeof(gSaveNameBuf) - 1) {
            gSaveNameBuf[len] = (char)codepoint;
            gSaveNameBuf[len + 1] = '\0';
        }
        return;
    }
    if (gRenamePopupOpen) {
        size_t len = strlen(gRenameBuf);
        if (codepoint == '\b' || codepoint == 127) {
            if (len > 0) gRenameBuf[len - 1] = '\0';
        } else if (codepoint >= 32 && codepoint < 127 && len < sizeof(gRenameBuf) - 1) {
            gRenameBuf[len] = (char)codepoint;
            gRenameBuf[len + 1] = '\0';
        }
        return;
    }
}

void avatarEditorHandleKey(int key, int action)
{
    (void)key;
    (void)action;
}

void avatarEditorHandleDrop(int count, const char** paths)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;
    int imported = 0;
    for (int i = 0; i < count; ++i) {
        std::string ext = std::filesystem::path(paths[i]).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" && av.importPng(paths[i]))
            ++imported;
    }
    if (imported > 0) {
        Terminal::instance().addLog("[AVATAR] Imported " + std::to_string(imported) + " PNG(s)");
        av.triggerSave();
    }
}

// ── Tabs ─────────────────────────────────────────────────────────────
static void drawEditorTabs(GLFWwindow* win)
{
    const GuiLayout& layout = avatarEditorLayout();
    const char* tabIds[] = {"tabFaces", "tabColors", "tabCosmetics", "tabPresets"};
    for (int ti = 0; ti < 4; ++ti) {
        const GuiElement* te = layout.get(tabIds[ti]);
        if (!te || !te->visible)
            continue;
        glm::vec4 bg = (ti == gEditorTab)
            ? (te->hasPressedColor() ? te->getPressedColorVec() : glm::vec4{0.2f, 0.45f, 0.3f, 1.0f})
            : te->getBackgroundColorVec();
        if (uiButton(win, te->text.c_str(), {te->x, te->y, te->w, te->h}, bg, te->id.c_str(),
                     nullptr, nullptr, nullptr, nullptr,
                     editorFontSize(te, avatarEditorTabFontSize)).clicked)
            gEditorTab = ti;
    }
}

static void drawEditorContent(GLFWwindow* win)
{
    const GuiLayout& layout = avatarEditorLayout();

    const GuiElement* panel = layout.get("panelEditor");
    float px = panel ? panel->x + 8.0f : 328.0f;
    float py = panel ? panel->y + 130.0f : 180.0f;
    float pw = panel ? panel->w - 16.0f : 604.0f;
    float ph = panel ? panel->h - 140.0f : 760.0f;

    switch (gEditorTab) {
        case 0: drawAvatarFacesTab(win, px, py, pw, ph); break;
        case 1: drawAvatarColorsTab(win, px, py, pw, ph); break;
        case 2: drawAvatarCosmeticsTab(win, px, py, pw, ph); break;
        case 3: drawAvatarPresetsTab(win, px, py, pw, ph); break;
    }
}

static void drawBottomBar(GLFWwindow* win, AvatarEditorResult& r)
{
    AvatarSystem& av = AvatarSystem::instance();
    const GuiLayout& layout = avatarEditorLayout();

    // Save status text
    const GuiElement* status = layout.get("saveStatus");
    if (status) {
        const auto& as = av.autosave();
        std::string text;
        switch (as.status()) {
            case AvatarAutosave::Status::Saving: text = "Saving..."; break;
            case AvatarAutosave::Status::Ok: {
                double secs = as.secondsSinceLastSave();
                if (secs < 0) text = "Not saved yet";
                else if (secs < 5) text = "Autosaved just now";
                else text = "Autosaved " + std::to_string((int)secs) + "s ago";
                break;
            }
            case AvatarAutosave::Status::Failed: text = "Could not save - retrying"; break;
            case AvatarAutosave::Status::Recovered: text = as.statusMessage(); break;
            default: text = "";
        }
        if (!text.empty())
            uiDrawText(text.c_str(), uiScaleX(status->x), uiScaleY(status->y),
                       avatarEditorFont(avatarEditorHintFontSize), status->getTextColorVec());
    }

    const char* btnIds[] = {"saveButton", "applyButton", "backButton"};
    for (int i = 0; i < 3; ++i) {
        const GuiElement* elem = layout.get(btnIds[i]);
        if (!elem || !elem->visible)
            continue;
        if (!uiButton(win, elem->text.c_str(), {elem->x, elem->y, elem->w, elem->h},
                      elem->getBackgroundColorVec(), elem->id.c_str(),
                      nullptr, nullptr, nullptr, nullptr,
                      editorFontSize(elem, avatarEditorButtonFontSize)).clicked)
            continue;
        if (elem->id == "saveButton") {
            if (av.hasAvatar()) {
                gSavePopupOpen = true;
                memset(gSaveNameBuf, 0, sizeof(gSaveNameBuf));
                strncpy(gSaveNameBuf, av.currentName().c_str(), sizeof(gSaveNameBuf) - 1);
                r.savePopupOpen = true;
            }
        } else if (elem->id == "applyButton") {
            r.goApply = true;
        } else if (elem->id == "backButton") {
            r.goBack = true;
            gSelectedTexture.clear();
        }
    }
}

AvatarEditorResult drawAvatarEditor(GLFWwindow* win)
{
    AvatarEditorResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    const GuiLayout& layout = avatarEditorLayout();

    // The preset-name text field only captures input while its tab is shown.
    if (gEditorTab != 3)
        gPresetInputActive = false;

    // Draw static panels / titles / hints from the JSON layout.
    for (const std::string& id : layout.elementIds()) {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible)
            continue;
        const std::string& type = elem->type;
        if (type != "panel" && type != "text" && type != "label")
            continue;
        // Popups are drawn only when open (see drawAvatarEditorPopups).
        if (id.rfind("savePopup", 0) == 0 || id.rfind("renamePopup", 0) == 0 ||
            id.rfind("deletePopup", 0) == 0)
            continue;
        // The 3D preview panel is rendered by gui-main before us; painting a
        // solid background over it would hide the player. Draw a border only.
        if (id == "panelPreview")
            continue;
        drawGuiElement(win, *elem);
    }

    // Frame for the live 3D preview.
    {
        const GuiElement* prev = layout.get("panelPreview");
        if (prev && prev->visible) {
            UIRect s = {uiScaleX(prev->x), uiScaleY(prev->y),
                        uiScaleX(prev->w), uiScaleY(prev->h)};
            uiDrawRectOutline(s, {0.15f, 0.2f, 0.3f, 0.7f}, "preview-border");
        }
    }

    drawEditorTabs(win);

    // Panels
    {
        const GuiElement* lib = layout.get("panelLibrary");
        const GuiElement* out = layout.get("panelOutfits");
        drawAvatarLibrary(win,
            lib ? lib->x + 8.0f : 28.0f,
            lib ? lib->y + 130.0f : 180.0f,
            lib ? lib->w - 16.0f : 264.0f,
            lib ? lib->h - 140.0f : 760.0f);
        drawAvatarOutfitsPanel(win,
            out ? out->x + 8.0f : 1588.0f,
            out ? out->y + 120.0f : 170.0f,
            out ? out->w - 16.0f : 304.0f,
            out ? out->h - 130.0f : 770.0f);
    }

    if (!av.hasAvatar()) {
        uiDrawText("No outfit loaded. Create or pick one from the right panel.",
                   uiScaleX(600.0f), uiScaleY(480.0f),
                   avatarEditorFont(avatarEditorHintFontSize), {0.8f, 0.85f, 0.9f, 1.0f});
    } else {
        drawEditorContent(win);
    }
    drawBottomBar(win, r);

    // Popups + open dropdown overlay (must be on top of everything).
    drawAvatarEditorPopups(win, r);
    drawAvatarCosmeticsOverlay(win);

    // Apply any finished async atlas rebuild to the live preview player.
    Player* previewPlayer = avatarEditorPreviewPlayer();
    if (previewPlayer)
        av.finalizeAtlasIfReady(*previewPlayer);

    // Drag & drop hover banner.
    if (isDropHoverActive()) {
        const std::string& hoverPath = getDropHoverPath();
        float sw = uiScreenW();
        UIRect banner = {0, 0, sw, 36.0f};
        uiDrawRect(banner, {0.1f, 0.5f, 0.25f, 0.9f}, "drop-banner");
        uiDrawRectOutline(banner, {0.2f, 0.8f, 0.4f, 1.0f}, "drop-banner-border");
        std::string display = "Drop PNG: " + hoverPath;
        if (display.size() > 100)
            display = "Drop PNG: ..." + display.substr(display.size() - 90);
        float tw = uiMeasureText(display.c_str(), 0.32f);
        uiDrawText(display.c_str(), sw * 0.5f - tw * 0.5f, 6.0f, 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});
    }

    return r;
}
