#include "analytics/analytics-manager.h"

#include "analytics/analytics-events.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

std::string dateToday()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buf;
}

std::string isoNow()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_s(&tm, &t);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}



bool parseDate(const std::string& value, std::tm& out)
{
    if (value.size() < 10)
        return false;
    std::istringstream input(value.substr(0, 10));
    input >> std::get_time(&out, "%Y-%m-%d");
    out.tm_hour = 12;
    return !input.fail();
}

int daysBetween(const std::string& from, const std::string& to)
{
    std::tm a{};
    std::tm b{};
    if (!parseDate(from, a) || !parseDate(to, b))
        return 0;
    const std::time_t ta = std::mktime(&a);
    const std::time_t tb = std::mktime(&b);
    if (ta == (std::time_t)-1 || tb == (std::time_t)-1)
        return 0;
    return (int)std::max(0.0, std::difftime(tb, ta) / 86400.0);
}

}

void AnalyticsManager::track(const std::string& eventName, const json& properties)
{
    if (!enabled() || !AnalyticsEvents::isKnown(eventName))
        return;

    json event;
    event["event_name"] = eventName;
    event["event_category"] = AnalyticsEvents::categoryFor(eventName);
    event["anonymous_id"] = mConfig.anonymousId;
    event["session_id"] = mSessionId;
    event["client_event_id"] =
        mSessionId + "-" + std::to_string(++mEventCounter);
    event["username"] = mUsername;
    event["app_version"] = "dev";
    event["occurred_at"] = isoNow();
    event["properties"] = properties.is_object() ? properties : json::object();
    if (!mAccountId.empty())
        event["account_id"] = mAccountId;

    mUploader.enqueue(event);
}

void AnalyticsManager::trackMovement(const std::string& eventName)
{
    track(eventName);
}

void AnalyticsManager::trackMapLoaded(
    const std::string& mapPath,
    int triangleCount,
    int spawnCount)
{
    track("map_loaded", {
        {"map", std::filesystem::path(mapPath).filename().string()},
        {"map_path", mapPath},
        {"triangles", triangleCount},
        {"spawns", spawnCount}
    });
}

void AnalyticsManager::trackWeaponUsed(const std::string& weaponId)
{
    track("weapon_used", {
        {"weapon", weaponId}
    });
}

void AnalyticsManager::trackUi(const std::string& eventName)
{
    track(eventName);
}

void AnalyticsManager::trackDisconnect(const std::string& reason)
{
    track("disconnect", {
        {"reason", reason}
    });
}

void AnalyticsManager::startSessionIfAllowed()
{
    if (mSessionStarted || !enabled())
        return;

    mSessionStarted = true;
    mSessionSeconds = 0.0;

    if (!mConfig.firstLaunchSent) {
        track("first_launch", {
            {"first_launch_date", mConfig.firstLaunchDate}
        });
        mConfig.firstLaunchSent = true;
        save();
    }

    if (!mPreviousShutdownClean) {
        track("crash_detected", {
            {"reason", "previous_run_unclean_shutdown"}
        });
        mPreviousShutdownClean = true;
    }

    sendReturnEvents();
    track("session_start", {
        {"launch_count", mConfig.launchCount}
    });
}

void AnalyticsManager::postConsent()
{
    json body;
    body["anonymous_id"] = mConfig.anonymousId;
    body["username"] = mUsername;
    body["analytics_enabled"] = mConfig.analyticsEnabled;
    body["permanently_disabled"] = mConfig.permanentlyDisabled;
    if (!mAccountId.empty())
        body["account_id"] = mAccountId;
    AnalyticsUploader::postJson(mConfig.consentEndpoint, body);
}

void AnalyticsManager::sendReturnEvents()
{
    const int days = daysBetween(mConfig.firstLaunchDate, dateToday());
    bool changed = false;
    if (days >= 1 && !mConfig.day1ReturnSent) {
        track("day_1_return");
        mConfig.day1ReturnSent = true;
        changed = true;
    }
    if (days >= 7 && !mConfig.day7ReturnSent) {
        track("day_7_return");
        mConfig.day7ReturnSent = true;
        changed = true;
    }
    if (days >= 30 && !mConfig.day30ReturnSent) {
        track("day_30_return");
        mConfig.day30ReturnSent = true;
        changed = true;
    }
    if (changed)
        save();
}
