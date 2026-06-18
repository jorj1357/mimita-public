#include "avatar-menu.h"
#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "../gui/ui-system.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-coord.h"
#include "../gui/gui-button.h"
#include "../devtools/terminal.h"

extern "C" {
#include "stb_image.h"
}

namespace {

// Text input state (for path entry)
constexpr int INPUT_MAX = 128;
struct AvatarInput {
    bool active = false;
    char buffer[INPUT_MAX]{};
    int cursor = 0;
    std::string label;
    std::string targetPart;
    std::string targetFace;
};

AvatarInput gInput;

// Only one slot picker open at a time
int gOpenPickerSlot = -1; // -1 = none, 0-3 = simple slots, 4+ = advanced
int gPickerScroll = 0;
std::vector<std::string> gPickerFiles;

const char* kSimpleLabels[] = {"Face Image", "Shirt Image", "Pants Image", "Skin Image"};
const char* kSimpleKeys[] = {"face", "shirt", "pants", "skin"};
const char* kPartLabels[] = {"Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"};
const char* kPartKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
const char* kFaceLabels[] = {"Front", "Back", "Left", "Right", "Top", "Bottom"};
const char* kFaceKeys[] = {"front", "back", "left", "right", "top", "bottom"};

void startInput(const std::string& label, const std::string& part, const std::string& face, const std::string& initial) {
    gInput.active = true;
    gInput.label = label;
    gInput.targetPart = part;
    gInput.targetFace = face;
    gInput.cursor = (int)initial.size();
    std::memset(gInput.buffer, 0, INPUT_MAX);
    std::strncpy(gInput.buffer, initial.c_str(), INPUT_MAX - 1);
}

void stopInput() {
    gInput.active = false;
    gInput.label.clear();
}

void openPicker(int slot, const std::vector<std::string>& files) {
    gOpenPickerSlot = slot;
    gPickerFiles = files;
    gPickerScroll = 0;
}

void closePicker() {
    gOpenPickerSlot = -1;
    gPickerFiles.clear();
}

}

void avatarMenuHandleChar(unsigned int codepoint) {
    if (!gInput.active) return;
    if (codepoint >= 32 && codepoint <= 126 && gInput.cursor < INPUT_MAX - 1) {
        gInput.buffer[gInput.cursor++] = (char)codepoint;
        gInput.buffer[gInput.cursor] = '\0';
    }
}

