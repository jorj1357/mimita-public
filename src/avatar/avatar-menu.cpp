#include "avatar-menu.h"
#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "../gui/ui-system.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-coord.h"
#include "../devtools/terminal.h"
#include "../config/player-settings.h"

namespace {

struct AvatarInput {
    bool active = false;
    char buffer[128]{};
    int cursor = 0;
    std::string label;
    std::string targetPart;
    std::string targetFace;
};
AvatarInput gInput;

int gOpenPickerSlot = -1;
int gPickerScroll = 0;
std::vector<std::string> gPickerFiles;
int gAdvScroll = 0;

const char* kSimpleLabels[] = {"Face Image", "Shirt Image", "Pants Image", "Skin Image"};
const char* kSimpleKeys[]  = {"face", "shirt", "pants", "skin"};
const char* kPartLabels[]  = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"};
const char* kPartKeys[]    = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
const char* kFaceLabels[]  = {"Front", "Back", "Left", "Right", "Top", "Bottom"};
const char* kFaceKeys[]    = {"front", "back", "left", "right", "top", "bottom"};

void startInput(const std::string& label, const std::string& part,
                const std::string& face, const std::string& initial) {
    gInput.active = true;
    gInput.label = label;
    gInput.targetPart = part;
    gInput.targetFace = face;
    gInput.cursor = (int)initial.size();
    std::memset(gInput.buffer, 0, sizeof(gInput.buffer));
    std::strncpy(gInput.buffer, initial.c_str(), sizeof(gInput.buffer) - 1);
}

void stopInput() { gInput.active = false; gInput.label.clear(); }

void openPicker(int slot, const std::vector<std::string>& files) {
    gOpenPickerSlot = slot;
    gPickerFiles = files;
    gPickerScroll = 0;
}

void closePicker() { gOpenPickerSlot = -1; gPickerFiles.clear(); }

}

void avatarMenuHandleChar(unsigned int codepoint) {
    if (!gInput.active) return;
    if (codepoint >= 32 && codepoint <= 126 && gInput.cursor < (int)sizeof(gInput.buffer) - 1) {
        gInput.buffer[gInput.cursor++] = (char)codepoint;
        gInput.buffer[gInput.cursor] = '\0';
    }
}

void avatarMenuHandleKey(int key, int action) {
    if (!gInput.active || (action != GLFW_PRESS && action != GLFW_REPEAT)) return;
    if (key == GLFW_KEY_BACKSPACE && gInput.cursor > 0) {
        gInput.buffer[--gInput.cursor] = '\0';
    } else if (key == GLFW_KEY_ENTER && gInput.cursor > 0) {
        AvatarSystem& av = AvatarSystem::instance();
        if (!gInput.targetFace.empty()) {
            av.setPartFace(gInput.targetPart, gInput.targetFace, std::string(gInput.buffer));
        } else if (!gInput.targetPart.empty()) {
            SimpleAvatar s = av.current().simple;
            if (gInput.targetPart == "face") s.face = gInput.buffer;
            else if (gInput.targetPart == "shirt") s.shirt = gInput.buffer;
            else if (gInput.targetPart == "pants") s.pants = gInput.buffer;
            else if (gInput.targetPart == "skin") s.skin = gInput.buffer;
            av.setSimple(s);
        }
        stopInput();
    } else if (key == GLFW_KEY_ESCAPE) {
        stopInput();
    }
}

// ─── draw the file picker overlay ───────────────────────────────────

