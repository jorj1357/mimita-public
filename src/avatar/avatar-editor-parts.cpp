// 08 20 2026, 12 00
/* purpose
* Implements the avatar editor's multi-face part picker.
* Renders all six body parts and their six selectable faces.
* Shares selection state with the image and cosmetics editors.
* DOES NOT own avatar JSON or render the 3D preview.
*/
#include "avatar-editor-parts.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-scroll.h"
#include "avatar.h"

#include <algorithm>
#include <cstdio>

namespace {

ScrollState gPartPickerScroll;

bool partFullySelected(int part)
{
    for (int face = 0; face < kFaceCount; ++face)
        if (!gSelectedFaces[part][face]) return false;
    return true;
}

void drawPartCheck(GLFWwindow* win, int part, float x, float y, float w)
{
    bool checked = partFullySelected(part);
    const char* ids[] = {"partHead", "partTorso", "partLeftArm", "partRightArm", "partLeftLeg", "partRightLeg"};
    const std::string label = editorLabelText(ids[part], partLabel(part));
    if (uiCheckbox(win, label.c_str(), {x, y, w, 32.0f}, &checked))
        setSelectedPartFaces(part, checked);
}

void drawFaceCheck(GLFWwindow* win, int part, int face, float x, float y, float w)
{
    const bool selected = gSelectedFaces[part][face];
    const glm::vec4 color = selected
        ? glm::vec4{0.18f, 0.55f, 0.28f, 1.0f}
        : glm::vec4{0.48f, 0.16f, 0.16f, 1.0f};
    const char* ids[] = {"faceFront", "faceBack", "faceLeft", "faceRight", "faceTop", "faceBottom"};
    const GuiElement* element = avatarEditorLayout().get(ids[face]);
    if (editorButton(win, editorLabelText(ids[face], faceLabel(face)).c_str(), {x, y, w, 32.0f}, color,
                     editorFontSize(element, avatarEditorSmallTabFontSize)).clicked) {
        gSelectedFaces[part][face] = !selected;
        gEditingCosmeticImage = false;
        gSelectedPart = part;
        gSelectedFace = face;
    }
}

} // namespace

void drawAvatarPartPickerTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& av = AvatarSystem::instance();
    if (!av.hasAvatar()) return;
    ensureAvatarEditorSelection();

    const float contentH = 720.0f;
    beginScroll(win, {px, py, pw, ph}, contentH, gPartPickerScroll);

    float y = py;
    const std::string title = editorLabelText("partPickerTitle", "PART PICKER");
    uiDrawText(title.c_str(), uiScaleX(px), uiScaleY(y),
               editorLabelFontSize("partPickerTitle", avatarEditorSectionFontSize),
               {0.4f, 0.75f, 0.55f, 1.0f});
    y += 32.0f;

    bool allSelected = selectedFaceCount() == kPartCount * kFaceCount;
    const std::string selectAll = editorLabelText("partPickerSelectAll", "Select all");
    if (uiCheckbox(win, selectAll.c_str(), {px, y, pw * 0.35f, 32.0f}, &allSelected))
        setAllSelectedFaces(allSelected);
    char summary[64];
    snprintf(summary, sizeof(summary), "%d of %d faces selected",
             selectedFaceCount(), kPartCount * kFaceCount);
    uiDrawText(summary, uiScaleX(px + pw * 0.42f), uiScaleY(y + 9.0f),
               avatarEditorFont(avatarEditorHintFontSize),
               {0.6f, 0.7f, 0.8f, 1.0f});
    y += 40.0f;

    for (int part = 0; part < kPartCount; ++part) {
        drawPartCheck(win, part, px, y, pw * 0.42f);
        y += 30.0f;
        const float faceW = std::max(80.0f, (pw - 18.0f) / 3.0f);
        for (int face = 0; face < kFaceCount; ++face) {
            const int col = face % 3;
            const int row = face / 3;
            drawFaceCheck(win, part, face,
                          px + 18.0f + col * (faceW + 6.0f),
                          y + row * 28.0f, faceW);
        }
        y += 84.0f;
    }

    endScroll({px, py, pw, ph}, contentH, gPartPickerScroll);
}
