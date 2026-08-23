// 08 20 2026, 12 00
/* purpose
* Orchestrates the five-tab avatar editor and the right-side preview frame.
* Owns tab scrolling, SaveLoad controls, avatar-name input, and tab overlays.
* Dispatches PNG, part, image, and cosmetic editing to focused owner files.
* DOES NOT own avatar serialization, atlas baking, GLB rendering, or GUI layout data.
*/
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-library.h"
#include "avatar-editor-parts.h"
#include "avatar-editor-image.h"
#include "avatar-editor-cosmetics.h"
#include "avatar-editor-popups.h"
#include "avatar-drop-target.h"
#include "avatar.h"
#include "avatar-editor-dropdown.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "gui/gui-element-render.h"
#include "gui/gui-coord.h"
#include "gui/ui-system-internal.h"
#include "gui/ui-text-input.h"
#include "devtools/terminal.h"

bool gSavePopupOpen = false;
char gSaveNameBuf[64] = "";
bool gRenamePopupOpen = false;
char gRenameBuf[64] = "";
bool gDeleteConfirmOpen = false;
bool gPresetInputActive = false;
UITextInputState gSaveNameState;

namespace {

constexpr const char* kTabIds[] = {
    "tabPngLibrary", "tabPartPicker", "tabImageEditor", "tabCosmetics", "tabSaveLoad"
};
constexpr int kTabCount = 5;

float gTabScrollX = 0.0f;
DropdownState gAvatarLoadDropdown;
std::vector<std::string> gAvatarLoadItems;
UIRect gAvatarLoadRect{};
DropdownState gPlayerModelDropdown;
std::vector<std::string> gPlayerModelItems;
UIRect gPlayerModelRect{};

void appendAscii(char* buffer, size_t capacity, unsigned int codepoint)
{
    size_t len = std::strlen(buffer);
    if (codepoint == '\b' || codepoint == 127) {
        if (len > 0) buffer[len - 1] = '\0';
    } else if (codepoint >= 32 && codepoint < 127 && len + 1 < capacity) {
        buffer[len] = (char)codepoint;
        buffer[len + 1] = '\0';
    }
}

void trimName(std::string& name)
{
    const size_t first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) { name.clear(); return; }
    const size_t last = name.find_last_not_of(" \t\r\n");
    name = name.substr(first, last - first + 1);
}

std::vector<std::string> scanPlayerModels()
{
    std::vector<std::string> result{"Default body model"};
    const std::filesystem::path root("assets/entity/player");
    std::error_code ec;
    if (!std::filesystem::exists(root, ec))
        return result;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".glb")
            result.push_back(entry.path().generic_string());
    }
    std::sort(result.begin() + 1, result.end());
    return result;
}

bool cursorInDesignRect(GLFWwindow* win, UIRect designRect)
{
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double sx = mx, sy = my;
    GuiCoordinateSystem::instance().cursorWindowToScreen(mx, my, sx, sy);
    return pointIn(sx, sy, GuiCoordinateSystem::instance().designToScreen(designRect));
}

void drawEditorTabs(GLFWwindow* win)
{
    const GuiLayout& layout = avatarEditorLayout();
    const GuiElement* strip = layout.get("tabStrip");
    if (!strip) return;

    const UIRect clip = {strip->x, strip->y, strip->w, strip->h};
    if (cursorInDesignRect(win, clip) && UISys::gScrollYOffset != 0.0) {
        gTabScrollX += (float)(UISys::gScrollYOffset * 100.0);
        UISys::gScrollYOffset = 0.0;
    }

    float contentWidth = 0.0f;
    for (int i = 0; i < kTabCount; ++i) {
        const GuiElement* tab = layout.get(kTabIds[i]);
        if (tab) contentWidth += tab->w + 6.0f;
    }
    gTabScrollX = std::clamp(gTabScrollX, 0.0f,
                             std::max(0.0f, contentWidth - strip->w));

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect screenClip = cs.designToScreen(clip);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)screenClip.x, (int)(UISys::gFbH - screenClip.y - screenClip.h),
              (int)screenClip.w, (int)screenClip.h);

    float x = strip->x - gTabScrollX;
    for (int ti = 0; ti < kTabCount; ++ti) {
        const GuiElement* tab = layout.get(kTabIds[ti]);
        if (!tab || !tab->visible) continue;
        glm::vec4 bg = (ti == gEditorTab)
            ? (tab->hasPressedColor() ? tab->getPressedColorVec() : glm::vec4{0.2f,0.45f,0.3f,1.0f})
            : tab->getBackgroundColorVec();
        if (editorButton(win, tab->text.c_str(), {x, tab->y, tab->w, tab->h}, bg,
                         editorFontSize(tab, avatarEditorTabFontSize)).clicked)
            gEditorTab = ti;
        x += tab->w + 6.0f;
    }
    glDisable(GL_SCISSOR_TEST);
}

