#include "auth/auth-controller.h"
#include "auth/auth-system.h"
#include "auth/auth-token.h"
#include "website/api-client.h"
#include "debug/debug-log.h"

#include <cstdio>

AuthController& AuthController::instance()
{
    static AuthController ctrl;
    return ctrl;
}

void AuthController::signIn()
{
    std::string identifier = mForm.identifier;
    std::string password = mForm.password;
    bool rememberMe = mForm.rememberMe;

    Debug::warn(Debug::Category::Auth, "LOGIN BEGIN identifierType=%s rememberMe=%d\n",
        identifier.find('@') != std::string::npos ? "email" : "username",
        (int)rememberMe);

    if (identifier.empty() || password.empty())
    {
        setError("INVALID_CREDENTIALS", "Please enter your username/email and password.");
        return;
    }

    mRuntime.state = AuthState::SigningIn;
    setStatus("Signing in...");

    GameLoginResult login = gameLogin(identifier, password, rememberMe,
                                      "mimita-device-unknown", "", "windows", "0.1.0");

    if (!login.ok)
    {
        Debug::warn(Debug::Category::Auth, "LOGIN RESPONSE failed errorCode=%s\n",
               login.errorCode.c_str());
        mRuntime.state = AuthState::ReadyToSignIn;
        setError(login.errorCode, login.errorMessage);

        mForm.password.clear();
        return;
    }

    Debug::warn(Debug::Category::Auth, "LOGIN RESPONSE success accountId=%d username=%s\n",
           login.accountId, login.username.c_str());

    updateFromLoginResult(login.accountId, login.username,
                          login.accessToken, login.refreshToken, rememberMe);

    mRuntime.state = AuthState::LoadingAccount;
    setStatus("Loading account...");

    GameUserInfo info = validateSession(login.accessToken);
    if (info.valid)
    {
        AuthSystem::instance().applyUserInfo(info);
        mRuntime.state = AuthState::SignedIn;
        setStatus("Signed in as " + info.username);

        CachedProfile cache;
        cache.id = info.id;
        cache.username = info.username;
        cache.displayName = info.displayName;
        cache.avatarUrl = info.avatarUrl;
        cache.supporterTier = info.supporterTier;
        storeProfileCache(cache);

        Debug::warn(Debug::Category::Auth, "LOGIN SUCCESS username=%s accountId=%d\n",
               info.username.c_str(), info.id);
    }
    else
    {
        mRuntime.state = AuthState::Failed;
        setError("BOOTSTRAP_FAILED", "Could not load account data.");
    }

    mForm.password.clear();
}

void AuthController::cancel()
{
    mForm.identifier.clear();
    mForm.password.clear();
    mForm.rememberMe = false;
    mRuntime.state = AuthState::ReadyToSignIn;
    mRuntime.statusText.clear();
    mRuntime.errorCode.clear();
    mRuntime.errorMessage.clear();
}

void AuthController::signOut()
{
    if (!mRuntime.refreshToken.empty())
    {
        gameLogout(mRuntime.refreshToken, "mimita-device-unknown");
    }
    clearSessionToken();
    clearProfileCache();
    mRuntime = {};
    mForm = {};
    mRuntime.state = AuthState::SignedOut;
    Debug::warn(Debug::Category::Auth, "signed out\n");
}

void AuthController::retry()
{
    mRuntime.state = AuthState::ReadyToSignIn;
    mRuntime.statusText.clear();
    mRuntime.errorCode.clear();
    mRuntime.errorMessage.clear();
}

void AuthController::checkStoredSession()
{
    mRuntime.state = AuthState::CheckingStoredSession;
    setStatus("Checking for stored session...");

    std::string stored = loadSessionToken();
    if (stored.empty())
    {
        Debug::warn(Debug::Category::Auth, "no stored session found\n");
        mRuntime.state = AuthState::SignedOut;
        return;
    }

    mRuntime.accessToken = stored;
    Debug::log(Debug::Category::Auth, "stored session token found\n");

    CachedProfile cached = loadProfileCache();
    if (!cached.username.empty())
    {
        mRuntime.username = cached.username;
        mRuntime.accountId = std::to_string(cached.id);
    }

    GameUserInfo info = validateSession(stored);
    if (info.valid)
    {
        mRuntime.state = AuthState::SignedIn;
        mRuntime.accountId = std::to_string(info.id);
        mRuntime.username = info.username;
        mRuntime.supporterTier = info.supporterTier;
        AuthSystem::instance().applyUserInfo(info);
        setStatus("Signed in as " + info.username);
        Debug::warn(Debug::Category::Auth, "session restored: username=%s\n",
               info.username.c_str());
    }
    else
    {
        Debug::warn(Debug::Category::Auth, "stored session invalid or unreachable\n");
        mRuntime.state = AuthState::SignedOut;
        clearSessionToken();
        clearProfileCache();
    }
}

void AuthController::refreshSession()
{
    if (mRuntime.refreshToken.empty())
    {
        mRuntime.state = AuthState::SignedOut;
        return;
    }

    mRuntime.state = AuthState::RefreshingSession;
    setStatus("Refreshing session...");

    GameRefreshResult refresh = gameRefresh(mRuntime.refreshToken, "mimita-device-unknown");

    if (refresh.ok)
    {
        mRuntime.accessToken = refresh.accessToken;
        if (!refresh.refreshToken.empty() && refresh.refreshToken != mRuntime.refreshToken)
        {
            mRuntime.refreshToken = refresh.refreshToken;
        }
        mRuntime.state = AuthState::SignedIn;
        setStatus("Session refreshed.");
        Debug::log(Debug::Category::Auth, "session refreshed successfully\n");
    }
    else
    {
        Debug::warn(Debug::Category::Auth, "session refresh failed: %s\n",
               refresh.errorCode.c_str());
        clearSessionToken();
        clearProfileCache();
        mRuntime = {};
        mForm = {};
        mRuntime.state = AuthState::SignedOut;
    }
}

void AuthController::setStatus(const std::string& text)
{
    mRuntime.statusText = text;
}

void AuthController::setError(const std::string& code, const std::string& message)
{
    mRuntime.state = AuthState::Failed;
    mRuntime.errorCode = code;
    mRuntime.errorMessage = message;
}

void AuthController::updateFromLoginResult(int accountId, const std::string& username,
                                           const std::string& accessToken,
                                           const std::string& refreshToken,
                                           bool rememberMe)
{
    mRuntime.accountId = std::to_string(accountId);
    mRuntime.username = username;
    mRuntime.accessToken = accessToken;
    mRuntime.refreshToken = refreshToken;
    mRuntime.rememberMe = rememberMe;

    if (rememberMe)
    {
        storeSessionToken(accessToken);
    }
}
