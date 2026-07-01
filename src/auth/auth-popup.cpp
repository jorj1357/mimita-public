#include "auth/auth-popup.h"
#include "auth/auth-system.h"
#include "auth/auth-token.h"
#include "website/api-client.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"

#include <cstdio>
#include <cctype>
#include <string>
#include <shellapi.h>

namespace {

enum class CodeFlowState
{
    None,
    CodeInput,
    Preview,
};

CodeFlowState gFlowState = CodeFlowState::None;
std::string gCode;
std::string gCodeMessage;
ClientCodePreview gPreview;
bool gConfirming = false;
bool gConfirmed = false;

}

void authPopupReset()
{
    gFlowState = CodeFlowState::None;
    gCode.clear();
    gCodeMessage.clear();
    gPreview = {};
    gConfirming = false;
    gConfirmed = false;
}

bool authPopupIsInCodeInput()
{
    return gFlowState == CodeFlowState::CodeInput;
}

void authPopupStartCodeInput()
{
    gFlowState = CodeFlowState::CodeInput;
    gCode.clear();
    gCodeMessage.clear();
    gPreview = {};
    gConfirming = false;
    gConfirmed = false;
    printf("[AUTH] code input started\n");
}

void authPopupHandleChar(unsigned int codepoint)
{
    if (gFlowState != CodeFlowState::CodeInput) return;
    if (gConfirmed) return;

    char c = (char)std::toupper((int)codepoint);
    if (c >= 'A' && c <= 'Z' && gCode.size() < 4)
    {
        gCode.push_back(c);
        gCodeMessage.clear();
    }
}

void authPopupHandleKey(int key, int action)
{
    if (gFlowState != CodeFlowState::CodeInput) return;
    if (gConfirmed) return;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE)
    {
        authPopupReset();
    }
    else if (key == GLFW_KEY_BACKSPACE && !gCode.empty())
    {
        gCode.pop_back();
        gCodeMessage.clear();
    }
    else if (key == GLFW_KEY_ENTER && gCode.size() == 4)
    {
        printf("[AUTH] previewing code: %s\n", gCode.c_str());
        gFlowState = CodeFlowState::Preview;
        gPreview = previewClientCode(gCode);
        if (!gPreview.valid)
        {
            gCodeMessage = gPreview.username.empty()
                ? "Invalid or expired code. Try again."
                : "Code not recognized.";
            gFlowState = CodeFlowState::CodeInput;
        }
    }
}