void drawSaveLoadInput(GLFWwindow* win, const GuiLayout& layout)
{
    const GuiElement* input = layout.get("saveLoadNameInput");
    if (!input) return;
    UIRect rect = {input->x, input->y, input->w, input->h};
    UITextInputOptions opts;
    opts.maxLength = 63;
    opts.selectAllOnFocus = true;
    uiTextInputRender(win, "saveLoadNameInput", rect, gSaveNameState, opts);
}

void drawSaveLoadTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    const GuiLayout& layout = avatarEditorLayout();

    const std::string saveLoadTitle = editorLabelText("saveLoadTitle", "SAVELOAD");
    uiDrawText(saveLoadTitle.c_str(), uiScaleX(px), uiScaleY(py),
               editorLabelFontSize("saveLoadTitle", avatarEditorSectionFontSize), {0.4f,0.75f,0.55f,1});
    const std::string currentLabel = editorLabelText("saveLoadCurrentAvatarLabel", "Current avatar:");
    uiDrawText(currentLabel.c_str(), uiScaleX(px), uiScaleY(py + 34),
               editorLabelFontSize("saveLoadCurrentAvatarLabel", avatarEditorHintFontSize), {0.6f,0.7f,0.8f,1});
    uiDrawText(av.hasAvatar() ? av.currentName().c_str() : "none",
               uiScaleX(px + 130), uiScaleY(py + 34),
               avatarEditorFont(avatarEditorHintFontSize), {0.8f,0.9f,0.7f,1});

    const float buttonW = std::max(160.0f, pw * 0.46f);
    const GuiElement* saveCurrent = layout.get("saveCurrentButton");
    if (editorButton(win, editorLabelText("saveCurrentButton", "Save this avatar").c_str(),
                     {px, py + 62, buttonW, 38}, layoutBg(saveCurrent, {0.18f,0.5f,0.28f,1}),
                     editorFontSize(saveCurrent, avatarEditorButtonFontSize)).clicked && av.hasAvatar())
        av.saveProject();

    const std::string saveAsLabel = editorLabelText("saveLoadSaveAsLabel", "Save as");
    uiDrawText(saveAsLabel.c_str(), uiScaleX(px), uiScaleY(py + 116),
               editorLabelFontSize("saveLoadSaveAsLabel", avatarEditorSectionFontSize), {0.4f,0.75f,0.55f,1});
    drawSaveLoadInput(win, layout);
    const GuiElement* saveAs = layout.get("saveAsButton");
    if (editorButton(win, editorLabelText("saveAsButton", "Save as new avatar").c_str(),
                     {px, py + 164, buttonW, 38}, layoutBg(saveAs, {0.18f,0.38f,0.5f,1}),
                     editorFontSize(saveAs, avatarEditorButtonFontSize)).clicked && av.hasAvatar()) {
        std::string name(gSaveNameState.value);
        trimName(name);
        if (!name.empty() && av.saveCurrentOutfit(name)) {
            avatarEditorLoadOutfit(name);
            gSaveNameState.value.clear();
            gSaveNameState.cursorPos = 0;
            gSaveNameState.selectionStart = -1;
        }
    }

    const std::string loadLabel = editorLabelText("saveLoadLoadLabel", "Load avatar");
    uiDrawText(loadLabel.c_str(), uiScaleX(px), uiScaleY(py + 222),
               editorLabelFontSize("saveLoadLoadLabel", avatarEditorSectionFontSize), {0.4f,0.75f,0.55f,1});
    gAvatarLoadItems = av.listAvatars();
    if (!gAvatarLoadItems.empty()) {
        int current = 0;
        for (int i = 0; i < (int)gAvatarLoadItems.size(); ++i)
            if (gAvatarLoadItems[i] == av.currentName()) current = i;
        if (gAvatarLoadDropdown.selectedIndex < 0 ||
            gAvatarLoadDropdown.selectedIndex >= (int)gAvatarLoadItems.size())
            gAvatarLoadDropdown.selectedIndex = current;
        gAvatarLoadRect = {px, py + 254, pw, 32};
        drawDropdown(win, gAvatarLoadDropdown, gAvatarLoadRect.x, gAvatarLoadRect.y,
                     gAvatarLoadRect.w, gAvatarLoadRect.h, "", gAvatarLoadItems);
        const GuiElement* loadSelected = layout.get("loadSelectedButton");
        if (editorButton(win, editorLabelText("loadSelectedButton", "Load selected avatar").c_str(),
                         {px, py + 302, buttonW, 38}, layoutBg(loadSelected, {0.25f,0.3f,0.5f,1}),
                         editorFontSize(loadSelected, avatarEditorButtonFontSize)).clicked) {
            const int index = gAvatarLoadDropdown.selectedIndex;
            if (index >= 0 && index < (int)gAvatarLoadItems.size())
                avatarEditorLoadOutfit(gAvatarLoadItems[index]);
        }
        const std::string undoLabel = editorLabelText("undoButton", "Undo");
        if (editorButton(win, undoLabel.c_str(),
                         {px, py + 350, buttonW, 34},
                         {0.35f,0.25f,0.45f,1}, avatarEditorButtonFontSize).clicked)
            av.undoEditorChange();

        uiDrawText(editorLabelText("saveLoadBodyModelLabel", "Player body model").c_str(),
                   uiScaleX(px), uiScaleY(py + 400),
                   editorLabelFontSize("saveLoadBodyModelLabel", avatarEditorSectionFontSize),
                   {0.4f,0.75f,0.55f,1});
        gPlayerModelItems = scanPlayerModels();
        int modelIndex = 0;
        for (int i = 1; i < (int)gPlayerModelItems.size(); ++i)
            if (gPlayerModelItems[i] == av.current().getPlayerModel()) modelIndex = i;
        if (gPlayerModelDropdown.selectedIndex < 0 ||
            gPlayerModelDropdown.selectedIndex >= (int)gPlayerModelItems.size())
            gPlayerModelDropdown.selectedIndex = modelIndex;
        gPlayerModelRect = {px, py + 432, pw, 32};
        drawDropdown(win, gPlayerModelDropdown, gPlayerModelRect.x, gPlayerModelRect.y,
                     gPlayerModelRect.w, gPlayerModelRect.h, "", gPlayerModelItems);
        uiDrawText(editorLabelText("saveLoadBodyModelHint", "Choose a GLB, then apply it to the preview.").c_str(),
                   uiScaleX(px), uiScaleY(py + 470),
                   editorLabelFontSize("saveLoadBodyModelHint", avatarEditorHintFontSize),
                   {0.7f,0.75f,0.8f,1});
        const GuiElement* applyModel = layout.get("applyBodyModelButton");
        if (editorButton(win, editorLabelText("applyBodyModelButton", "Apply body model").c_str(),
                         {px, py + 500, buttonW, 38}, layoutBg(applyModel, {0.25f,0.3f,0.5f,1}),
                         editorFontSize(applyModel, avatarEditorButtonFontSize)).clicked) {
            const int selected = gPlayerModelDropdown.selectedIndex;
            if (selected == 0 || (selected > 0 && selected < (int)gPlayerModelItems.size())) {
                const std::string path = selected == 0 ? "" : gPlayerModelItems[selected];
                if (av.setPlayerModel(path)) {
                    if (Player* previewPlayer = avatarEditorPreviewPlayer())
                        av.applyToPlayer(*previewPlayer, true);
                    avatarEditorApplyCosmeticsToPlayer();
                }
            }
        }
    } else {
        uiDrawText(editorLabelText("saveLoadEmpty", "No valid avatar folders found.").c_str(), uiScaleX(px), uiScaleY(py + 260),
                   editorLabelFontSize("saveLoadEmpty", avatarEditorHintFontSize), {0.8f,0.6f,0.4f,1});
    }
}

