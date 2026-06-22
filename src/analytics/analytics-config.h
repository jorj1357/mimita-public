#pragma once

#include <string>

struct AnalyticsConfig
{
    bool analyticsEnabled = true;
    bool consentShown = false;
    bool permanentlyDisabled = false;
    bool firstLaunchSent = false;
    bool day1ReturnSent = false;
    bool day7ReturnSent = false;
    bool day30ReturnSent = false;
    bool lastShutdownClean = true;

    int launchCount = 0;

    std::string anonymousId;
    std::string firstLaunchDate;
    std::string lastLaunchDate;

    std::string eventsEndpoint = "https://mimita.fun/api/game/analytics/events";
    std::string consentEndpoint = "https://mimita.fun/api/game/analytics/consent";
    std::string deletionEndpoint = "https://mimita.fun/api/game/analytics/deletion-request";
    std::string privacyUrl = "https://mimita.fun/terms/privacy";
};

namespace AnalyticsConfigStore
{
    bool load(AnalyticsConfig& config);
    bool save(const AnalyticsConfig& config);
}