void avatarMenuHandleKey(int key, int action) {
    if (!gInput.active || (action != GLFW_PRESS && action != GLFW_REPEAT))
        return;
    if (key == GLFW_KEY_BACKSPACE && gInput.cursor > 0) {
        gInput.buffer[--gInput.cursor] = '\0';
    } else if (key == GLFW_KEY_ENTER && gInput.cursor > 0) {
        AvatarSystem& av = AvatarSystem::instance();
        if (!gInput.targetFace.empty()) {
            av.setPartFace(gInput.targetPart, gInput.targetFace, std::string(gInput.buffer));
        } else if (!gInput.targetPart.empty()) {
            // Simple slot: match by key name
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

static void drawSlot(GLFWwindow* win, float x, float& y, float w, float h,
                     const char* label, const std::string& value, int slotIndex,
                     int fbW, int fbH) {
    float xS = uiScaleX(x);
    float yS = uiScaleY(y);
    float wS = uiScaleX(w);
    float hS = uiScaleY(h);

    uiDrawText(label, xS, yS, 0.30f, {0.8f, 0.85f, 0.95f, 1.0f});
    float textW = uiMeasureText(label, 0.30f) + 10.0f;

    // Value display (clickable to open picker)
    UIRect valRect = {xS + textW, yS, wS - textW, hS};
    glm::vec4 valColor = value.empty() ? glm::vec4{0.15f, 0.15f, 0.2f, 1.0f} : glm::vec4{0.2f, 0.3f, 0.2f, 1.0f};
    uiDrawRect(valRect, valColor, "avatar-slot");
    uiDrawText(value.empty() ? "<none>" : value.c_str(),
               xS + textW + 5.0f, yS + 2.0f, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});

    if (uiButton(win, "", valRect, valColor, label).clicked) {
        AvatarSystem& av = AvatarSystem::instance();
        openPicker(slotIndex, av.listPngs(av.currentName()));
    }

    // Edit button (pencil icon approximated as text)
    UIRect editRect = {xS + wS - uiScaleX(60.0f), yS, uiScaleX(60.0f), hS};
    if (uiButton(win, "EDIT", editRect, {0.3f, 0.3f, 0.5f, 1.0f}).clicked) {
        startInput(label, kSimpleKeys[slotIndex], "", value);
    }

    y += h + 8.0f;
}

static void drawAdvancedSlot(GLFWwindow* win, float x, float& y, float w, float h,
                             const std::string& value, int slotIndex,
                             const std::string& part, const std::string& face) {
    float xS = uiScaleX(x);
    float yS = uiScaleY(y);
    float wS = uiScaleX(w);
    float hS = uiScaleY(h);

    std::string label = part + " " + face;
    std::transform(label.begin(), label.end(), label.begin(), ::tolower);

    uiDrawText(label.c_str(), xS, yS, 0.22f, {0.7f, 0.75f, 0.85f, 1.0f});
    float textW = uiMeasureText(label.c_str(), 0.22f) + 10.0f;

    UIRect valRect = {xS + textW, yS, wS - textW - uiScaleX(60.0f), hS};
    glm::vec4 valColor = value.empty() ? glm::vec4{0.12f, 0.12f, 0.17f, 1.0f} : glm::vec4{0.17f, 0.25f, 0.17f, 1.0f};
    uiDrawRect(valRect, valColor, "adv-slot");
    uiDrawText(value.empty() ? "<skin>" : value.c_str(),
               xS + textW + 3.0f, yS + 1.0f, 0.22f, {1.0f, 1.0f, 1.0f, 1.0f});

    if (uiButton(win, "", valRect, valColor, label.c_str()).clicked) {
        AvatarSystem& av = AvatarSystem::instance();
        int pi = 0, fi = 0;
        for (int i = 0; i < 6; ++i) { if (part == kPartKeys[i]) { pi = i; break; } }
        for (int i = 0; i < 6; ++i) { if (face == kFaceKeys[i]) { fi = i; break; } }
        openPicker(4 + pi * 6 + fi, av.listPngs(av.currentName()));
    }

    UIRect editRect = {xS + wS - uiScaleX(55.0f), yS, uiScaleX(55.0f), hS};
    if (uiButton(win, "EDIT", editRect, {0.25f, 0.25f, 0.4f, 1.0f}).clicked) {
        startInput(label, part, face, value);
    }

    y += h + 4.0f;
}

AvatarMenuResult drawAvatarMenu(GLFWwindow* win) {
    AvatarMenuResult r{};
    AvatarSystem& av = AvatarSystem::instance();

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");

    // Background
    uiDrawRect({0, 0, (float)fbW, (float)fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "avatar-bg");

    // Title
    uiDrawText("AVATAR CREATOR", uiScaleX(50.0f), uiScaleY(30.0f), 0.7f, {0.95f, 0.98f, 1.0f, 1.0f});

    float currentY = 100.0f;
    const float colW = 600.0f;
    const float slotH = 32.0f;
    const float leftX = 50.0f;
    const float rightX = 680.0f;

    // Current avatar name
    std::string header = "Current: " + (av.hasAvatar() ? av.currentName() : "<none>");
    uiDrawText(header.c_str(), uiScaleX(leftX), uiScaleY(currentY), 0.35f, {0.6f, 1.0f, 0.6f, 1.0f});
    currentY += 40.0f;

    // Avatar list
    uiDrawText("Avatars:", uiScaleX(leftX), uiScaleY(currentY), 0.30f, {0.65f, 0.85f, 1.0f, 1.0f});
    currentY += 28.0f;
    std::vector<std::string> avatars = av.listAvatars();
    float listY = currentY;
    for (size_t i = 0; i < avatars.size() && i < 8; ++i) {
        UIRect aRect = {uiScaleX(leftX + 10.0f), uiScaleY(listY), uiScaleX(280.0f), uiScaleY(26.0f)};
        bool isActive = avatars[i] == av.currentName();
        glm::vec4 bg = isActive ? glm::vec4{0.2f, 0.5f, 0.3f, 1.0f} : glm::vec4{0.1f, 0.12f, 0.18f, 1.0f};
        uiDrawRect(aRect, bg, "avatar-entry");
        if (isActive)
            uiDrawRectOutline(aRect, {0.3f, 0.8f, 0.5f, 1.0f}, "avatar-active");
        uiDrawText(avatars[i].c_str(), aRect.x + 5.0f, aRect.y + 3.0f, 0.25f, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, avatars[i].c_str(), aRect, bg).clicked && !isActive) {
            av.loadAvatar(avatars[i]);
        }
        listY += 30.0f;
    }
    currentY = std::max(currentY, listY + 10.0f);

    // Simple mode inputs
    uiDrawText("Simple Mode:", uiScaleX(rightX), uiScaleY(100.0f), 0.35f, {0.95f, 0.98f, 1.0f, 1.0f});
    float sy = 140.0f;
    if (av.hasAvatar()) {
        const SimpleAvatar& s = av.current().simple;
        drawSlot(win, rightX, sy, colW, slotH, "Face", s.face, 0, fbW, fbH);
        drawSlot(win, rightX, sy, colW, slotH, "Shirt", s.shirt, 1, fbW, fbH);
        drawSlot(win, rightX, sy, colW, slotH, "Pants", s.pants, 2, fbW, fbH);
        drawSlot(win, rightX, sy, colW, slotH, "Skin", s.skin, 3, fbW, fbH);
    }
    currentY = std::max(currentY, sy + 10.0f);

    // Simple mode info
    uiDrawText("face.png -> head front", uiScaleX(leftX), uiScaleY(currentY), 0.22f, {0.5f, 0.6f, 0.7f, 1.0f});
    currentY += 18.0f;
    uiDrawText("shirt.png -> torso + arms", uiScaleX(leftX), uiScaleY(currentY), 0.22f, {0.5f, 0.6f, 0.7f, 1.0f});
    currentY += 18.0f;
    uiDrawText("pants.png -> legs", uiScaleX(leftX), uiScaleY(currentY), 0.22f, {0.5f, 0.6f, 0.7f, 1.0f});
    currentY += 18.0f;
    uiDrawText("skin.png -> head sides/top/back", uiScaleX(leftX), uiScaleY(currentY), 0.22f, {0.5f, 0.6f, 0.7f, 1.0f});
    currentY += 30.0f;

    // Advanced mode toggle
    bool adv = av.hasAvatar() && av.current().advancedMode;
    UIRect advToggle = {uiScaleX(leftX), uiScaleY(currentY), uiScaleX(220.0f), uiScaleY(28.0f)};
    if (uiButton(win, adv ? "ADVANCED: ON" : "ADVANCED: OFF", advToggle,
                 adv ? glm::vec4{0.4f, 0.3f, 0.6f, 1.0f} : glm::vec4{0.15f, 0.15f, 0.2f, 1.0f}).clicked) {
        av.setAdvancedMode(!adv);
    }
    currentY += 36.0f;

    // Advanced mode per-face slots
    if (adv && av.hasAvatar()) {
        float ax = leftX;
        float ay = currentY;
        const float advW = 920.0f;
        float rowW = advW / 6.0f;

        for (int pi = 0; pi < 6; ++pi) {
            float px = leftX + pi * rowW * uiScaleX(1.0f);
            if (pi > 0) px = leftX + (advW * pi / 6.0f);
            uiDrawText(kPartLabels[pi], uiScaleX(px), uiScaleY(ay), 0.28f, {0.8f, 0.9f, 1.0f, 1.0f});
            float fy = ay + 22.0f;

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
            const AvatarPartFaces* part = getPart(pi);
            if (!part) continue;

            for (int fi = 0; fi < 6; ++fi) {
                std::string val = part->byName(kFaceKeys[fi]);
                // Advanced slots drawn inline
                float fx = px;
                float fxs = uiScaleX(fx);
                float fys = uiScaleY(fy);
                float fws = uiScaleX(rowW - 4.0f);
                float fhs = uiScaleY(20.0f);

                uiDrawText(kFaceLabels[fi], fxs, fys, 0.20f, {0.6f, 0.65f, 0.75f, 1.0f});
                float tW = uiMeasureText(kFaceLabels[fi], 0.20f) + 4.0f;

                UIRect fRect = {fxs + tW, fys, fws - tW - uiScaleX(40.0f), fhs};
                glm::vec4 fColor = val.empty() ? glm::vec4{0.1f, 0.1f, 0.14f, 1.0f} : glm::vec4{0.14f, 0.2f, 0.14f, 1.0f};
                uiDrawRect(fRect, fColor, "adv-face");
                uiDrawText(val.empty() ? "<default>" : val.c_str(),
                           fRect.x + 2.0f, fRect.y + 1.0f, 0.18f, {1.0f, 1.0f, 1.0f, 1.0f});

                int slotId = 4 + pi * 6 + fi;
                if (uiButton(win, "", fRect, fColor, (std::to_string(slotId)).c_str()).clicked) {
                    openPicker(slotId, av.listPngs(av.currentName()));
                }

                fy += 23.0f;
            }
            ay = fy + 10.0f;
            currentY = std::max(currentY, ay);
        }
    }

    currentY += 20.0f;

    // File picker popup
    if (gOpenPickerSlot >= 0 && !gPickerFiles.empty()) {
        float pickerX = uiScaleX(400.0f);
        float pickerY = uiScaleY(250.0f);
        float pickerW = uiScaleX(500.0f);
        float pickerH = uiScaleY(400.0f);

        uiDrawRect({pickerX, pickerY, pickerW, pickerH}, {0.08f, 0.09f, 0.12f, 0.95f}, "picker-bg");
        uiDrawRectOutline({pickerX, pickerY, pickerW, pickerH}, {0.3f, 0.5f, 0.7f, 1.0f}, "picker-border");
        uiDrawText("Select PNG (click or type path then ENTER):",
                   pickerX + 10.0f, pickerY + 10.0f, 0.25f, {0.8f, 0.85f, 0.95f, 1.0f});

        // Text input for manual path
        UIRect inputRect = {pickerX + 10.0f, pickerY + 40.0f, pickerW - 20.0f, uiScaleY(28.0f)};
        uiDrawRect(inputRect, {0.12f, 0.14f, 0.2f, 1.0f}, "picker-input");
        uiDrawText(gInput.active ? gInput.buffer : "(click files below or type path)",
                   inputRect.x + 5.0f, inputRect.y + 3.0f, 0.24f, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, "OK", {inputRect.x + inputRect.w - uiScaleX(40.0f), inputRect.y, uiScaleX(40.0f), inputRect.h},
                     {0.2f, 0.4f, 0.3f, 1.0f}).clicked) {
            // Apply current input buffer if text entry was done via keyboard
        }

        // File list
        float fileY = pickerY + 75.0f;
        int visibleCount = (int)(pickerH - 85.0f) / 30;
        for (int i = gPickerScroll; i < (int)gPickerFiles.size() && i < gPickerScroll + visibleCount; ++i) {
            UIRect fileRect = {pickerX + 10.0f, fileY, pickerW - 20.0f, uiScaleY(26.0f)};
            glm::vec4 fileBg = (i % 2 == 0) ? glm::vec4{0.12f, 0.13f, 0.17f, 1.0f} : glm::vec4{0.1f, 0.11f, 0.15f, 1.0f};
            uiDrawRect(fileRect, fileBg, "picker-file");
            uiDrawText(gPickerFiles[i].c_str(), fileRect.x + 5.0f, fileRect.y + 2.0f, 0.22f, {1.0f, 1.0f, 1.0f, 1.0f});

            if (uiButton(win, gPickerFiles[i].c_str(), fileRect, fileBg).clicked) {
                // Assign this file to the slot
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
                    if (pi >= 0 && pi < 6 && fi >= 0 && fi < 6) {
                        avs.setPartFace(kPartKeys[pi], kFaceKeys[fi], gPickerFiles[i]);
                    }
                }
                closePicker();
            }
            fileY += 30.0f;
        }

        // Close button
        UIRect closeRect = {pickerX + pickerW - uiScaleX(30.0f), pickerY + 5.0f, uiScaleX(25.0f), uiScaleY(25.0f)};
        if (uiButton(win, "X", closeRect, {0.5f, 0.15f, 0.15f, 1.0f}).clicked) {
            closePicker();
        }
    }

    // Bottom buttons
    float btnY = std::max(currentY, (float)fbH - 100.0f);
    if (gOpenPickerSlot < 0) {
        UIRect saveRect = {uiScaleX(50.0f), uiScaleY(btnY), uiScaleX(160.0f), uiScaleY(42.0f)};
        if (uiButton(win, "SAVE", saveRect, {0.2f, 0.55f, 0.3f, 1.0f}).clicked) {
            if (av.hasAvatar()) {
                if (av.current().advancedMode)
                    av.saveAdvanced(av.currentName(), av.current());
                else
                    av.saveSimple(av.currentName(), av.current().simple);
                av.loadAvatar(av.currentName()); // reload to ensure state
                Terminal::instance().addLog("[AVATAR] Saved: " + av.currentName());
                r.goSave = true;
            }
        }

        UIRect applyRect = {uiScaleX(230.0f), uiScaleY(btnY), uiScaleX(160.0f), uiScaleY(42.0f)};
        if (uiButton(win, "APPLY", applyRect, {0.25f, 0.4f, 0.6f, 1.0f}).clicked) {
            r.goApply = true;
        }

        UIRect backRect = {uiScaleX(410.0f), uiScaleY(btnY), uiScaleX(160.0f), uiScaleY(42.0f)};
        if (uiButton(win, "BACK", backRect, {0.55f, 0.2f, 0.2f, 1.0f}).clicked) {
            r.goBack = true;
        }
    }

    return r;
}