AuthPopupAction drawAuthPopup(GLFWwindow* window)
{
    float fbW = uiScreenW();
    float fbH = uiScreenH();
    AuthSystem& auth = AuthSystem::instance();
    AuthPopupAction result = AuthPopupAction::None;

    uiDrawRect({0, 0, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.55f}, "auth-dim");

    float popupW = uiScaleX(700.0f);
    float popupH = uiScaleY(520.0f);
    float popupX = (fbW - popupW) * 0.5f;
    float popupY = (fbH - popupH) * 0.5f;

    uiDrawRect({popupX, popupY, popupW, popupH},
               {0.06f, 0.07f, 0.09f, 0.97f}, "auth-popup");
    uiDrawRectOutline({popupX, popupY, popupW, popupH},
                      {0.4f, 0.6f, 0.9f, 0.8f}, "auth-popup-border");

    float centerX = popupX + popupW * 0.5f;

    if (gFlowState == CodeFlowState::None)
    {
        // ── Initial state: description + buttons ─────────────────────
        float titleY = popupY + uiScaleY(60.0f);
        uiDrawText("No Mimita Account Detected",
                   popupX + uiScaleX(40.0f), titleY, 0.55f,
                   {0.85f, 0.9f, 1.0f, 1.0f});

        float descY = titleY + uiScaleY(100.0f);
        const char* desc = "Signing in lets you keep your username,\ncosmetics, stats, and progression across computers.";
        uiDrawText(desc, popupX + uiScaleX(40.0f), descY, 0.30f,
                   {0.7f, 0.75f, 0.85f, 1.0f});

        float noteY = descY + uiScaleY(120.0f);
        uiDrawText("Accounts are optional, but recommended.",
                   popupX + uiScaleX(40.0f), noteY, 0.28f,
                   {0.6f, 0.65f, 0.75f, 1.0f});

        float btnY = popupY + popupH - uiScaleY(180.0f);
        float btnW = uiScaleX(280.0f);
        float btnH = uiScaleY(48.0f);

        if (uiButton(window, "Log In",
                     {centerX - btnW * 0.5f, btnY, btnW, btnH},
                     {0.2f, 0.45f, 0.8f, 1.0f}, "auth-login").clicked)
        {
            printf("[AUTH] popup: Log In clicked\n");
            ShellExecuteA(nullptr, "open",
                "https://mimita.fun/clientsignin",
                nullptr, nullptr, SW_SHOWNORMAL);
            result = AuthPopupAction::LogIn;
        }

        if (uiButton(window, "Create Account",
                     {centerX - btnW * 0.5f, btnY + btnH + uiScaleY(10.0f), btnW, btnH},
                     {0.18f, 0.4f, 0.22f, 1.0f}, "auth-create").clicked)
        {
            printf("[AUTH] popup: Create Account clicked\n");
            ShellExecuteA(nullptr, "open",
                "https://mimita.fun/signup",
                nullptr, nullptr, SW_SHOWNORMAL);
            result = AuthPopupAction::CreateAccount;
        }

        if (uiButton(window, "Enter Sign In Code",
                     {centerX - btnW * 0.5f, btnY + (btnH + uiScaleY(10.0f)) * 2, btnW, btnH},
                     {0.25f, 0.3f, 0.45f, 1.0f}, "auth-enter-code").clicked)
        {
            printf("[AUTH] popup: Enter Sign In Code clicked\n");
            gFlowState = CodeFlowState::CodeInput;
            gCode.clear();
            gCodeMessage.clear();
        }

        float skipBtnY = btnY + (btnH + uiScaleY(10.0f)) * 3 + uiScaleY(12.0f);
        if (uiButton(window, "Continue Offline",
                     {centerX - btnW * 0.5f, skipBtnY, btnW, uiScaleY(40.0f)},
                     {0.25f, 0.25f, 0.3f, 1.0f}, "auth-skip").clicked)
        {
            printf("[AUTH] popup: Continue Offline clicked\n");
            result = AuthPopupAction::ContinueOffline;
        }
    }
    else if (gFlowState == CodeFlowState::CodeInput && !gConfirmed)
    {
        // ── Code input state ─────────────────────────────────────────
        float titleY = popupY + uiScaleY(50.0f);
        uiDrawText("Enter 4-Letter Code",
                   popupX + uiScaleX(40.0f), titleY, 0.50f,
                   {0.85f, 0.9f, 1.0f, 1.0f});

        float descY = titleY + uiScaleY(60.0f);
        uiDrawText("Go to mimita.fun/clientsignin in your browser,\nsign in, and enter the code shown.",
                   popupX + uiScaleX(40.0f), descY, 0.28f,
                   {0.7f, 0.75f, 0.85f, 1.0f});

        // Code input boxes
        float boxY = descY + uiScaleY(90.0f);
        float boxSize = uiScaleX(64.0f);
        float gap = uiScaleX(16.0f);
        float totalW = boxSize * 4 + gap * 3;
        float boxStartX = centerX - totalW * 0.5f;

        for (int i = 0; i < 4; i++)
        {
            float bx = boxStartX + (boxSize + gap) * i;
            glm::vec4 bg = (i < (int)gCode.size())
                ? glm::vec4(0.2f, 0.45f, 0.8f, 1.0f)
                : glm::vec4(0.12f, 0.13f, 0.16f, 1.0f);
            uiDrawRect({bx, boxY, boxSize, boxSize}, bg, "auth-code-box");
            uiDrawRectOutline({bx, boxY, boxSize, boxSize},
                              {0.3f, 0.4f, 0.55f, 0.5f}, "auth-code-border");

            if (i < (int)gCode.size())
            {
                char letter[2] = {gCode[i], '\0'};
                float lw = uiMeasureText(letter, 0.50f);
                uiDrawText(letter, bx + (boxSize - lw) * 0.5f,
                           boxY + (boxSize - uiScaleY(50.0f)) * 0.5f,
                           0.50f, {1.0f, 1.0f, 1.0f, 1.0f});
            }
        }

        // Hint
        float hintY = boxY + boxSize + uiScaleY(16.0f);
        uiDrawText("Type the 4-letter code from the website.",
                   centerX - uiMeasureText("Type the 4-letter code from the website.", 0.24f) * 0.5f,
                   hintY, 0.24f, {0.6f, 0.65f, 0.75f, 0.7f});

        // Back button
        float backBtnY = hintY + uiScaleY(40.0f);
        if (uiButton(window, "Back",
                     {centerX - uiScaleX(120.0f), backBtnY, uiScaleX(240.0f), uiScaleY(40.0f)},
                     {0.25f, 0.25f, 0.3f, 1.0f}, "auth-code-back").clicked)
        {
            gFlowState = CodeFlowState::None;
            gCode.clear();
            gCodeMessage.clear();
        }

        // Error message
        if (!gCodeMessage.empty())
        {
            float errW = uiMeasureText(gCodeMessage.c_str(), 0.28f);
            uiDrawText(gCodeMessage.c_str(), centerX - errW * 0.5f,
                       backBtnY + uiScaleY(50.0f), 0.28f,
                       {1.0f, 0.3f, 0.3f, 1.0f});
        }
    }
    else if (gFlowState == CodeFlowState::Preview && !gConfirmed)
    {
        // ── Preview state: "Is this you?" ────────────────────────────
        float titleY = popupY + uiScaleY(50.0f);
        uiDrawText("Is this you?",
                   popupX + uiScaleX(40.0f), titleY, 0.50f,
                   {0.85f, 0.9f, 1.0f, 1.0f});

        // Username
        float nameY = titleY + uiScaleY(80.0f);
        std::string display = gPreview.displayName.empty()
            ? gPreview.username : gPreview.displayName;
        float nameW = uiMeasureText(display.c_str(), 0.55f);
        uiDrawText(display.c_str(), centerX - nameW * 0.5f, nameY, 0.55f,
                   {0.9f, 0.95f, 1.0f, 1.0f});

        // Username label
        float unameY = nameY + uiScaleY(60.0f);
        std::string unameStr = "@" + gPreview.username;
        float unameW = uiMeasureText(unameStr.c_str(), 0.30f);
        uiDrawText(unameStr.c_str(), centerX - unameW * 0.5f, unameY, 0.30f,
                   {0.6f, 0.65f, 0.75f, 0.7f});

        // Avatar placeholder
        float avatarY = unameY + uiScaleY(50.0f);
        float avatarS = uiScaleX(96.0f);
        uiDrawRect({centerX - avatarS * 0.5f, avatarY, avatarS, avatarS},
                   {0.15f, 0.18f, 0.22f, 1.0f}, "auth-avatar-placeholder");
        char initial[2] = {gPreview.username.empty() ? '?' : (char)std::toupper(gPreview.username[0]), '\0'};
        float iw = uiMeasureText(initial, 0.55f);
        uiDrawText(initial, centerX - iw * 0.5f, avatarY + (avatarS - uiScaleY(50.0f)) * 0.5f,
                   0.55f, {0.4f, 0.45f, 0.55f, 1.0f});

        // Buttons
        float btnY = avatarY + avatarS + uiScaleY(40.0f);
        float btnW = uiScaleX(220.0f);
        float btnH = uiScaleY(50.0f);

        if (gConfirming)
        {
            uiDrawText("Linking...",
                       centerX - uiMeasureText("Linking...", 0.34f) * 0.5f,
                       btnY + uiScaleY(10.0f), 0.34f,
                       {0.6f, 0.65f, 0.75f, 1.0f});
        }
        else
        {
            if (uiButton(window, "Yes, link account",
                         {centerX - btnW * 0.5f, btnY, btnW, btnH},
                         {0.2f, 0.6f, 0.3f, 1.0f}, "auth-confirm-yes").clicked)
            {
                printf("[AUTH] confirming code: %s\n", gCode.c_str());
                gConfirming = true;
                ClientCodeConfirm confirm = confirmClientCode(gCode);
                if (confirm.success)
                {
                    printf("[AUTH] code confirmed, session: %s\n", confirm.sessionToken.c_str());
                    GameUserInfo confirmInfo;
                    confirmInfo.valid = true;
                    confirmInfo.id = confirm.accountId;
                    confirmInfo.username = confirm.username;
                    confirmInfo.displayName = confirm.username;
                    auth.finishAuth(confirm.sessionToken, &confirmInfo);
                    gConfirmed = true;
                    gFlowState = CodeFlowState::None;
                }
                else
                {
                    gCodeMessage = "Failed to link account. Try again.";
                    gConfirming = false;
                    gFlowState = CodeFlowState::CodeInput;
                    gCode.clear();
                }
            }

            float noBtnY = btnY + btnH + uiScaleY(12.0f);
            if (uiButton(window, "No",
                         {centerX - btnW * 0.5f, noBtnY, btnW, btnH * 0.7f},
                         {0.4f, 0.15f, 0.15f, 1.0f}, "auth-confirm-no").clicked)
            {
                printf("[AUTH] declined link\n");
                gFlowState = CodeFlowState::CodeInput;
                gCode.clear();
                gCodeMessage.clear();
                gPreview = {};
            }
        }
    }

    return result;
}

