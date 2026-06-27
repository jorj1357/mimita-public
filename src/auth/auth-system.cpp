#include "auth/auth-system.h"
#include "auth/auth-token.h"
#include "website/api-client.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <shellapi.h>

AuthSystem& AuthSystem::instance()
{
    static AuthSystem system;
    return system;
}

void AuthSystem::init(const std::string& cliSessionToken)
{
    mGuestName = generateGuestName();
    mState = AuthState::Checking;
    printf("[AUTH] checking for stored session...\n");

    if (!cliSessionToken.empty())
    {
        printf("[AUTH] received session token from command line\n");
        mUser.sessionToken = cliSessionToken;
        storeSessionToken(cliSessionToken);
        return;
    }

    std::string stored = loadSessionToken();
    if (stored.empty())
    {
        printf("[AUTH] no stored session found\n");
        mState = AuthState::NeedsLogin;
        return;
    }

    mUser.sessionToken = stored;
    printf("[AUTH] stored session token found\n");

    // Load cached profile so username shows immediately without network call
    CachedProfile cached = loadProfileCache();
    if (!cached.username.empty())
    {
        mUser.id = cached.id;
        mUser.username = cached.username;
        mUser.displayName = cached.displayName;
        mUser.avatarUrl = cached.avatarUrl;
        mUser.supporterTier = cached.supporterTier;
        printf("[AUTH] using cached profile: %s\n", cached.username.c_str());
        // State stays Checking — tickValidate will re-check with server next frame
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
        mUser.id = info.id;
        mUser.username = info.username;
        mUser.displayName = info.displayName;
        mUser.email = info.email;
        mUser.bio = info.bio;
        mUser.avatarUrl = info.avatarUrl;
        mUser.supporterTier = info.supporterTier;
        mUser.role = info.role;
        mUser.emailVerified = info.emailVerified;
        mUser.createdAt = info.createdAt;
        mUser.avatarData = info.avatarData;
        printf("[AUTH] session validated: user=%s id=%d\n",
               info.username.c_str(), info.id);

        // Update local cache
        CachedProfile cache;
        cache.id = info.id;
        cache.username = info.username;
        cache.displayName = info.displayName;
        cache.avatarUrl = info.avatarUrl;
        cache.supporterTier = info.supporterTier;
        storeProfileCache(cache);

        refreshStats();
    }
    else
    {
        printf("[AUTH] stored session invalid or server unreachable, clearing\n");
        clearSessionToken();
        clearProfileCache();
        mUser.sessionToken.clear();
        mState = AuthState::NeedsLogin;
    }
}

void AuthSystem::fetchFullProfile()
{
    GameUserInfo info = getProfile(mUser.sessionToken);
    if (info.valid)
    {
        mUser.id = info.id;
        mUser.username = info.username;
        mUser.displayName = info.displayName;
        mUser.email = info.email;
        mUser.bio = info.bio;
        mUser.avatarUrl = info.avatarUrl;
        mUser.avatarData = info.avatarData;
        mUser.supporterTier = info.supporterTier;
        mUser.role = info.role;
        mUser.createdAt = info.createdAt;
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
    printf("[AUTH] stats refreshed: mmr=%d wins=%d\n",
           mUser.stats.currentMmr, mUser.stats.wins);
}

std::string AuthSystem::displayName() const
{
    // Always prefer display name if we have user data (cached or live)
    if (!mUser.displayName.empty())
        return mUser.displayName;
    if (!mUser.username.empty())
        return mUser.username;
    // Fall back to guest only if no account data at all
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
        printf("[AUTH] link code request failed\n");
        return;
    }

    mState = AuthState::Linking;
    mLinkCode = link.code;
    mGrantToken = link.grantToken;
    mLinkPollTimer = 0.0f;
    mLinkPollAttempts = 0;

    std::string url = "https://mimita.fun/signin?redirect=/link";
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    printf("[AUTH] opened browser for sign in, code=%s\n", mLinkCode.c_str());
}

void AuthSystem::pollLinkFlow()
{
    if (mState != AuthState::Linking)
        return;
    if (mLinkCode.empty() || mGrantToken.empty())
        return;

    mLinkPollTimer += 1.0f / 60.0f;
    if (mLinkPollTimer < 2.0f)
        return;
    mLinkPollTimer = 0.0f;

    mLinkPollAttempts++;
    if (mLinkPollAttempts > 150)
    {
        printf("[AUTH] link polling timed out\n");
        mState = AuthState::NeedsLogin;
        mLinkError = "Timed out waiting for authentication.";
        return;
    }

    bool claimed = pollLinkStatus(mLinkCode);
    if (!claimed) return;

    printf("[AUTH] link code claimed, finalizing...\n");
    std::string sessionToken = finalizeLink(mLinkCode, mGrantToken);
    if (sessionToken.empty())
    {
        printf("[AUTH] link finalize failed\n");
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
        printf("[AUTH] token exchange failed\n");
        mState = AuthState::NeedsLogin;
        return;
    }

    finishAuth(sessionToken);
}

GameUserInfo AuthSystem::finishTokenExchange(const std::string& sessionToken)
{
    return validateSession(sessionToken);
}

void AuthSystem::finishAuth(const std::string& token)
{
    storeSessionToken(token);
    mUser.sessionToken = token;

    GameUserInfo info = validateSession(token);
    if (info.valid)
    {
        mState = AuthState::Authenticated;
        mUser.id = info.id;
        mUser.username = info.username;
        mUser.displayName = info.displayName;
        mUser.email = info.email;
        mUser.bio = info.bio;
        mUser.avatarUrl = info.avatarUrl;
        mUser.supporterTier = info.supporterTier;
        mUser.role = info.role;
        mUser.emailVerified = info.emailVerified;
        mUser.createdAt = info.createdAt;
        mUser.avatarData = info.avatarData;
        printf("[AUTH] authenticated: user=%s id=%d\n",
               info.username.c_str(), info.id);

        // Save to local cache for fast startup next time
        CachedProfile cache;
        cache.id = info.id;
        cache.username = info.username;
        cache.displayName = info.displayName;
        cache.avatarUrl = info.avatarUrl;
        cache.supporterTier = info.supporterTier;
        storeProfileCache(cache);

        refreshStats();
    }
    else
    {
        printf("[AUTH] session validation failed after auth\n");
        mState = AuthState::NeedsLogin;
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
    printf("[AUTH] logged out\n");
}

void AuthSystem::clearSession()
{
    clearSessionToken();
    clearProfileCache();
    mUser.sessionToken.clear();
    mState = AuthState::NeedsLogin;
    mUser = {};
    mGuestName = generateGuestName();
    printf("[AUTH] session cleared\n");
}

void AuthSystem::skipLogin()
{
    mState = AuthState::Offline;
    printf("[AUTH] offline mode, guest name=%s\n", mGuestName.c_str());
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
