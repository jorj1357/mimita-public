#include "analytics/analytics-manager.h"

#include "analytics/analytics-consent.h"
#include "analytics/analytics-events.h"
#include "devtools/terminal.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <windows.h>

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

std::string randomHex(size_t bytes)
{
    std::random_device rd;
    std::mt19937_64 rng(((uint64_t)rd() << 32) ^ (uint64_t)std::time(nullptr));
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes; i += 8)
        out << std::setw(16) << rng();
    std::string text = out.str();
    return text.substr(0, bytes * 2);
}

bool readJsonFile(const char* path, json& out)
{
    std::ifstream input(path);
    if (!input.is_open())
        return false;
    try {
        input >> out;
        return true;
    }
    catch (...) {
        return false;
    }
}

std::string valueAsString(const json& value)
{
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_number_integer())
        return std::to_string(value.get<long long>());
    return "";
}

void loadProfileHints(
    const std::string& username,
    std::string& accountId,
    std::string& email)
{
    json current;
    if (readJsonFile("config/current-profile.json", current)) {
        if (accountId.empty())
            accountId = valueAsString(current.value("account_id", json{}));
        if (accountId.empty())
            accountId = valueAsString(current.value("accountId", json{}));
        if (email.empty())
            email = current.value("email", "");
    }

    json root;
    if (!readJsonFile("config/profiles.json", root))
        return;
    const json profiles = root.value("profiles", json::array());
    for (const json& profile : profiles) {
        if (profile.value("username", "") != username)
            continue;
        if (accountId.empty())
            accountId = valueAsString(profile.value("account_id", json{}));
        if (accountId.empty())
            accountId = valueAsString(profile.value("accountId", json{}));
        if (email.empty())
            email = profile.value("email", "");
        break;
    }
}

}

AnalyticsManager& AnalyticsManager::instance()
{
    static AnalyticsManager manager;
    return manager;
}

void AnalyticsManager::init(const std::string& username)
{
    if (mInitialized)
        return;

    AnalyticsConfigStore::load(mConfig);
    if (mConfig.anonymousId.empty())
        mConfig.anonymousId = "mimita-" + randomHex(12);
    if (mConfig.firstLaunchDate.empty())
        mConfig.firstLaunchDate = dateToday();

    mPreviousShutdownClean = mConfig.lastShutdownClean;
    mConfig.lastShutdownClean = false;
    mSessionId = "session-" + randomHex(10);
    mConfig.launchCount += 1;
    mConfig.lastLaunchDate = dateToday();
    mInitialized = true;
    setUser(username);
    save();

    mShowFirstLaunchPopup = !mConfig.consentShown && !mConfig.permanentlyDisabled;
    if (!mShowFirstLaunchPopup)
        startSessionIfAllowed();
}

void AnalyticsManager::shutdown()
{
    if (!mInitialized)
        return;

    if (mSessionStarted) {
        track("session_end", {{"duration_seconds", mSessionSeconds}});
        track("session_duration", {{"duration_seconds", mSessionSeconds}});
    }
    mConfig.lastShutdownClean = true;
    mUploader.flush(mConfig.eventsEndpoint, true);
    save();
    mSessionStarted = false;
}

void AnalyticsManager::update(float dt)
{
    if (!mInitialized)
        return;
    if (mSessionStarted)
        mSessionSeconds += std::max(0.0f, dt);
    if (enabled())
        mUploader.update(dt, mConfig.eventsEndpoint);
}

void AnalyticsManager::setUser(const std::string& username)
{
    mUsername = username;
    loadProfileHints(username, mAccountId, mEmail);
}

bool AnalyticsManager::enabled() const
{
    return mInitialized &&
        mConfig.consentShown &&
        mConfig.analyticsEnabled &&
        !mConfig.permanentlyDisabled;
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

void AnalyticsManager::drawFirstLaunchPopup(GLFWwindow* win)
{
    if (!mShowFirstLaunchPopup)
        return;

    switch (AnalyticsConsent::drawFirstLaunchPopup(win)) {
        case AnalyticsConsentAction::Continue:
            mConfig.consentShown = true;
            mConfig.analyticsEnabled = true;
            mConfig.permanentlyDisabled = false;
            mShowFirstLaunchPopup = false;
            save();
            postConsent();
            startSessionIfAllowed();
            mStatusMessage = "analytics enabled";
            break;
        case AnalyticsConsentAction::ReadMore:
            openPrivacyPolicy();
            break;
        case AnalyticsConsentAction::Disable:
            disablePermanently();
            mShowFirstLaunchPopup = false;
            break;
        default:
            break;
    }
}

void AnalyticsManager::drawSettingsPanel(GLFWwindow* win)
{
    switch (AnalyticsConsent::drawSettingsPanel(
        win,
        enabled(),
        mConfig.permanentlyDisabled,
        mStatusMessage.c_str())) {
        case AnalyticsConsentAction::Disable:
            disablePermanently();
            break;
        case AnalyticsConsentAction::ReadMore:
            openPrivacyPolicy();
            break;
        case AnalyticsConsentAction::RequestDeletion:
            requestDataDeletion();
            break;
        default:
            break;
    }
}

void AnalyticsManager::disablePermanently()
{
    mConfig.consentShown = true;
    mConfig.analyticsEnabled = false;
    mConfig.permanentlyDisabled = true;
    save();
    postConsent();
    mStatusMessage = "analytics disabled permanently";
    std::printf("[ANALYTICS] disabled permanently\n");
}

void AnalyticsManager::requestDataDeletion()
{
    json body;
    body["anonymous_id"] = mConfig.anonymousId;
    body["username"] = mUsername;
    body["email"] = mEmail;
    if (!mAccountId.empty())
        body["account_id"] = mAccountId;

    const bool ok = AnalyticsUploader::postJson(mConfig.deletionEndpoint, body);
    mStatusMessage = ok
        ? "deletion request sent"
        : "deletion request failed";
    std::printf("[ANALYTICS] deletion_request %s\n", ok ? "sent" : "failed");
}

void AnalyticsManager::openPrivacyPolicy() const
{
    ShellExecuteA(nullptr, "open", mConfig.privacyUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void AnalyticsManager::registerCommands()
{
    Terminal::instance().registerCommand({
        "analytics_status",
        "Show analytics status",
        "analytics_status",
        [](const std::vector<std::string>&) {
            const AnalyticsManager& a = AnalyticsManager::instance();
            Terminal::instance().addLog(
                std::string("[ANALYTICS] ") + (a.enabled() ? "enabled" : "disabled"));
        },
        "2026-06-22",
        CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "analytics_disable",
        "Disable analytics permanently",
        "analytics_disable",
        [](const std::vector<std::string>&) {
            AnalyticsManager::instance().disablePermanently();
            Terminal::instance().addLog("[ANALYTICS] disabled permanently");
        },
        "2026-06-22",
        CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "analytics_request_delete",
        "Request deletion of analytics data",
        "analytics_request_delete",
        [](const std::vector<std::string>&) {
            AnalyticsManager::instance().requestDataDeletion();
            Terminal::instance().addLog("[ANALYTICS] deletion request submitted");
        },
        "2026-06-22",
        CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "analytics_flush",
        "Upload queued analytics events now",
        "analytics_flush",
        [](const std::vector<std::string>&) {
            AnalyticsManager::instance().mUploader.flush(
                AnalyticsManager::instance().mConfig.eventsEndpoint, true);
            Terminal::instance().addLog("[ANALYTICS] flush complete");
        },
        "2026-06-22",
        CommandCategory::Debug
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

void AnalyticsManager::save()
{
    AnalyticsConfigStore::save(mConfig);
}
