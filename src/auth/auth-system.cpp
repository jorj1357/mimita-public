#include "auth/auth-system.h"
#include "auth/auth-token.h"
#include "website/api-client.h"
#include "debug/debug-log.h"

#include <cstdlib>
#include <random>
#include <shellapi.h>
#include "entities/player.h"

extern Player* gpPlayer;

AuthSystem& AuthSystem::instance()
{
    static AuthSystem system;
    return system;
}

static void applyInfo(AuthUser& user, const GameUserInfo& info)
{
    user.id = info.id;
    user.username = info.username;
    user.displayName = info.displayName;
    user.email = info.email;
    user.bio = info.bio;
    user.avatarUrl = info.avatarUrl;
    user.avatarData = info.avatarData;
    user.supporterTier = info.supporterTier;
    user.role = info.role;
    user.emailVerified = info.emailVerified;
    user.createdAt = info.createdAt;
}

static void cacheAndFinish(AuthUser& user)
{
    CachedProfile cache;
    cache.id = user.id;
    cache.username = user.username;
    cache.displayName = user.displayName;
    cache.avatarUrl = user.avatarUrl;
    cache.supporterTier = user.supporterTier;
    storeProfileCache(cache);
}

void AuthSystem::init(const std::string& cliSessionToken)
{
    mGuestName = generateGuestName();
    mState = AuthState::Checking;
    Debug::warn(Debug::Category::Auth, "checking for stored session...\n");

    if (!cliSessionToken.empty())
    {
        Debug::warn(Debug::Category::Auth, "received session token from command line\n");
        mUser.sessionToken = cliSessionToken;
        storeSessionToken(cliSessionToken);
        return;
    }

    std::string stored = loadSessionToken();
    if (stored.empty())
    {
        Debug::warn(Debug::Category::Auth, "no stored session found\n");
        mState = AuthState::NeedsLogin;
        return;
    }

    mUser.sessionToken = stored;
    Debug::log(Debug::Category::Auth, "stored session token found, loading cached profile\n");

    CachedProfile cached = loadProfileCache();
    if (!cached.username.empty())
    {
        mUser.id = cached.id;
        mUser.username = cached.username;
        mUser.displayName = cached.displayName;
        mUser.avatarUrl = cached.avatarUrl;
        mUser.supporterTier = cached.supporterTier;
        Debug::warn(Debug::Category::Auth, "using cached profile: %s\n", cached.username.c_str());
    }
}

void AuthSystem::tickValidate()
{
    if (mState != AuthState::Checking)
        return;
    if (mUser.sessionToken.empty())
    {
        mState = AuthState::NeedsLogin;
        return;
    }
    validateStoredToken();
}

void AuthSystem::validateStoredToken()
{
    GameUserInfo info = validateSession(mUser.sessionToken);
    if (info.valid)
    {
        mState = AuthState::Authenticated;
        if (!info.username.empty()) {
            applyInfo(mUser, info);
            Debug::warn(Debug::Category::Auth,
                "session restored: username=%s displayName=%s userId=%d\n",
                info.username.c_str(), info.displayName.c_str(), info.id);
        } else {
            Debug::warn(Debug::Category::Auth,
                "server returned empty user data, preserving cached profile: %s\n",
                mUser.username.c_str());
        }
        if (gpPlayer)
            gpPlayer->username = displayName();
        cacheAndFinish(mUser);
        refreshStats();
    }
    else
    {
        Debug::warn(Debug::Category::Auth, "stored session invalid or unreachable, clearing\n");
        clearSessionToken();
        clearProfileCache();
        mUser.sessionToken.clear();
        mState = AuthState::NeedsLogin;
    }
}

void AuthSystem::fetchFullProfile()
{
    GameUserInfo info = getProfile(mUser.sessionToken);
    if (info.valid && !info.username.empty())
    {
        applyInfo(mUser, info);
        Debug::log(Debug::Category::Auth,
            "profile refreshed: username=%s displayName=%s\n",
            info.username.c_str(), info.displayName.c_str());
    }
    else if (info.valid)
    {
        Debug::log(Debug::Category::Auth, "profile refresh returned empty data, preserving current\n");
    }
}

void AuthSystem::refreshProfile()
{
    if (mState != AuthState::Authenticated) return;
    fetchFullProfile();
}

bool AuthSystem::updateProfile(const std::string& displayName, const std::string& bio)
{
    if (mState != AuthState::Authenticated) return false;
    json updates;
    if (!displayName.empty()) updates["display_name"] = displayName;
    if (!bio.empty()) updates["bio"] = bio;
    bool ok = ::updateProfile(mUser.sessionToken, updates);
    if (ok) fetchFullProfile();
    return ok;
}

void AuthSystem::refreshStats()
{
    if (mState != AuthState::Authenticated) return;
    if (mUser.sessionToken.empty()) return;
    mUser.stats = getStats(mUser.sessionToken);
    Debug::log(Debug::Category::Auth, "stats refreshed: mmr=%d wins=%d\n",
               mUser.stats.currentMmr, mUser.stats.wins);
}

std::string AuthSystem::displayName() const
{
    if (!mUser.displayName.empty())
        return mUser.displayName;
    if (!mUser.username.empty())
        return mUser.username;
    return mGuestName;
}

std::string AuthSystem::generateGuestName() const
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1000, 9999);
    return "Guest" + std::to_string(dist(gen));
}