static void drawPicker(GLFWwindow* win, GuiLayout& layout) {
    if (gOpenPickerSlot < 0 || gPickerFiles.empty()) return;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    const GuiElement* panelEl = layout.get("pickerPanel");
    float pdx = panelEl ? panelEl->x : 680.0f;
    float pdy = panelEl ? panelEl->y : 330.0f;
    float pdw = panelEl ? panelEl->w : 560.0f;
    float pdh = panelEl ? panelEl->h : 420.0f;

    UIRect bg = cs.designToScreen({pdx, pdy, pdw, pdh});
    uiDrawRect(bg, {0.08f, 0.09f, 0.13f, 0.96f}, "picker-bg");
    uiDrawRectOutline(bg, {0.3f, 0.5f, 0.7f, 1.0f}, "picker-border");

    const GuiElement* titleEl = layout.get("pickerTitle");
    float tdx = titleEl ? titleEl->x : (pdx + 14.0f);
    float tdy = titleEl ? titleEl->y : (pdy + 12.0f);
    float tsize = titleEl ? titleEl->fontSize : 0.35f;
    if (tsize <= 0.0f) tsize = 0.35f;
    uiDrawText("SELECT PNG", uiScaleX(tdx), uiScaleY(tdy), tsize, {0.8f, 0.85f, 0.95f, 1.0f});

    const GuiElement* closeEl = layout.get("pickerClose");
    float clx = closeEl ? closeEl->x : (pdx + pdw - 36.0f);
    float cly = closeEl ? closeEl->y : (pdy + 8.0f);
    float clw = closeEl ? closeEl->w : 28.0f;
    float clh = closeEl ? closeEl->h : 28.0f;
    if (uiButton(win, "X", {clx, cly, clw, clh},
                 {0.5f, 0.15f, 0.15f, 1.0f}).clicked) { closePicker(); }

    const GuiElement* rowEl = layout.get("pickerRow");
    float rx = rowEl ? rowEl->x : (pdx + 14.0f);
    float ry = rowEl ? rowEl->y : (pdy + 58.0f);
    float rw = rowEl ? rowEl->w : (pdw - 28.0f);
    float rh = rowEl ? rowEl->h : 30.0f;

    int visible = (int)((pdy + pdh - 10.0f - ry) / rh);
    int count = (int)gPickerFiles.size();

    uiDrawText("Click a file or type path + ENTER", uiScaleX(rx), uiScaleY(ry - 28.0f),
               0.22f, {0.5f, 0.6f, 0.7f, 1.0f});

    float itemY = ry;

    const GuiElement* suEl = layout.get("pickerScrollUp");
    float sux = suEl ? suEl->x : (pdx + pdw - 46.0f);
    float suy = suEl ? suEl->y : (ry - 26.0f);
    float suw = suEl ? suEl->w : 40.0f;
    float suh = suEl ? suEl->h : 20.0f;

    if (gPickerScroll > 0) {
        if (uiButton(win, "^", {sux, suy, suw, suh},
                     {0.2f, 0.3f, 0.4f, 1.0f}).clicked) {
            gPickerScroll = std::max(0, gPickerScroll - 1);
        }
    }

    for (int i = gPickerScroll; i < count && i < gPickerScroll + visible; ++i) {
        UIRect rowRect = {rx, itemY, rw, rh - 2.0f};
        UIRect sr = cs.designToScreen(rowRect);
        glm::vec4 fb = (i % 2 == 0) ? glm::vec4{0.12f, 0.13f, 0.17f, 1.0f}
                                     : glm::vec4{0.10f, 0.11f, 0.15f, 1.0f};
        uiDrawRect(sr, fb, "picker-file");
        float fsize = rowEl ? rowEl->fontSize : 0.24f;
        if (fsize <= 0.0f) fsize = 0.24f;
        uiDrawText(gPickerFiles[i].c_str(), sr.x + 6.0f, sr.y + 4.0f, fsize,
                   {1.0f, 1.0f, 1.0f, 1.0f});

        if (uiButton(win, "", rowRect, fb).clicked) {
            AvatarSystem& avs = AvatarSystem::instance();
            if (gOpenPickerSlot < 4) {
                SimpleAvatar s = avs.current().simple;
                switch (gOpenPickerSlot) {
                    case 0: s.face = gPickerFiles[i]; break;
                    case 1: s.shirt = gPickerFiles[i]; break;
                    case 2: s.pants = gPickerFiles[i]; break;
                    case 3: s.skin = gPickerFiles[i]; break;
                }
                avs.setSimple(s);
            } else {
                int pi = (gOpenPickerSlot - 4) / 6;
                int fi = (gOpenPickerSlot - 4) % 6;
                if (pi >= 0 && pi < 6 && fi >= 0 && fi < 6)
                    avs.setPartFace(kPartKeys[pi], kFaceKeys[fi], gPickerFiles[i]);
            }
            closePicker();
            printf("[AVATAR UI] Avatar slot assigned\n");
        }
        itemY += rh;
    }

    if (gPickerScroll + visible < count) {
        const GuiElement* sdEl = layout.get("pickerScrollDown");
        float sdx = sdEl ? sdEl->x : (pdx + pdw - 46.0f);
        float sdy = sdEl ? sdEl->y : itemY;
        float sdw = sdEl ? sdEl->w : 40.0f;
        float sdh = sdEl ? sdEl->h : 20.0f;
        if (uiButton(win, "v", {sdx, sdy, sdw, sdh},
                     {0.2f, 0.3f, 0.4f, 1.0f}).clicked) {
            gPickerScroll = std::min(gPickerScroll + 1, count - visible);
        }
    }
}

