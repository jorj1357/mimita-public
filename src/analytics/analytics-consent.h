#pragma once

struct GLFWwindow;

enum class AnalyticsConsentAction
{
    None,
    Continue,
    ReadMore,
    Disable,
    RequestDeletion,
    Toggle
};

namespace AnalyticsConsent
{
    AnalyticsConsentAction drawFirstLaunchPopup(GLFWwindow* win);
    AnalyticsConsentAction drawSettingsPanel(
        GLFWwindow* win,
        bool analyticsEnabled,
        bool permanentlyDisabled,
        const char* statusMessage);
}