void AuthSystem::startLinkFlow()
{
    mLinkError.clear();
    LinkCodeResult link = requestAuthLink();
    if (!link.ok)
    {
        mLinkError = "Failed to reach authentication server.";
        Debug::warn(Debug::Category::Auth, "link code request failed\n");
        return;
    }

    mState = AuthState::Linking;
    mLinkCode = link.code;
    mGrantToken = link.grantToken;
    mLinkPollTimer = 0.0f;
    mLinkPollAttempts = 0;

    std::string url = "https://mimita.fun/signin?redirect=/link";
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    Debug::warn(Debug::Category::Auth, "opened browser for sign in, code=%s\n", mLinkCode.c_str());
}

void AuthSystem::pollLinkFlow()
{
    if (mState != AuthState::Linking) return;
    if (mLinkCode.empty() || mGrantToken.empty()) return;

    mLinkPollTimer += 1.0f / 60.0f;
    if (mLinkPollTimer < 2.0f) return;
    mLinkPollTimer = 0.0f;

    mLinkPollAttempts++;
    if (mLinkPollAttempts > 150)
    {
        Debug::warn(Debug::Category::Auth, "link polling timed out\n");
        mState = AuthState::NeedsLogin;
        mLinkError = "Timed out waiting for authentication.";
        return;
    }

    bool claimed = pollLinkStatus(mLinkCode);
    if (!claimed) return;

    Debug::log(Debug::Category::Auth, "link code claimed, finalizing...\n");
    std::string sessionToken = finalizeLink(mLinkCode, mGrantToken);
    if (sessionToken.empty())
    {
        Debug::warn(Debug::Category::Auth, "link finalize failed\n");
        mState = AuthState::NeedsLogin;
        mLinkError = "Authentication finalization failed.";
        return;
    }

    finishAuth(sessionToken);
}

void AuthSystem::startTokenExchange(const std::string& exchangeToken)
{
    mPendingExchangeToken = exchangeToken;
    mState = AuthState::TokenExchange;

    std::string sessionToken = exchangeTempToken(exchangeToken);
    if (sessionToken.empty())
    {
        Debug::warn(Debug::Category::Auth, "token exchange failed\n");
        mState = AuthState::NeedsLogin;
        return;
    }

    finishAuth(sessionToken);
}

GameUserInfo AuthSystem::finishTokenExchange(const std::string& sessionToken)
{
    return validateSession(sessionToken);
}

void AuthSystem::finishAuth(const std::string& token, const GameUserInfo* userInfo)
{
    storeSessionToken(token);
    mUser.sessionToken = token;

    int loggedIn = 0;

    if (userInfo && userInfo->valid && !userInfo->username.empty())
    {
        mState = AuthState::Authenticated;
        applyInfo(mUser, *userInfo);
        loggedIn = 1;

        Debug::warn(Debug::Category::Auth,
            "LOGIN SUCCESS (from confirm) username=%s displayName=%s userId=%d\n",
            userInfo->username.c_str(), userInfo->displayName.c_str(), userInfo->id);

        GameUserInfo serverInfo = validateSession(token);
        if (serverInfo.valid && !serverInfo.username.empty())
        {
            applyInfo(mUser, serverInfo);
            Debug::warn(Debug::Category::Auth,
                "LOGIN SUCCESS (from server) username=%s displayName=%s userId=%d\n",
                serverInfo.username.c_str(), serverInfo.displayName.c_str(), serverInfo.id);
        }
        else if (serverInfo.valid)
        {
            Debug::warn(Debug::Category::Auth,
                "server returned empty user data, using confirm data\n");
        }
    }
    else
    {
        GameUserInfo info = validateSession(token);
        if (info.valid)
        {
            mState = AuthState::Authenticated;
            applyInfo(mUser, info);
            loggedIn = 1;

            Debug::warn(Debug::Category::Auth,
                "LOGIN SUCCESS (server only) username=%s displayName=%s userId=%d\n",
                info.username.c_str(), info.displayName.c_str(), info.id);
        }
        else
        {
            Debug::warn(Debug::Category::Auth, "session validation failed after auth\n");
            mState = AuthState::NeedsLogin;
        }
    }

    if (loggedIn)
    {
        if (gpPlayer)
            gpPlayer->username = displayName();
        Debug::warn(Debug::Category::Auth, "current username=%s logged in=1\n",
               displayName().c_str());
        cacheAndFinish(mUser);
        refreshStats();
    }
}

void AuthSystem::logout()
{
    clearSessionToken();
    clearProfileCache();
    mState = AuthState::NeedsLogin;
    mUser = {};
    mUser.sessionToken.clear();
    mGuestName = generateGuestName();
    if (gpPlayer)
        gpPlayer->username = displayName();
    Debug::warn(Debug::Category::Auth, "logged out\n");
}

void AuthSystem::clearSession()
{
    clearSessionToken();
    clearProfileCache();
    mUser.sessionToken.clear();
    mState = AuthState::NeedsLogin;
    mUser = {};
    mGuestName = generateGuestName();
    if (gpPlayer)
        gpPlayer->username = displayName();
    Debug::warn(Debug::Category::Auth, "session cleared\n");
}

void AuthSystem::skipLogin()
{
    mState = AuthState::Offline;
    Debug::warn(Debug::Category::Auth, "offline mode, guest name=%s\n", mGuestName.c_str());
}

json AuthSystem::getCloudSettings()
{
    if (mState != AuthState::Authenticated) return {};
    return getSettings(mUser.sessionToken);
}

bool AuthSystem::saveCloudSettings(const json& settings)
{
    if (mState != AuthState::Authenticated) return false;
    return updateSettings(mUser.sessionToken, settings);
}

json AuthSystem::getCloudAvatarData()
{
    if (mState != AuthState::Authenticated) return {};
    return getAvatarData(mUser.sessionToken);
}

bool AuthSystem::saveCloudAvatarData(const json& avatarData)
{
    if (mState != AuthState::Authenticated) return false;
    return ::uploadAvatarData(mUser.sessionToken, avatarData);
}
