// 08 15 2026, 15 30
/* purpose
* Save/rename/delete confirm popups for the avatar editor.
* Geometry comes from the JSON layout; text via char buffers.
* DOES NOT own input routing (avatarEditorHandleChar does).
*/
// Save / rename / delete popups for the avatar editor. Geometry comes from
// the JSON layout; the text fields use simple char buffers fed through
// avatarEditorHandleChar.
#include "avatar-editor.h"
#include "avatar-editor-helpers.h"
#include "avatar.h"

#include <cstring>
#include <string>

#include "gui/gui-coord.h"
#include "gui/gui-element-render.h"

namespace {

void drawInputText(const GuiLayout& layout, const char* elemId, const char* buf,
                   const char* placeholder)
{
    const GuiElement* el = layout.get(elemId);
    if (!el)
        return;
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect s = cs.designToScreen({el->x, el->y, el->w, el->h});
    uiDrawRect(s, el->getBackgroundColorVec(), elemId);
    if (el->hasOutlineColor())
        uiDrawRectOutline(s, el->getOutlineColorVec(), elemId);
    uiDrawText(buf[0] ? buf : placeholder, s.x + 6.0f, s.y + 4.0f,
               avatarEditorFont(avatarEditorButtonFontSize),
               buf[0] ? glm::vec4{1,1,1,1} : glm::vec4{0.4f,0.4f,0.5f,1.0f});
}

} // anonymous namespace

void drawAvatarEditorPopups(GLFWwindow* win, AvatarEditorResult& r)
{
    AvatarSystem& av = AvatarSystem::instance();
    const GuiLayout& layout = avatarEditorLayout();
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    auto drawBgAndTitle = [&](const char* bgId, const char* titleId) {
        const GuiElement* bg = layout.get(bgId);
        if (bg && bg->visible) {
            UIRect s = cs.designToScreen({bg->x, bg->y, bg->w, bg->h});
            uiDrawRect(s, bg->getBackgroundColorVec(), bgId);
            uiDrawRectOutline(s, bg->getOutlineColorVec(), bgId);
        }
        const GuiElement* title = layout.get(titleId);
        if (title && title->visible)
            drawGuiElement(win, *title);
    };

    // ── Save popup ────────────────────────────────────────────────
    if (gSavePopupOpen) {
        drawBgAndTitle("savePopupBg", "savePopupTitle");
        const GuiElement* lbl = layout.get("savePopupLabel");
        if (lbl && lbl->visible)
            drawGuiElement(win, *lbl);
        drawInputText(layout, "savePopupInput", gSaveNameBuf, "name...");

        const GuiElement* sv = layout.get("savePopupSaveBtn");
        if (sv && sv->visible && editorButton(win, sv->text.c_str(),
            {sv->x, sv->y, sv->w, sv->h}, sv->getBackgroundColorVec(),
            editorFontSize(sv, avatarEditorButtonFontSize)).clicked) {
            std::string name(gSaveNameBuf);
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);
            if (!name.empty()) {
                av.saveCurrentOutfit(name);
                gSavePopupOpen = false;
                r.goSave = true;
            }
        }
        const GuiElement* cn = layout.get("savePopupCancelBtn");
        if (cn && cn->visible && editorButton(win, cn->text.c_str(),
            {cn->x, cn->y, cn->w, cn->h}, cn->getBackgroundColorVec(),
            editorFontSize(cn, avatarEditorButtonFontSize)).clicked)
            gSavePopupOpen = false;
    }

    // ── Rename popup ──────────────────────────────────────────────
    if (gRenamePopupOpen) {
        drawBgAndTitle("renamePopupBg", "renamePopupTitle");
        drawInputText(layout, "renamePopupInput", gRenameBuf, "name...");

        const GuiElement* cf = layout.get("renamePopupConfirmBtn");
        if (cf && cf->visible && editorButton(win, cf->text.c_str(),
            {cf->x, cf->y, cf->w, cf->h}, cf->getBackgroundColorVec(),
            editorFontSize(cf, avatarEditorButtonFontSize)).clicked) {
            std::string newName(gRenameBuf);
            newName.erase(0, newName.find_first_not_of(" \t\r\n"));
            newName.erase(newName.find_last_not_of(" \t\r\n") + 1);
            if (!newName.empty()) {
                std::string oldName = av.currentName();
                if (av.renameOutfit(oldName, newName)) {
                    avatarEditorLoadOutfit(newName);
                }
                gRenamePopupOpen = false;
            }
        }
        const GuiElement* cn2 = layout.get("renamePopupCancelBtn");
        if (cn2 && cn2->visible && editorButton(win, cn2->text.c_str(),
            {cn2->x, cn2->y, cn2->w, cn2->h}, cn2->getBackgroundColorVec(),
            editorFontSize(cn2, avatarEditorButtonFontSize)).clicked)
            gRenamePopupOpen = false;
    }

    // ── Delete confirm ────────────────────────────────────────────
    if (gDeleteConfirmOpen) {
        drawBgAndTitle("deletePopupBg", "deletePopupTitle");

        const GuiElement* bg = layout.get("deletePopupBg");
        if (bg) {
            uiDrawText((std::string("\"") + av.currentName() + "\"").c_str(),
                       cs.designToScreenX(bg->x + 20.0f), cs.designToScreenY(bg->y + 54.0f),
                       avatarEditorFont(avatarEditorButtonFontSize), {0.6f, 0.7f, 0.8f, 1.0f});
        }

        const GuiElement* yes = layout.get("deletePopupYesBtn");
        if (yes && yes->visible && editorButton(win, yes->text.c_str(),
            {yes->x, yes->y, yes->w, yes->h}, yes->getBackgroundColorVec(),
            editorFontSize(yes, avatarEditorButtonFontSize)).clicked) {
            std::string deleted = av.currentName();
            av.deleteOutfit(deleted);
            auto remaining = av.listAvatars();
            if (!remaining.empty())
                avatarEditorLoadOutfit(remaining[0]);
            gDeleteConfirmOpen = false;
        }
        const GuiElement* no = layout.get("deletePopupNoBtn");
        if (no && no->visible && editorButton(win, no->text.c_str(),
            {no->x, no->y, no->w, no->h}, no->getBackgroundColorVec(),
            editorFontSize(no, avatarEditorButtonFontSize)).clicked)
            gDeleteConfirmOpen = false;
    }
}
