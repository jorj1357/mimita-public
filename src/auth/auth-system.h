#pragma once

#include <string>
#include <functional>
#include "website/api-client.h"

enum class AuthState
{
    Checking,
    Offline,
    Authenticated,
    NeedsLogin,
    Linking,
    TokenExchange,
};

struct AuthUser
{
    int id = 0;
    std::string username;
    std::string displayName;
    std::string email;
    std::string bio;
    std::string avatarUrl;
    json avatarData;
    std::string supporterTier;
    std::string role;
    std::vector<std::string> achievements;
    bool emailVerified = false;
    std::string createdAt;
    std::string sessionToken;
    GameStats stats;
};

class AuthSystem
{
public:
    static AuthSystem& instance();

    void init(const std::string& cliSessionToken = {});
    void tickValidate();
    AuthState state() const { return mState; }
    const AuthUser& user() const { return mUser; }
    std::string displayName() const;

    void startLinkFlow();
    void pollLinkFlow();
    bool isLinking() const { return mState == AuthState::Linking; }
    const std::string& linkCode() const { return mLinkCode; }
    const std::string& linkError() const { return mLinkError; }

    void startTokenExchange(const std::string& exchangeToken);
    GameUserInfo finishTokenExchange(const std::string& sessionToken);

    void logout();
    void skipLogin();
    void clearSession();

    std::string generateGuestName() const;

    // Profile management
    void refreshProfile();
    bool updateProfile(const std::string& displayName, const std::string& bio);
    bool uploadAvatarData(const json& avatarData);

    // Stats
    void refreshStats();

    // Settings
    json getCloudSettings();
    bool saveCloudSettings(const json& settings);

    // Avatar data
    json getCloudAvatarData();
    bool saveCloudAvatarData(const json& avatarData);

    // Called by auth-popup after successful code confirm
    // Uses optional userInfo to avoid depending on validateSession for identity data.
    void finishAuth(const std::string& token, const GameUserInfo* userInfo = nullptr);

private:
    AuthSystem() = default;
    void validateStoredToken();
    void fetchFullProfile();

    AuthState mState = AuthState::Checking;
    AuthUser mUser;
    std::string mGuestName;
    std::string mLinkCode;
    std::string mGrantToken;
    std::string mLinkError;
    float mLinkPollTimer = 0.0f;
    int mLinkPollAttempts = 0;
    std::string mPendingExchangeToken;
};
