// 08 15 2026, 15 30
/* purpose
* Colors tab: per-part RGB tint sliders.
* Writes PartColors through AvatarSystem and refreshes preview.
* DOES NOT own avatar data or the atlas builder.
*/
// Colors tab: per-body-part RGB tint sliders with live preview.
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar.h"
#include "avatar-editor-scroll.h"

void drawAvatarColorsTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar())
        return;

    float contentH = kPartCount * 110.0f;
    ScrollState ss;
    beginScroll(win, {px, py, pw, ph}, contentH, ss);

    float y = py;
    const float sliderW = pw - 8.0f;

    for (int pi = 0; pi < kPartCount; ++pi) {
        glm::vec3 color = av.partColor(partKey(pi));

        uiDrawRect({uiScaleX(px), uiScaleY(y), uiScaleX(28), uiScaleY(28)},
                   {color.r, color.g, color.b, 1.0f}, "part-swatch");
        uiDrawText(partLabel(pi), uiScaleX(px + 36.0f), uiScaleY(y),
                   avatarEditorFont(avatarEditorSectionFontSize), {0.8f, 0.85f, 0.95f, 1.0f});
        y += 34.0f;

        bool changed = false;
        float r = color.r, g = color.g, b = color.b;
        changed |= drawEditorSlider(win, "Red", px, y, sliderW, r, 0.0f, 2.0f); y += 28.0f;
        changed |= drawEditorSlider(win, "Green", px, y, sliderW, g, 0.0f, 2.0f); y += 28.0f;
        changed |= drawEditorSlider(win, "Blue", px, y, sliderW, b, 0.0f, 2.0f); y += 28.0f;

        if (changed) {
            av.setPartColor(partKey(pi), glm::vec3(r, g, b));
            avatarEditorRefreshPreview();
            av.triggerSave();
        }
        y += 20.0f;
    }

    endScroll({px, py, pw, ph}, contentH, ss);
}
