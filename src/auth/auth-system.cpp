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
        mUser.avatarUrl = info.avatarUrl;
        printf("[AUTH] session validated: user=%s id=%d\n",
               info.username.c_str(), info.id);
    }
    else
    {
        printf("[AUTH] stored session invalid or server unreachable, clearing\n");
        clearSessionToken();
        mUser.sessionToken.clear();
        mState = AuthState::NeedsLogin;
    }
}

std::string AuthSystem::displayName() const
{
    if (mState == AuthState::Authenticated && !mUser.username.empty())
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
        mUser.avatarUrl = info.avatarUrl;
        printf("[AUTH] authenticated: user=%s id=%d\n",
               info.username.c_str(), info.id);
    }
    else
    {
        printf("[AUTH] session validation failed after finalize\n");
        mState = AuthState::NeedsLogin;
    }
}

void AuthSystem::logout()
{
    clearSessionToken();
    mState = AuthState::NeedsLogin;
    mUser = {};
    mUser.sessionToken.clear();
    mGuestName = generateGuestName();
    printf("[AUTH] logged out\n");
}

void AuthSystem::skipLogin()
{
    mState = AuthState::Offline;
    printf("[AUTH] offline mode, guest name=%s\n", mGuestName.c_str());
}
