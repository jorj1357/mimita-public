#include "analytics/analytics-config.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

constexpr const char* kConfigPath = "config/analytics.json";

template <typename T>
void readValue(const json& j, const char* key, T& value)
{
    if (j.contains(key))
        value = j[key].get<T>();
}

}

bool AnalyticsConfigStore::load(AnalyticsConfig& config)
{
    std::ifstream input(kConfigPath);
    if (!input.is_open())
        return false;

    try {
        json root;
        input >> root;
        readValue(root, "analytics_enabled", config.analyticsEnabled);
        readValue(root, "consent_shown", config.consentShown);
        readValue(root, "permanently_disabled", config.permanentlyDisabled);
        readValue(root, "first_launch_sent", config.firstLaunchSent);
        readValue(root, "day_1_return_sent", config.day1ReturnSent);
        readValue(root, "day_7_return_sent", config.day7ReturnSent);
        readValue(root, "day_30_return_sent", config.day30ReturnSent);
        readValue(root, "last_shutdown_clean", config.lastShutdownClean);
        readValue(root, "launch_count", config.launchCount);
        readValue(root, "anonymous_id", config.anonymousId);
        readValue(root, "first_launch_date", config.firstLaunchDate);
        readValue(root, "last_launch_date", config.lastLaunchDate);
        readValue(root, "events_endpoint", config.eventsEndpoint);
        readValue(root, "consent_endpoint", config.consentEndpoint);
        readValue(root, "deletion_endpoint", config.deletionEndpoint);
        readValue(root, "privacy_url", config.privacyUrl);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool AnalyticsConfigStore::save(const AnalyticsConfig& config)
{
    std::filesystem::create_directories("config");

    json root;
    root["analytics_enabled"] = config.analyticsEnabled;
    root["consent_shown"] = config.consentShown;
    root["permanently_disabled"] = config.permanentlyDisabled;
    root["first_launch_sent"] = config.firstLaunchSent;
    root["day_1_return_sent"] = config.day1ReturnSent;
    root["day_7_return_sent"] = config.day7ReturnSent;
    root["day_30_return_sent"] = config.day30ReturnSent;
    root["last_shutdown_clean"] = config.lastShutdownClean;
    root["launch_count"] = config.launchCount;
    root["anonymous_id"] = config.anonymousId;
    root["first_launch_date"] = config.firstLaunchDate;
    root["last_launch_date"] = config.lastLaunchDate;
    root["events_endpoint"] = config.eventsEndpoint;
    root["consent_endpoint"] = config.consentEndpoint;
    root["deletion_endpoint"] = config.deletionEndpoint;
    root["privacy_url"] = config.privacyUrl;

    const std::string temporary = std::string(kConfigPath) + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output.is_open())
        return false;
    output << root.dump(2) << '\n';
    output.close();

    std::error_code ec;
    std::filesystem::remove(kConfigPath, ec);
    ec.clear();
    std::filesystem::rename(temporary, kConfigPath, ec);
    return !ec;
}