void drawEditorContent(GLFWwindow* win)
{
    const GuiLayout& layout = avatarEditorLayout();
    const GuiElement* panel = layout.get("editorContent");
    float px = panel ? panel->x : 34.0f;
    float py = panel ? panel->y : 190.0f;
    float pw = panel ? panel->w : 1020.0f;
    float ph = panel ? panel->h : 790.0f;

    switch (gEditorTab) {
        case 0: drawAvatarLibrary(win, px, py, pw, ph); break;
        case 1: drawAvatarPartPickerTab(win, px, py, pw, ph); break;
        case 2: drawAvatarImageEditorTab(win, px, py, pw, ph); break;
        case 3: drawAvatarCosmeticsTab(win, px, py, pw, ph); break;
        case 4: drawSaveLoadTab(win, px, py, pw, ph); break;
        default: gEditorTab = 0; break;
    }
}

void drawBottomBar(GLFWwindow* win, AvatarEditorResult& r)
{
    const GuiLayout& layout = avatarEditorLayout();
    const GuiElement* status = layout.get("saveStatus");
    AvatarSystem& av = AvatarSystem::instance();
    if (status) {
        std::string text = av.autosave().statusMessage();
        if (!text.empty())
            uiDrawText(text.c_str(), uiScaleX(status->x), uiScaleY(status->y),
                       avatarEditorFont(avatarEditorHintFontSize), status->getTextColorVec());
    }
    const GuiElement* save = layout.get("saveButton");
    if (save && save->visible && editorButton(win, save->text.c_str(),
            {save->x,save->y,save->w,save->h}, save->getBackgroundColorVec(),
            editorFontSize(save, avatarEditorButtonFontSize)).clicked && av.hasAvatar()) {
        av.saveProject();
        r.goSave = true;
    }
    const GuiElement* apply = layout.get("applyButton");
    if (apply && apply->visible && editorButton(win, apply->text.c_str(),
            {apply->x,apply->y,apply->w,apply->h}, apply->getBackgroundColorVec(),
            editorFontSize(apply, avatarEditorButtonFontSize)).clicked)
        r.goApply = true;
    const GuiElement* back = layout.get("backButton");
    if (back && editorButton(win, back->text.c_str(), {back->x,back->y,back->w,back->h},
                             back->getBackgroundColorVec(), editorFontSize(back, avatarEditorButtonFontSize)).clicked) {
        r.goBack = true;
        gSelectedTexture.clear();
    }
}

} // namespace

