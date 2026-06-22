#include "analytics/analytics-events.h"

#include <array>
#include <utility>

namespace {

using EventCategory = std::pair<const char*, const char*>;

constexpr std::array<EventCategory, 23> kEvents{{
    {"account_created", "account"},
    {"login", "account"},
    {"logout", "account"},
    {"failed_login", "errors"},
    {"session_start", "gameplay"},
    {"session_end", "gameplay"},
    {"session_duration", "gameplay"},
    {"jump", "movement"},
    {"dash", "movement"},
    {"air_jump", "movement"},
    {"wall_jump", "movement"},
    {"map_loaded", "engagement"},
    {"map_completed", "engagement"},
    {"weapon_used", "engagement"},
    {"settings_opened", "ui"},
    {"outfit_editor_opened", "ui"},
    {"replay_viewed", "ui"},
    {"first_launch", "retention"},
    {"day_1_return", "retention"},
    {"day_7_return", "retention"},
    {"day_30_return", "retention"},
    {"crash_detected", "errors"},
    {"disconnect", "errors"}
}};

}

const char* AnalyticsEvents::categoryFor(const std::string& eventName)
{
    for (const auto& entry : kEvents)
        if (eventName == entry.first)
            return entry.second;
    return "custom";
}

bool AnalyticsEvents::isKnown(const std::string& eventName)
{
    for (const auto& entry : kEvents)
        if (eventName == entry.first)
            return true;
    return false;
}
