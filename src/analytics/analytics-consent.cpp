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
    line("Help improve Mimita by anonymously sending crash reports and gameplay analytics.", 560.0f, 380.0f);
    line("More info: mimita.fun/terms/privacy", 560.0f, 520.0f);

    if (uiButton(win, "OK, sounds good", {560.0f, 600.0f, 310.0f, 56.0f},
                 {0.18f, 0.46f, 0.28f, 1.0f}, "analytics-accept").clicked)
        return AnalyticsConsentAction::Continue;
    if (uiButton(win, "No thanks", {890.0f, 600.0f, 310.0f, 56.0f},
                 {0.42f, 0.16f, 0.16f, 1.0f}, "analytics-decline").clicked)
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