void avatarEditorHandleChar(unsigned int codepoint)
{
    if (gSaveNameState.focused) {
        uiTextInputHandleChar(gSaveNameState, codepoint, {.maxLength = 63});
        return;
    }
    if (gPresetInputActive)
        appendAscii(gPresetNameBuf, sizeof(gPresetNameBuf), codepoint);
    else if (gSavePopupOpen)
        appendAscii(gSaveNameBuf, sizeof(gSaveNameBuf), codepoint);
    else if (gRenamePopupOpen)
        appendAscii(gRenameBuf, sizeof(gRenameBuf), codepoint);
}

void avatarEditorHandleKey(int key, int action, int mods)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (gSaveNameState.focused)
        uiTextInputHandleKey(glfwGetCurrentContext(), gSaveNameState, key, action, mods,
                             {.maxLength = 63});
}

void avatarEditorHandleDrop(int count, const char** paths)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;
    int imported = 0;
    for (int i = 0; i < count; ++i) {
        std::string ext = std::filesystem::path(paths[i]).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if ((ext == ".png" || ext == ".jpg" || ext == ".jpeg") && av.importPng(paths[i])) ++imported;
    }
    if (imported > 0) {
        Terminal::instance().addLog("[AVATAR] Imported " + std::to_string(imported) + " image(s)");
        av.triggerSave();
    }
}

AvatarEditorResult drawAvatarEditor(GLFWwindow* win)
{
    AvatarEditorResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    const GuiLayout& layout = avatarEditorLayout();

    const char* staticIds[] = {"panelEditor", "editorTitle", "tabStrip", "previewTitle", "bottomBar"};
    for (const char* id : staticIds) {
        const GuiElement* elem = layout.get(id);
        if (elem && elem->visible)
            drawGuiElement(win, *elem);
    }

    const GuiElement* preview = layout.get("panelPreview");
    if (preview) {
        UIRect s = GuiCoordinateSystem::instance().designToScreen({preview->x,preview->y,preview->w,preview->h});
        uiDrawRectOutline(s, preview->getOutlineColorVec(), "avatar-preview-border");
    }

    drawEditorTabs(win);
    if (!av.hasAvatar()) {
        uiDrawText(editorLabelText("noAvatarLoaded", "No avatar loaded.").c_str(), uiScaleX(80), uiScaleY(430),
                   editorLabelFontSize("noAvatarLoaded", avatarEditorHintFontSize), {0.8f,0.6f,0.4f,1});
    } else {
        drawEditorContent(win);
    }
    drawBottomBar(win, r);

    drawAvatarEditorPopups(win, r);
    drawAvatarCosmeticsOverlay(win);
    const int loaded = drawDropdownOverlay(win, gAvatarLoadDropdown,
                                           gAvatarLoadRect.x, gAvatarLoadRect.y,
                                           gAvatarLoadRect.w, gAvatarLoadRect.h,
                                           gAvatarLoadItems);
    if (loaded >= 0 && loaded < (int)gAvatarLoadItems.size()) {
        gAvatarLoadDropdown.selectedIndex = loaded;
        const std::string selectedAvatar = gAvatarLoadItems[loaded];
        if (selectedAvatar != av.currentName())
            avatarEditorLoadOutfit(selectedAvatar);
    }
    drawDropdownOverlay(win, gPlayerModelDropdown,
                        gPlayerModelRect.x, gPlayerModelRect.y,
                        gPlayerModelRect.w, gPlayerModelRect.h,
                        gPlayerModelItems);

    if (Player* previewPlayer = avatarEditorPreviewPlayer())
        av.finalizeAtlasIfReady(*previewPlayer);

    if (isDropHoverActive()) {
        const std::string& hoverPath = getDropHoverPath();
        UIRect banner = {0, 0, uiScreenW(), 36.0f};
        uiDrawRect(banner, {0.1f,0.5f,0.25f,0.9f}, "drop-banner");
        uiDrawText((std::string("Drop PNG: ") + hoverPath).c_str(), 12.0f, 6.0f,
                   0.32f, {1,1,1,1});
    }
    return r;
}