// ─── draw a labeled slot row (simple mode) ─────────────────────────

struct SlotRow {
    float x, y, w, h;
    const char* label;
    std::string value;
    int slotIndex;
};

static void drawSlotRow(GLFWwindow* win, const SlotRow& s, GuiLayout& layout) {
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    float xs = uiScaleX(s.x), ys = uiScaleY(s.y);
    float ws = uiScaleX(s.w), hs = uiScaleY(s.h);

    float txtSize = 0.30f;
    uiDrawText(s.label, xs, ys, txtSize, {0.7f, 0.8f, 0.9f, 1.0f});
    float tw = uiMeasureText(s.label, txtSize) + 12.0f;

    float vx = xs + tw;
    float vw = ws - tw;

    const GuiElement* browseEl = layout.get("simpleSlotBrowse");
    float browseDW = browseEl ? (float)browseEl->w : 75.0f;
    float browseW = uiScaleX(browseDW);
    vw -= browseW + uiScaleX(4.0f);

    glm::vec4 vc = s.value.empty() ? glm::vec4{0.14f, 0.14f, 0.18f, 1.0f}
                                   : glm::vec4{0.16f, 0.28f, 0.16f, 1.0f};
    uiDrawRect({vx, ys, vw, hs}, vc, "slot-val");
    uiDrawText(s.value.empty() ? "<none>" : s.value.c_str(),
               vx + 6.0f, ys + 4.0f, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});

    if (uiButton(win, "", cs.screenToDesign({vx, ys, vw, hs}), vc).clicked) {
        openPicker(s.slotIndex, AvatarSystem::instance().listPngs(
            AvatarSystem::instance().currentName()));
    }

    float bx = vx + vw + uiScaleX(4.0f);
    UIRect browseRect = cs.screenToDesign({bx, ys, browseW, hs});
    if (uiButton(win, "BROWSE", browseRect,
                 {0.25f, 0.35f, 0.5f, 1.0f}).clicked) {
        openPicker(s.slotIndex, AvatarSystem::instance().listPngs(
            AvatarSystem::instance().currentName()));
    }
}

// ─── draw a small advanced-mode slot ────────────────────────────────

