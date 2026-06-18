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

// ─── helpers ────────────────────────────────────────────────────────

static float pickerSectionHeight() {
    return 420.0f;
}

// ─── draw the file picker overlay ───────────────────────────────────

static void drawPicker(GLFWwindow* win) {
    if (gOpenPickerSlot < 0 || gPickerFiles.empty()) return;

    float pw = 560.0f, ph = pickerSectionHeight();
    float px = (1920.0f - pw) * 0.5f;
    float py = (1080.0f - ph) * 0.5f;

    UIRect bg = {uiScaleX(px), uiScaleY(py), uiScaleX(pw), uiScaleY(ph)};
    uiDrawRect(bg, {0.08f, 0.09f, 0.13f, 0.96f}, "picker-bg");
    uiDrawRectOutline(bg, {0.3f, 0.5f, 0.7f, 1.0f}, "picker-border");

    float cx = bg.x + 14.0f;
    float cy = bg.y + 12.0f;
    float cw = bg.w - 28.0f;

    uiDrawText("SELECT PNG", cx, cy, 0.35f, {0.8f, 0.85f, 0.95f, 1.0f});
    cy += 32.0f;

    // close X
    if (uiButton(win, "X", {bg.x + bg.w - 36.0f, bg.y + 8.0f, 28.0f, 28.0f},
                 {0.5f, 0.15f, 0.15f, 1.0f}).clicked) { closePicker(); }

    // file list
    float rowH = 30.0f;
    int visible = (int)((bg.y + bg.h - cy - 10.0f) / rowH);
    int count = (int)gPickerFiles.size();

    uiDrawText("Click a file or type path + ENTER", cx, cy, 0.22f, {0.5f, 0.6f, 0.7f, 1.0f});
    cy += 26.0f;

    // scroll up
    if (gPickerScroll > 0) {
        if (uiButton(win, "^", {cx + cw - 40.0f, cy - 24.0f, 40.0f, 20.0f},
                     {0.2f, 0.3f, 0.4f, 1.0f}).clicked) {
            gPickerScroll = std::max(0, gPickerScroll - 1);
        }
    }

    for (int i = gPickerScroll; i < count && i < gPickerScroll + visible; ++i) {
        UIRect fr = {cx, cy, cw, rowH - 2.0f};
        glm::vec4 fb = (i % 2 == 0) ? glm::vec4{0.12f, 0.13f, 0.17f, 1.0f}
                                     : glm::vec4{0.10f, 0.11f, 0.15f, 1.0f};
        uiDrawRect(fr, fb, "picker-file");
        uiDrawText(gPickerFiles[i].c_str(), fr.x + 6.0f, fr.y + 4.0f, 0.24f,
                   {1.0f, 1.0f, 1.0f, 1.0f});

        if (uiButton(win, "", fr, fb).clicked) {
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
        cy += rowH;
    }

    // scroll down
    if (gPickerScroll + visible < count) {
        if (uiButton(win, "v", {cx + cw - 40.0f, cy, 40.0f, 20.0f},
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

static void drawSlotRow(GLFWwindow* win, const SlotRow& s) {
    float xs = uiScaleX(s.x), ys = uiScaleY(s.y);
    float ws = uiScaleX(s.w), hs = uiScaleY(s.h);

    uiDrawText(s.label, xs, ys, 0.30f, {0.7f, 0.8f, 0.9f, 1.0f});
    float tw = uiMeasureText(s.label, 0.30f) + 12.0f;

    // value field + picker button
    float vx = xs + tw, vw = ws - tw - uiScaleX(75.0f);
    glm::vec4 vc = s.value.empty() ? glm::vec4{0.14f, 0.14f, 0.18f, 1.0f}
                                   : glm::vec4{0.16f, 0.28f, 0.16f, 1.0f};
    uiDrawRect({vx, ys, vw, hs}, vc, "slot-val");
    uiDrawText(s.value.empty() ? "<none>" : s.value.c_str(),
               vx + 6.0f, ys + 4.0f, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});

    if (uiButton(win, "", {vx, ys, vw, hs}, vc).clicked) {
        openPicker(s.slotIndex, AvatarSystem::instance().listPngs(
            AvatarSystem::instance().currentName()));
    }

    // browse button
    float bx = vx + vw + 4.0f;
    if (uiButton(win, "BROWSE", {bx, ys, uiScaleX(70.0f), hs},
                 {0.25f, 0.35f, 0.5f, 1.0f}).clicked) {
        openPicker(s.slotIndex, AvatarSystem::instance().listPngs(
            AvatarSystem::instance().currentName()));
    }
}

// ─── draw a small advanced-mode slot ────────────────────────────────

static void drawAdvSlot(GLFWwindow* win, float x, float y, float w, float h,
                        const std::string& faceLabel, const std::string& value,
                        int slotIndex) {
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

    if (uiButton(win, "", {vx, ys, vw, hs}, vc).clicked) {
        openPicker(slotIndex, AvatarSystem::instance().listPngs(
            AvatarSystem::instance().currentName()));
    }
}

// ─── draw a section heading ─────────────────────────────────────────

static void drawHeading(const char* text, float x, float y, glm::vec4 color) {
    uiDrawText(text, uiScaleX(x), uiScaleY(y), 0.38f, color);
}

// ═══════════════════════════════════════════════════════════════════
//  PUBLIC — drawAvatarMenu
// ═══════════════════════════════════════════════════════════════════

AvatarMenuResult drawAvatarMenu(GLFWwindow* win) {
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();

    printf("[AVATAR UI] Opening Avatar Creator\n");
    Terminal::instance().addLog("[AVATAR UI] Opening Avatar Creator");

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    // background
    uiDrawRect({0, 0, (float)fbW, (float)fbH}, {0.030f, 0.035f, 0.048f, 1.0f}, "avatar-bg");

    // ── title ─────────────────────────────────────────────────────
    uiDrawText("AVATAR CREATOR", uiScaleX(50.0f), uiScaleY(28.0f), 0.65f,
               {0.95f, 0.98f, 1.0f, 1.0f});

    // ── left column: avatar list ──────────────────────────────────
    float lx = 50.0f, ly = 90.0f;
    const float col1X = lx, col1W = 320.0f;
    const float col2X = 420.0f, col2W = 580.0f;
    const float rowH = 34.0f;
    const float gap = 8.0f;

    drawHeading("AVATARS", col1X, ly, {0.65f, 0.85f, 1.0f, 1.0f});
    ly += 36.0f;

    std::vector<std::string> avatars = av.listAvatars();
    for (size_t i = 0; i < avatars.size(); ++i) {
        UIRect ar = {uiScaleX(col1X + 4.0f), uiScaleY(ly),
                     uiScaleX(col1W - 8.0f), uiScaleY(30.0f)};
        bool active = avatars[i] == av.currentName();
        glm::vec4 ab = active ? glm::vec4{0.18f, 0.45f, 0.25f, 1.0f}
                              : glm::vec4{0.09f, 0.11f, 0.16f, 1.0f};
        uiDrawRect(ar, ab, "avatar-entry");
        if (active)
            uiDrawRectOutline(ar, {0.3f, 0.8f, 0.5f, 1.0f}, "avatar-active");
        uiDrawText(avatars[i].c_str(), ar.x + 6.0f, ar.y + 4.0f, 0.26f,
                   {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, avatars[i].c_str(), ar, ab).clicked && !active) {
            printf("[AVATAR UI] Loading avatar: %s\n", avatars[i].c_str());
            av.loadAvatar(avatars[i]);
            GetPlayerSettings().avatarName = avatars[i];
            SavePlayerSettings();
        }
        ly += 34.0f;
    }

    // current avatar name
    if (av.hasAvatar()) {
        std::string cur = "Active: " + av.currentName();
        uiDrawText(cur.c_str(), uiScaleX(col1X), uiScaleY(ly + 8.0f), 0.26f,
                   {0.5f, 0.9f, 0.5f, 1.0f});
    }

    // ── middle column: simple mode ────────────────────────────────
    float sy = 90.0f;
    drawHeading("SIMPLE MODE", col2X, sy, {0.95f, 0.98f, 1.0f, 1.0f});
    sy += 38.0f;
    uiDrawText("4 images → auto-assigned to all body part faces",
               uiScaleX(col2X), uiScaleY(sy), 0.22f, {0.5f, 0.6f, 0.7f, 1.0f});
    sy += 28.0f;

    if (av.hasAvatar()) {
        const SimpleAvatar& s = av.current().simple;
        for (int i = 0; i < 4; ++i) {
            std::string val;
            switch (i) {
                case 0: val = s.face; break;
                case 1: val = s.shirt; break;
                case 2: val = s.pants; break;
                case 3: val = s.skin; break;
            }
            drawSlotRow(win, {col2X, sy, col2W, rowH, kSimpleLabels[i], val, i});
            sy += rowH + gap;
        }
        sy += 6.0f;
    }

    // mapping hint
    float hintY = sy;
    uiDrawText("face.png → head front", uiScaleX(col2X), uiScaleY(hintY), 0.20f,
               {0.45f, 0.55f, 0.65f, 1.0f});
    hintY += 18.0f;
    uiDrawText("shirt.png → torso + arms", uiScaleX(col2X), uiScaleY(hintY), 0.20f,
               {0.45f, 0.55f, 0.65f, 1.0f});
    hintY += 18.0f;
    uiDrawText("pants.png → legs", uiScaleX(col2X), uiScaleY(hintY), 0.20f,
               {0.45f, 0.55f, 0.65f, 1.0f});
    hintY += 18.0f;
    uiDrawText("skin.png → head sides / top / back", uiScaleX(col2X), uiScaleY(hintY), 0.20f,
               {0.45f, 0.55f, 0.65f, 1.0f});

    // ── save / apply / back buttons ───────────────────────────────
    float btnY = 700.0f;
    if (gOpenPickerSlot < 0) {
        if (uiButton(win, "SAVE",
            {uiScaleX(col2X), uiScaleY(btnY), uiScaleX(140.0f), uiScaleY(42.0f)},
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

        if (uiButton(win, "APPLY",
            {uiScaleX(col2X + 155.0f), uiScaleY(btnY), uiScaleX(140.0f), uiScaleY(42.0f)},
            {0.22f, 0.38f, 0.55f, 1.0f}).clicked)
        {
            printf("[AVATAR UI] Apply clicked\n");
            r.goApply = true;
        }

        if (uiButton(win, "BACK",
            {uiScaleX(col2X + 310.0f), uiScaleY(btnY), uiScaleX(140.0f), uiScaleY(42.0f)},
            {0.50f, 0.18f, 0.18f, 1.0f}).clicked)
        {
            printf("[AVATAR UI] Back clicked\n");
            r.goBack = true;
        }
    }

    // ── advanced mode toggle ──────────────────────────────────────
    float advToggleY = btnY - 56.0f;
    bool adv = av.hasAvatar() && av.current().advancedMode;
    UIRect advToggle = {uiScaleX(col2X), uiScaleY(advToggleY),
                        uiScaleX(200.0f), uiScaleY(30.0f)};
    if (uiButton(win, adv ? "ADVANCED MODE: ON" : "ADVANCED MODE: OFF",
                 advToggle,
                 adv ? glm::vec4{0.35f, 0.25f, 0.55f, 1.0f}
                     : glm::vec4{0.12f, 0.14f, 0.18f, 1.0f}).clicked)
    {
        av.setAdvancedMode(!adv);
        printf("[AVATAR UI] Advanced mode toggled: %s\n", adv ? "OFF" : "ON");
    }

    // ── advanced mode per-face ────────────────────────────────────
    if (adv && av.hasAvatar()) {
        float ax = 1050.0f, ay = 90.0f;
        float sectionW = 780.0f;
        float colW = sectionW / 6.0f;
        float faceH = 24.0f;
        float faceGap = 3.0f;

        drawHeading("ADVANCED MODE", ax, ay, {0.95f, 0.80f, 1.0f, 1.0f});
        ay += 38.0f;
        uiDrawText("Assign each face individually", uiScaleX(ax), uiScaleY(ay), 0.22f,
                   {0.5f, 0.6f, 0.7f, 1.0f});
        ay += 28.0f;

        // scroll arrows
        float listH = 560.0f;
        int totalRows = 6; // 6 body parts
        int visibleRows = (int)(listH / (faceH + faceGap + 22.0f));
        if (visibleRows < 1) visibleRows = 1;

        if (gAdvScroll > 0) {
            if (uiButton(win, "^ up",
                {uiScaleX(ax), uiScaleY(ay - 24.0f), uiScaleX(70.0f), uiScaleY(20.0f)},
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

        float startAy = ay;
        for (int pi = gAdvScroll; pi < 6 && pi < gAdvScroll + visibleRows; ++pi) {
            const AvatarPartFaces* part = getPart(pi);
            if (!part) continue;

            // part heading
            float px = uiScaleX(ax);
            float py = uiScaleY(ay);
            uiDrawText(kPartLabels[pi], px, py, 0.30f, {0.85f, 0.90f, 1.0f, 1.0f});
            ay += 22.0f;

            // 6 faces in a row
            for (int fi = 0; fi < 6; ++fi) {
                float fx = ax + fi * colW;
                std::string val = part->byName(kFaceKeys[fi]);
                int slotId = 4 + pi * 6 + fi;
                drawAdvSlot(win, fx, ay, colW - 4.0f, faceH,
                           kFaceLabels[fi], val, slotId);
            }
            ay += faceH + faceGap + 6.0f;
        }

        // scroll down
        if (gAdvScroll + visibleRows < totalRows) {
            if (uiButton(win, "v down",
                {uiScaleX(ax), uiScaleY(ay), uiScaleX(70.0f), uiScaleY(20.0f)},
                {0.2f, 0.3f, 0.4f, 1.0f}).clicked)
                gAdvScroll = std::min(gAdvScroll + 1, totalRows - visibleRows);
        }

        // adv mode save/apply
        float advBtnY = ay + 24.0f;
        if (uiButton(win, "SAVE ADV",
            {uiScaleX(ax), uiScaleY(advBtnY), uiScaleX(130.0f), uiScaleY(38.0f)},
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
        if (uiButton(win, "APPLY ADV",
            {uiScaleX(ax + 145.0f), uiScaleY(advBtnY), uiScaleX(130.0f), uiScaleY(38.0f)},
            {0.22f, 0.38f, 0.55f, 1.0f}).clicked)
            r.goApply = true;
    }

    // ── file picker overlay ───────────────────────────────────────
    drawPicker(win);

    return r;
}
