// 07 19 2026, 12 00
/* purpose
* Own the game client's authenticated user state.
* Expose the small account/session API used by menus and gameplay.
* Bridge website account data into the local player profile.
* DOES NOT store raw passwords or own HTTP transport details.
* DOES NOT own account database schema or website routes.
* DOES NOT render account UI directly.
*/

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
    SignedOut,
    CheckingStoredSession,
    RefreshingSession,
    ReadyToSignIn,
    SigningIn,
    SignedIn,
    LoadingAccount,
    SigningOut,
    Failed,
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
    MimitaVip::VipAppearance vipAppearance;
    std::string role;
    std::vector<std::string> achievements;
    bool emailVerified = false;
    std::string createdAt;
    std::string sessionToken;
    GameStats stats;
    json settings;
    json inventory;
    json titles;
    json loadout;
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

    // Account-owned game data
    json getCloudInventory();
    bool saveCloudInventory(const json& inventory);
    json getCloudLoadout();
    bool saveCloudLoadout(const json& loadout);
    json getCloudTitles();
    bool saveCloudTitles(const json& titles);

    // Apply external user info (from AuthController login)
    void applyUserInfo(const GameUserInfo& info);
    void applyBootstrap(const std::string& token, const GameBootstrap& bootstrap);

    // Called by auth-popup after successful code confirm
    // Uses optional userInfo to avoid depending on validateSession for identity data.
    void finishAuth(const std::string& token, const GameUserInfo* userInfo = nullptr, bool persistSession = true);

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
