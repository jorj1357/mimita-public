// 08 15 2026, 15 30
/* purpose
* Presets tab: save the current look and load it back.
* Writes presets through AvatarSystem.
* DOES NOT own preset storage or avatar data.
*/
// Presets tab: save the current look under a name and load it back.
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar.h"
#include "avatar-editor-scroll.h"

#include <cstring>
#include <string>

char gPresetNameBuf[64] = "";

void drawAvatarPresetsTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;

    float contentH = 420.0f;
    ScrollState ss;
    beginScroll(win, {px, py, pw, ph}, contentH, ss);

    float y = py;

    uiDrawText("Save current look as:", uiScaleX(px + 4.0f), uiScaleY(y),
               avatarEditorFont(avatarEditorHintFontSize), {0.5f, 0.6f, 0.7f, 1.0f});
    y += 28.0f;

    // Name input (simple char buffer; text entered via avatarEditorHandleChar
    // is routed to this buffer when the field is focused).
    UIRect inputRect = {uiScaleX(px + 4.0f), uiScaleY(y), uiScaleX(220.0f), uiScaleY(28.0f)};
    uiDrawRect(inputRect, {0.08f, 0.09f, 0.13f, 1.0f}, "preset-input");
    uiDrawText(gPresetNameBuf[0] ? gPresetNameBuf : "preset name...",
               inputRect.x + 6.0f, inputRect.y + 4.0f,
               avatarEditorFont(avatarEditorButtonFontSize),
               gPresetNameBuf[0] ? glm::vec4{1,1,1,1} : glm::vec4{0.4f,0.4f,0.5f,1.0f});

    extern bool gPresetInputActive;
    if (uiButton(win, "", {px + 4.0f, y, 220.0f, 28.0f}, {0,0,0,0}).clicked)
        gPresetInputActive = true;
    if (editorButton(win, "SAVE", {px + 232.0f, y, 90.0f, 28.0f}, {0.2f, 0.5f, 0.3f, 1.0f},
                     avatarEditorFont(avatarEditorButtonFontSize)).clicked) {
        if (gPresetNameBuf[0]) {
            av.savePreset(gPresetNameBuf);
            memset(gPresetNameBuf, 0, sizeof(gPresetNameBuf));
            gPresetInputActive = false;
        }
    }
    y += 40.0f;

    auto presets = av.listPresets();
    for (auto& p : presets) {
        bool active = (p == av.current().activePreset);
        glm::vec4 col = active ? glm::vec4{0.2f, 0.48f, 0.28f, 1.0f}
                               : glm::vec4{0.08f, 0.09f, 0.14f, 1.0f};
        UIRect row = {px + 4.0f, y, pw - 8.0f, 28.0f};
        UIRect screenRow = {uiScaleX(px + 4.0f), uiScaleY(y), uiScaleX(pw - 8.0f), uiScaleY(28.0f)};
        uiDrawRect(screenRow, col, "preset-entry");
        if (active)
            uiDrawRectOutline(screenRow, {0.3f, 0.8f, 0.5f, 1.0f}, "preset-active");
        uiDrawText(p.c_str(), screenRow.x + 8.0f, screenRow.y + 4.0f,
                   avatarEditorFont(avatarEditorOutfitListFontSize), {1,1,1,1});
        if (uiButton(win, "", row, col).clicked) {
            av.loadPreset(p);
            avatarEditorRefreshPreview();
            avatarEditorApplyCosmeticsToPlayer();
        }
        y += 32.0f;
    }

    if (presets.empty())
        uiDrawText("No presets saved yet.", uiScaleX(px + 4.0f), uiScaleY(y),
                   avatarEditorFont(avatarEditorHintFontSize), {0.4f, 0.5f, 0.6f, 1.0f});

    endScroll({px, py, pw, ph}, contentH, ss);
}