bool drawAuthCodeDialog(GLFWwindow* window)
{
    if (gFlowState == CodeFlowState::None)
        return false;

    float fbW = uiScreenW();
    float fbH = uiScreenH();
    AuthSystem& auth = AuthSystem::instance();

    uiDrawRect({0, 0, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.55f}, "auth-code-dim");

    float popupW = uiScaleX(660.0f);
    float popupH = uiScaleY(460.0f);
    float popupX = (fbW - popupW) * 0.5f;
    float popupY = (fbH - popupH) * 0.5f;
    float centerX = popupX + popupW * 0.5f;

    uiDrawRect({popupX, popupY, popupW, popupH},
               {0.06f, 0.07f, 0.09f, 0.97f}, "auth-code-popup");
    uiDrawRectOutline({popupX, popupY, popupW, popupH},
                      {0.4f, 0.6f, 0.9f, 0.8f}, "auth-code-border");

    if (gFlowState == CodeFlowState::CodeInput && !gConfirmed)
    {
        float titleY = popupY + uiScaleY(50.0f);
        uiDrawText("Enter 4-Letter Code",
                   popupX + uiScaleX(40.0f), titleY, 0.50f,
                   {0.85f, 0.9f, 1.0f, 1.0f});

        float descY = titleY + uiScaleY(60.0f);
        uiDrawText("Go to mimita.fun/clientsignin in your browser,\nsign in, and enter the code shown.",
                   popupX + uiScaleX(40.0f), descY, 0.28f,
                   {0.7f, 0.75f, 0.85f, 1.0f});

        float boxY = descY + uiScaleY(90.0f);
        float boxSize = uiScaleX(64.0f);
        float gap = uiScaleX(16.0f);
        float totalW = boxSize * 4 + gap * 3;
        float boxStartX = centerX - totalW * 0.5f;

        for (int i = 0; i < 4; i++)
        {
            float bx = boxStartX + (boxSize + gap) * i;
            glm::vec4 bg = (i < (int)gCode.size())
                ? glm::vec4(0.2f, 0.45f, 0.8f, 1.0f)
                : glm::vec4(0.12f, 0.13f, 0.16f, 1.0f);
            uiDrawRect({bx, boxY, boxSize, boxSize}, bg, "auth-code-box");
            uiDrawRectOutline({bx, boxY, boxSize, boxSize},
                              {0.3f, 0.4f, 0.55f, 0.5f}, "auth-code-border");

            if (i < (int)gCode.size())
            {
                char letter[2] = {gCode[i], '\0'};
                float lw = uiMeasureText(letter, 0.50f);
                uiDrawText(letter, bx + (boxSize - lw) * 0.5f,
                           boxY + (boxSize - uiScaleY(50.0f)) * 0.5f,
                           0.50f, {1.0f, 1.0f, 1.0f, 1.0f});
            }
        }

        float hintY = boxY + boxSize + uiScaleY(16.0f);
        uiDrawText("Type the 4-letter code from the website.",
                   centerX - uiMeasureText("Type the 4-letter code from the website.", 0.24f) * 0.5f,
                   hintY, 0.24f, {0.6f, 0.65f, 0.75f, 0.7f});

        float backBtnY = hintY + uiScaleY(40.0f);
        if (uiButton(window, "Cancel",
                     {centerX - uiScaleX(120.0f), backBtnY, uiScaleX(240.0f), uiScaleY(40.0f)},
                     {0.35f, 0.15f, 0.15f, 1.0f}, "auth-code-cancel").clicked)
        {
            authPopupReset();
        }



        if (!gCodeMessage.empty())
        {
            float errW = uiMeasureText(gCodeMessage.c_str(), 0.28f);
            uiDrawText(gCodeMessage.c_str(), centerX - errW * 0.5f,
                       backBtnY + uiScaleY(50.0f), 0.28f,
                       {1.0f, 0.3f, 0.3f, 1.0f});
        }

        // When added by user from main menu, also show a message
        uiDrawText("Paste or type the 4-letter code, then press Enter.",
                   centerX - uiMeasureText("Paste or type the 4-letter code, then press Enter.", 0.20f) * 0.5f,
                   popupY + popupH - uiScaleY(30.0f), 0.20f,
                   {0.5f, 0.55f, 0.65f, 0.6f});
    }
    else if (gFlowState == CodeFlowState::Preview && !gConfirmed)
    {
        float titleY = popupY + uiScaleY(50.0f);
        uiDrawText("Is this you?",
                   popupX + uiScaleX(40.0f), titleY, 0.50f,
                   {0.85f, 0.9f, 1.0f, 1.0f});

        float nameY = titleY + uiScaleY(80.0f);
        std::string display = gPreview.displayName.empty()
            ? gPreview.username : gPreview.displayName;
        float nameW = uiMeasureText(display.c_str(), 0.55f);
        uiDrawText(display.c_str(), centerX - nameW * 0.5f, nameY, 0.55f,
                   {0.9f, 0.95f, 1.0f, 1.0f});

        float unameY = nameY + uiScaleY(60.0f);
        std::string unameStr = "@" + gPreview.username;
        float unameW = uiMeasureText(unameStr.c_str(), 0.30f);
        uiDrawText(unameStr.c_str(), centerX - unameW * 0.5f, unameY, 0.30f,
                   {0.6f, 0.65f, 0.75f, 0.7f});

        float avatarY = unameY + uiScaleY(50.0f);
        float avatarS = uiScaleX(96.0f);
        uiDrawRect({centerX - avatarS * 0.5f, avatarY, avatarS, avatarS},
                   {0.15f, 0.18f, 0.22f, 1.0f}, "auth-avatar-preview");
        char initial[2] = {gPreview.username.empty() ? '?' : (char)std::toupper(gPreview.username[0]), '\0'};
        float iw = uiMeasureText(initial, 0.55f);
        uiDrawText(initial, centerX - iw * 0.5f,
                   avatarY + (avatarS - uiScaleY(50.0f)) * 0.5f,
                   0.55f, {0.4f, 0.45f, 0.55f, 1.0f});

        float btnY = avatarY + avatarS + uiScaleY(40.0f);
        float btnW = uiScaleX(220.0f);
        float btnH = uiScaleY(50.0f);

        if (gConfirming)
        {
            uiDrawText("Linking...",
                       centerX - uiMeasureText("Linking...", 0.34f) * 0.5f,
                       btnY + uiScaleY(10.0f), 0.34f,
                       {0.6f, 0.65f, 0.75f, 1.0f});
        }
        else
        {
            if (uiButton(window, "Yes, link account",
                         {centerX - btnW * 0.5f, btnY, btnW, btnH},
                         {0.2f, 0.6f, 0.3f, 1.0f}, "auth-confirm-yes").clicked)
            {
                printf("[AUTH] confirming code: %s\n", gCode.c_str());
                gConfirming = true;
                ClientCodeConfirm confirm = confirmClientCode(gCode);
                if (confirm.success)
                {
                    printf("[AUTH] code confirmed\n");
                    GameUserInfo confirmInfo;
                    confirmInfo.valid = true;
                    confirmInfo.id = confirm.accountId;
                    confirmInfo.username = confirm.username;
                    confirmInfo.displayName = confirm.username;
                    auth.finishAuth(confirm.sessionToken, &confirmInfo);
                    gConfirmed = true;
                    gFlowState = CodeFlowState::None;
                }
                else
                {
                    gCodeMessage = "Failed to link account. Try again.";
                    gConfirming = false;
                    gFlowState = CodeFlowState::CodeInput;
                    gCode.clear();
                }
            }

            float noBtnY = btnY + btnH + uiScaleY(12.0f);
            if (uiButton(window, "No",
                         {centerX - btnW * 0.5f, noBtnY, btnW, btnH * 0.7f},
                         {0.4f, 0.15f, 0.15f, 1.0f}, "auth-confirm-no").clicked)
            {
                printf("[AUTH] declined link\n");
                authPopupReset();
            }
        }
    }

    return true;
}
