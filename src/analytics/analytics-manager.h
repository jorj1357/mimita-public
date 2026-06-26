#pragma once

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "analytics/analytics-config.h"
#include "analytics/analytics-uploader.h"

struct GLFWwindow;

class AnalyticsManager
{
public:
    static AnalyticsManager& instance();

    void init(const std::string& username);
    void shutdown();
    void update(float dt);
    void setUser(const std::string& username);
    void registerCommands();

    void drawFirstLaunchPopup(GLFWwindow* win);
    void drawSettingsPanel(GLFWwindow* win);

    void track(const std::string& eventName, const nlohmann::json& properties = nlohmann::json::object());
    void trackMovement(const std::string& eventName);
    void trackMapLoaded(const std::string& mapPath, int triangleCount, int spawnCount);
    void trackWeaponUsed(const std::string& weaponId);
    void trackUi(const std::string& eventName);
    void trackDisconnect(const std::string& reason);
    void requestDataDeletion();
    void setAnalyticsEnabled(bool enabled);
    void disablePermanently();
    void openPrivacyPolicy() const;

    bool enabled() const;

private:
    AnalyticsManager() = default;

    void startSessionIfAllowed();
    void postConsent();
    void sendReturnEvents();
    void save();

    AnalyticsConfig mConfig;
    AnalyticsUploader mUploader;
    bool mInitialized = false;
    bool mSessionStarted = false;
    bool mShowFirstLaunchPopup = false;
    bool mPreviousShutdownClean = true;
    int mEventCounter = 0;
    double mSessionSeconds = 0.0;
    std::string mSessionId;
    std::string mUsername;
    std::string mAccountId;
    std::string mEmail;
    std::string mStatusMessage;
};
