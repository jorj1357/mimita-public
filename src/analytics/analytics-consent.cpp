#include "analytics/analytics-consent.h"

#include "gui/ui-system.h"

#include <GLFW/glfw3.h>

namespace {

void line(const char* text, float x, float y, float scale = 0.34f)
{
    uiDrawText(text, uiScaleX(x), uiScaleY(y), scale, {0.86f, 0.90f, 0.96f, 1.0f});
}

}

AnalyticsConsentAction AnalyticsConsent::drawFirstLaunchPopup(GLFWwindow* win)
{
    uiDrawRect({0.0f, 0.0f, uiScreenW(), uiScreenH()}, {0.0f, 0.0f, 0.0f, 0.62f}, "analytics-dim");
    uiDrawRect({uiScaleX(470.0f), uiScaleY(160.0f), uiScaleX(980.0f), uiScaleY(720.0f)},
               {0.055f, 0.065f, 0.085f, 0.98f}, "analytics-popup");
    uiDrawRectOutline({uiScaleX(470.0f), uiScaleY(160.0f), uiScaleX(980.0f), uiScaleY(720.0f)},
                      {0.55f, 0.78f, 1.0f, 0.95f}, "analytics-popup-border");

    uiDrawText("MIMITA ANALYTICS", uiScaleX(610.0f), uiScaleY(225.0f), 0.64f,
               {0.72f, 0.88f, 1.0f, 1.0f});
    line("We collect anonymous gameplay analytics so we can see what people actually use.", 560.0f, 315.0f);
    line("This helps improve onboarding, maps, weapons, movement, retention, and crashes.", 560.0f, 365.0f);
    line("We do not sell analytics data.", 560.0f, 415.0f);
    line("We do not collect passwords, chat contents, or sensitive information.", 560.0f, 465.0f);
    line("You can disable analytics permanently, and you can request deletion at any time.", 560.0f, 515.0f);
    line("More information: mimita.fun/terms/privacy", 560.0f, 565.0f);

    if (uiButton(win, "CONTINUE", {560.0f, 700.0f, 250.0f, 56.0f},
                 {0.18f, 0.46f, 0.28f, 1.0f}, "analytics-continue").clicked)
        return AnalyticsConsentAction::Continue;
    if (uiButton(win, "READ MORE", {835.0f, 700.0f, 250.0f, 56.0f},
                 {0.14f, 0.22f, 0.34f, 1.0f}, "analytics-read-more").clicked)
        return AnalyticsConsentAction::ReadMore;
    if (uiButton(win, "DISABLE", {1110.0f, 700.0f, 250.0f, 56.0f},
                 {0.42f, 0.16f, 0.16f, 1.0f}, "analytics-disable").clicked)
        return AnalyticsConsentAction::Disable;

    return AnalyticsConsentAction::None;
}

AnalyticsConsentAction AnalyticsConsent::drawSettingsPanel(
    GLFWwindow* win,
    bool analyticsEnabled,
    bool permanentlyDisabled,
    const char* statusMessage)
{
    const float x = 1080.0f;
    uiDrawText("ANALYTICS SETTINGS", uiScaleX(x), uiScaleY(776.0f), 0.42f,
               {0.65f, 0.85f, 1.0f, 1.0f});
    uiDrawText(analyticsEnabled ? "Analytics Enabled: ON" : "Analytics Enabled: OFF",
               uiScaleX(x), uiScaleY(824.0f), 0.31f,
               analyticsEnabled ? glm::vec4(0.55f, 1.0f, 0.65f, 1.0f)
                                : glm::vec4(1.0f, 0.62f, 0.55f, 1.0f));

    AnalyticsConsentAction action = AnalyticsConsentAction::None;
    const char* disableLabel = permanentlyDisabled ? "DISABLED" : "DISABLE ANALYTICS";
    if (uiButton(win, disableLabel, {x, 858.0f, 280.0f, 34.0f},
                 permanentlyDisabled ? glm::vec4(0.18f, 0.18f, 0.18f, 1.0f)
                                     : glm::vec4(0.42f, 0.16f, 0.16f, 1.0f),
                 "analytics-settings-disable").clicked && !permanentlyDisabled)
        action = AnalyticsConsentAction::Disable;

    if (uiButton(win, "REQUEST DATA DELETION", {x, 902.0f, 280.0f, 34.0f},
                 {0.18f, 0.22f, 0.32f, 1.0f}, "analytics-settings-delete").clicked)
        action = AnalyticsConsentAction::RequestDeletion;

    if (uiButton(win, "PRIVACY POLICY", {x, 946.0f, 280.0f, 34.0f},
                 {0.14f, 0.24f, 0.34f, 1.0f}, "analytics-settings-privacy").clicked)
        action = AnalyticsConsentAction::ReadMore;

    if (statusMessage && statusMessage[0])
        uiDrawText(statusMessage, uiScaleX(1382.0f), uiScaleY(910.0f), 0.25f,
                   {0.92f, 0.95f, 1.0f, 1.0f});

    return action;
}