static void drawAdvSlot(GLFWwindow* win, float x, float y, float w, float h,
                        const std::string& faceLabel, const std::string& value,
                        int slotIndex) {
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    float xs = uiScaleX(x), ys = uiScaleY(y);
    float ws = uiScaleX(w), hs = uiScaleY(h);

    uiDrawText(faceLabel.c_str(), xs, ys, 0.22f, {0.6f, 0.65f, 0.75f, 1.0f});
    float tw = uiMeasureText(faceLabel.c_str(), 0.22f) + 6.0f;

    float vx = xs + tw, vw = ws - tw;

    glm::vec4 vc = value.empty() ? glm::vec4{0.10f, 0.10f, 0.14f, 1.0f}
                                 : glm::vec4{0.14f, 0.22f, 0.14f, 1.0f};
    uiDrawRect({vx, ys, vw, hs}, vc, "adv-val");
    uiDrawText(value.empty() ? "<default>" : value.c_str(),
               vx + 3.0f, ys + 2.0f, 0.20f, {1.0f, 1.0f, 1.0f, 1.0f});

    float sx = cs.scaleX();
    float btnX = x + tw / sx;
    float btnW = w - tw / sx;
    if (uiButton(win, "", {btnX, y, btnW, h}, vc).clicked) {
        openPicker(slotIndex, AvatarSystem::instance().listPngs(
            AvatarSystem::instance().currentName()));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  PUBLIC — drawAvatarMenu
// ═══════════════════════════════════════════════════════════════════

AvatarMenuResult drawAvatarMenu(GLFWwindow* win) {
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    printf("[AVATAR UI] Opening Avatar Creator\n");
    Terminal::instance().addLog("[AVATAR UI] Opening Avatar Creator");

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    uiDrawRect({0, 0, (float)fbW, (float)fbH}, {0.030f, 0.035f, 0.048f, 1.0f}, "avatar-bg");

    // ── title ─────────────────────────────────────────────────────
    {
        const GuiElement* te = layout.get("title");
        float tx = te ? te->x : 50.0f;
        float ty = te ? te->y : 28.0f;
        float tsize = te ? te->fontSize : 0.65f;
        if (tsize <= 0.0f) tsize = 0.65f;
        uiDrawText("AVATAR CREATOR", uiScaleX(tx), uiScaleY(ty), tsize,
                   {0.95f, 0.98f, 1.0f, 1.0f});
    }

    // ── left column: avatar list ──────────────────────────────────

    const GuiElement* ah = layout.get("avatarHeading");
    {
        float hx = ah ? ah->x : 50.0f;
        float hy = ah ? ah->y : 90.0f;
        float hsize = ah ? ah->fontSize : 0.38f;
        if (hsize <= 0.0f) hsize = 0.38f;
        uiDrawText("AVATARS", uiScaleX(hx), uiScaleY(hy), hsize,
                   {0.65f, 0.85f, 1.0f, 1.0f});
    }

    const GuiElement* ae = layout.get("avatarEntry");
    float entryX = ae ? ae->x : 54.0f;
    float entryW = ae ? ae->w : 312.0f;
    float entryH = ae ? ae->h : 30.0f;

    const GuiElement* asp = layout.get("avatarEntrySpacing");
    float avatarSpacing = asp ? asp->h : 4.0f;
    float avatarStep = entryH + avatarSpacing;

    float ly = ah ? (ah->y + ah->h + 4.0f) : 126.0f;

    std::vector<std::string> avatars = av.listAvatars();
    for (size_t i = 0; i < avatars.size(); ++i) {
        float curY = ly + i * avatarStep;
        UIRect ar = {entryX, curY, entryW, entryH};
        UIRect sr = cs.designToScreen(ar);
        bool active = avatars[i] == av.currentName();
        glm::vec4 ab = active ? glm::vec4{0.18f, 0.45f, 0.25f, 1.0f}
                              : glm::vec4{0.09f, 0.11f, 0.16f, 1.0f};
        uiDrawRect(sr, ab, "avatar-entry");
        if (active)
            uiDrawRectOutline(sr, {0.3f, 0.8f, 0.5f, 1.0f}, "avatar-active");
        float esize = ae ? ae->fontSize : 0.26f;
        if (esize <= 0.0f) esize = 0.26f;
        uiDrawText(avatars[i].c_str(), sr.x + 6.0f, sr.y + 4.0f, esize,
                   {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, avatars[i].c_str(), ar, ab).clicked && !active) {
            printf("[AVATAR UI] Loading avatar: %s\n", avatars[i].c_str());
            av.loadAvatar(avatars[i]);
            GetPlayerSettings().avatarName = avatars[i];
            SavePlayerSettings();
        }
    }

    // active avatar name
    if (av.hasAvatar()) {
        const GuiElement* aat = layout.get("avatarActiveText");
        float atx = aat ? aat->x : 50.0f;
        float aty = ly + (float)avatars.size() * avatarStep + 8.0f;
        float atsize = aat ? aat->fontSize : 0.26f;
        if (atsize <= 0.0f) atsize = 0.26f;
        std::string cur = "Active: " + av.currentName();
        uiDrawText(cur.c_str(), uiScaleX(atx), uiScaleY(aty), atsize,
                   {0.5f, 0.9f, 0.5f, 1.0f});
    }

    // ── middle column: simple mode ────────────────────────────────

    const GuiElement* sh = layout.get("simpleHeading");
    float col2X = sh ? sh->x : 420.0f;

    {
        float hx = sh ? sh->x : col2X;
        float hy = sh ? sh->y : 90.0f;
        float hsize = sh ? sh->fontSize : 0.38f;
        if (hsize <= 0.0f) hsize = 0.38f;
        uiDrawText("SIMPLE MODE", uiScaleX(hx), uiScaleY(hy), hsize,
                   {0.95f, 0.98f, 1.0f, 1.0f});
    }

    const GuiElement* sd = layout.get("simpleDesc");
    float descY = sd ? sd->y : (sh ? sh->y + sh->h + 2.0f : 128.0f);
    {
        float dx = sd ? sd->x : col2X;
        float dsize = sd ? sd->fontSize : 0.22f;
        if (dsize <= 0.0f) dsize = 0.22f;
        uiDrawText("4 images \u2192 auto-assigned to all body part faces",
                   uiScaleX(dx), uiScaleY(descY), dsize, {0.5f, 0.6f, 0.7f, 1.0f});
    }

    const GuiElement* ssr = layout.get("simpleSlotRow");
    float slotX = ssr ? ssr->x : col2X;
    float slotY = ssr ? ssr->y : (descY + 28.0f);
    float slotW = ssr ? ssr->w : 580.0f;
    float slotH = ssr ? ssr->h : 34.0f;

    const GuiElement* ssp = layout.get("simpleSlotSpacing");
    float slotGap = ssp ? ssp->h : 8.0f;

    if (av.hasAvatar()) {
        const SimpleAvatar& sa = av.current().simple;
        for (int i = 0; i < 4; ++i) {
            std::string val;
            switch (i) {
                case 0: val = sa.face; break;
                case 1: val = sa.shirt; break;
                case 2: val = sa.pants; break;
                case 3: val = sa.skin; break;
            }
            drawSlotRow(win, {slotX, slotY, slotW, slotH, kSimpleLabels[i], val, i}, layout);
            slotY += slotH + slotGap;
        }
    }

    // mapping hints — Y is always dynamically positioned after slots
    {
        const char* hintTexts[4] = {
            "face.png \u2192 head front",
            "shirt.png \u2192 torso + arms",
            "pants.png \u2192 legs",
            "skin.png \u2192 head sides / top / back"
        };
        float hintBaseY = slotY + 6.0f;
        for (int i = 0; i < 4; ++i) {
            char hintId[16];
            snprintf(hintId, sizeof(hintId), "hint%d", i);
            const GuiElement* he = layout.get(hintId);
            float hx = he ? he->x : col2X;
            float hy = hintBaseY + (float)i * 18.0f;
            float hsize = he ? he->fontSize : 0.20f;
            if (hsize <= 0.0f) hsize = 0.20f;
            uiDrawText(hintTexts[i], uiScaleX(hx), uiScaleY(hy), hsize,
                       {0.45f, 0.55f, 0.65f, 1.0f});
        }
    }

    // ── save / apply / back buttons ───────────────────────────────

    const GuiElement* sb = layout.get("saveButton");
    const GuiElement* abtn = layout.get("applyButton");
    const GuiElement* bb = layout.get("backButton");

    if (gOpenPickerSlot < 0) {
        if (sb && uiButton(win, "SAVE", {sb->x, sb->y, sb->w, sb->h},
                           {0.18f, 0.50f, 0.26f, 1.0f}).clicked)
        {
            if (av.hasAvatar()) {
                if (av.current().advancedMode)
                    av.saveAdvanced(av.currentName(), av.current());
                else
                    av.saveSimple(av.currentName(), av.current().simple);
                av.loadAvatar(av.currentName());
                Terminal::instance().addLog("[AVATAR] Saved avatar: " + av.currentName());
                printf("[AVATAR UI] Avatar saved: %s\n", av.currentName().c_str());
                r.goSave = true;
            }
        }

        if (abtn && uiButton(win, "APPLY", {abtn->x, abtn->y, abtn->w, abtn->h},
                             {0.22f, 0.38f, 0.55f, 1.0f}).clicked)
        {
            printf("[AVATAR UI] Apply clicked\n");
            r.goApply = true;
        }

        if (bb && uiButton(win, "BACK", {bb->x, bb->y, bb->w, bb->h},
                           {0.50f, 0.18f, 0.18f, 1.0f}).clicked)
        {
            printf("[AVATAR UI] Back clicked\n");
            r.goBack = true;
        }
    }

    // ── advanced mode toggle ──────────────────────────────────────

    const GuiElement* at = layout.get("advToggle");
    float advToggleX = at ? at->x : col2X;
    float advToggleY = at ? at->y : (sb ? sb->y - 56.0f : 644.0f);
    float advToggleW = at ? at->w : 200.0f;
    float advToggleH = at ? at->h : 30.0f;

    bool adv = av.hasAvatar() && av.current().advancedMode;
    if (uiButton(win, adv ? "ADVANCED MODE: ON" : "ADVANCED MODE: OFF",
                 {advToggleX, advToggleY, advToggleW, advToggleH},
                 adv ? glm::vec4{0.35f, 0.25f, 0.55f, 1.0f}
                     : glm::vec4{0.12f, 0.14f, 0.18f, 1.0f}).clicked)
    {
        av.setAdvancedMode(!adv);
        printf("[AVATAR UI] Advanced mode toggled: %s\n", adv ? "OFF" : "ON");
    }

    // ── advanced mode per-face ────────────────────────────────────

    if (adv && av.hasAvatar()) {
        const GuiElement* advh = layout.get("advHeading");
        float ax = advh ? advh->x : 1050.0f;
        float ay = advh ? advh->y : 90.0f;

        {
            float hx = advh ? advh->x : ax;
            float hy = advh ? advh->y : ay;
            float hsize = advh ? advh->fontSize : 0.38f;
            if (hsize <= 0.0f) hsize = 0.38f;
            uiDrawText("ADVANCED MODE", uiScaleX(hx), uiScaleY(hy), hsize,
                       {0.95f, 0.80f, 1.0f, 1.0f});
        }

        const GuiElement* adesc = layout.get("advDesc");
        {
            float dx = adesc ? adesc->x : ax;
            float dy = adesc ? adesc->y : (ay + 38.0f);
            float dsize = adesc ? adesc->fontSize : 0.22f;
            if (dsize <= 0.0f) dsize = 0.22f;
            uiDrawText("Assign each face individually", uiScaleX(dx), uiScaleY(dy), dsize,
                       {0.5f, 0.6f, 0.7f, 1.0f});
        }

        const GuiElement* afs = layout.get("advFaceSlot");
        float colW = afs ? (afs->w + 4.0f) : 130.0f;
        float faceH = afs ? afs->h : 24.0f;

        const GuiElement* afg = layout.get("advFaceSpacing");
        float faceGap = afg ? afg->w : 3.0f;

        const GuiElement* arg = layout.get("advRowSpacing");
        float rowGap = arg ? arg->h : 6.0f;

        const GuiElement* alh = layout.get("advListHeight");
        float listH = alh ? alh->h : 560.0f;

        const GuiElement* aph = layout.get("advPartHeading");
        float partH = aph ? aph->h : 22.0f;

        float faceStep = faceH + faceGap;
        float partStep = partH + faceStep * 6.0f + rowGap;

        int totalRows = 6;
        int visibleRows = (int)(listH / partStep);
        if (visibleRows < 1) visibleRows = 1;

        float startAy = ay + 66.0f;
        float currentAy = startAy;

        const GuiElement* ascUp = layout.get("advScrollUp");
        if (gAdvScroll > 0) {
            float sux = ascUp ? ascUp->x : ax;
            float suy = ascUp ? ascUp->y : (currentAy - 24.0f);
            float suw = ascUp ? ascUp->w : 70.0f;
            float suh = ascUp ? ascUp->h : 20.0f;
            if (uiButton(win, "^ up", {sux, suy, suw, suh},
                         {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
                gAdvScroll = std::max(0, gAdvScroll - 1);
        }

        auto getPart = [&](int idx) -> const AvatarPartFaces* {
            const AvatarDefinition& def = av.current();
            if (idx == 0) return &def.head;
            if (idx == 1) return &def.torso;
            if (idx == 2) return &def.leftArm;
            if (idx == 3) return &def.rightArm;
            if (idx == 4) return &def.leftLeg;
            if (idx == 5) return &def.rightLeg;
            return nullptr;
        };

        for (int pi = gAdvScroll; pi < 6 && pi < gAdvScroll + visibleRows; ++pi) {
            const AvatarPartFaces* part = getPart(pi);
            if (!part) continue;

            float px = uiScaleX(ax);
            float py = uiScaleY(currentAy);
            float psize = aph ? aph->fontSize : 0.30f;
            if (psize <= 0.0f) psize = 0.30f;
            uiDrawText(kPartLabels[pi], px, py, psize,
                       {0.85f, 0.90f, 1.0f, 1.0f});
            currentAy += partH;

            for (int fi = 0; fi < 6; ++fi) {
                float fx = ax + fi * (colW + faceGap);
                std::string val = part->byName(kFaceKeys[fi]);
                int slotId = 4 + pi * 6 + fi;
                drawAdvSlot(win, fx, currentAy, colW, faceH,
                           kFaceLabels[fi], val, slotId);
            }
            currentAy += faceStep + 6.0f;
        }

        // scroll down
        if (gAdvScroll + visibleRows < totalRows) {
            const GuiElement* ascDn = layout.get("advScrollDown");
            float sdx = ascDn ? ascDn->x : ax;
            float sdy = ascDn ? ascDn->y : currentAy;
            float sdw = ascDn ? ascDn->w : 70.0f;
            float sdh = ascDn ? ascDn->h : 20.0f;
            if (uiButton(win, "v down", {sdx, sdy, sdw, sdh},
                         {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
                gAdvScroll = std::min(gAdvScroll + 1, totalRows - visibleRows);
        }

        // adv mode save/apply — Y is dynamic, below the list
        float advBtnY = currentAy + 24.0f;
        const GuiElement* sab = layout.get("saveAdvButton");
        if (sab && uiButton(win, "SAVE ADV",
                            {sab->x, advBtnY, sab->w, sab->h},
                            {0.18f, 0.50f, 0.26f, 1.0f}).clicked)
        {
            if (av.hasAvatar()) {
                av.saveAdvanced(av.currentName(), av.current());
                av.loadAvatar(av.currentName());
                Terminal::instance().addLog("[AVATAR] Saved (advanced): " + av.currentName());
                printf("[AVATAR UI] Advanced saved\n");
                r.goSave = true;
            }
        }
        const GuiElement* aab = layout.get("applyAdvButton");
        if (aab && uiButton(win, "APPLY ADV",
                            {aab->x, advBtnY, aab->w, aab->h},
                            {0.22f, 0.38f, 0.55f, 1.0f}).clicked)
            r.goApply = true;
    }

    // ── file picker overlay ───────────────────────────────────────
    drawPicker(win, layout);

    return r;
}
