// 07 19 2026, 12 00
/* purpose
* Drive the username/email plus password login flow in the exe.
* Convert auth form input into game auth API calls and runtime state.
* Persist only remember-me refresh credentials when requested.
* DOES NOT store passwords locally or render login widgets.
* DOES NOT own backend password verification or account schema.
* DOES NOT implement browser code-link account login.
*/

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

    const std::string deviceId = loadOrCreateDeviceId();
    const std::string deviceName = getDeviceName();

    GameLoginResult login = gameLogin(identifier, password, rememberMe,
                                      deviceId, deviceName, "windows", "0.1.0");

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

    GameBootstrap bootstrap = getGameBootstrap(login.accessToken);
    if (bootstrap.valid)
    {
        AuthSystem::instance().applyBootstrap(login.accessToken, bootstrap);
        mRuntime.state = AuthState::SignedIn;
        setStatus("Signed in as " + bootstrap.user.username);

        CachedProfile cache;
        cache.id = bootstrap.user.id;
        cache.username = bootstrap.user.username;
        cache.displayName = bootstrap.user.displayName;
        cache.avatarUrl = bootstrap.user.avatarUrl;
        cache.supporterTier = bootstrap.user.supporterTier;
        storeProfileCache(cache);

        Debug::warn(Debug::Category::Auth, "LOGIN SUCCESS username=%s accountId=%d\n",
               bootstrap.user.username.c_str(), bootstrap.user.id);
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
        gameLogout(mRuntime.refreshToken, loadOrCreateDeviceId());
    }
    clearSessionToken();
    clearRefreshToken();
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
    if (!stored.empty())
    {
        mRuntime.accessToken = stored;
        Debug::log(Debug::Category::Auth, "stored legacy access token found\n");

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
            AuthSystem::instance().finishAuth(stored, &info, false);
            setStatus("Signed in as " + info.username);
            Debug::warn(Debug::Category::Auth, "session restored from legacy access token: username=%s\n",
                   info.username.c_str());
            return;
        }

        Debug::warn(Debug::Category::Auth, "stored legacy access token invalid\n");
        clearSessionToken();
    }

    std::string refreshToken = loadRefreshToken();
    if (refreshToken.empty())
    {
        Debug::warn(Debug::Category::Auth, "no stored refresh session found\n");
        mRuntime.state = AuthState::SignedOut;
        return;
    }

    mRuntime.refreshToken = refreshToken;
    Debug::log(Debug::Category::Auth, "stored refresh token found, refreshing session\n");

    const std::string deviceId = loadOrCreateDeviceId();
    GameRefreshResult refreshed = gameRefresh(refreshToken, deviceId);
    if (!refreshed.ok || refreshed.accessToken.empty())
    {
        Debug::warn(Debug::Category::Auth, "stored refresh session invalid: %s\n",
            refreshed.errorCode.c_str());
        clearRefreshToken();
        clearProfileCache();
        mRuntime.state = AuthState::SignedOut;
        return;
    }

    mRuntime.accessToken = refreshed.accessToken;
    if (!refreshed.refreshToken.empty() && refreshed.refreshToken != refreshToken)
    {
        mRuntime.refreshToken = refreshed.refreshToken;
        storeRefreshToken(refreshed.refreshToken);
    }

    GameBootstrap bootstrap = getGameBootstrap(refreshed.accessToken);
    if (!bootstrap.valid)
    {
        Debug::warn(Debug::Category::Auth, "refreshed session could not load account data\n");
        clearRefreshToken();
        clearProfileCache();
        mRuntime.state = AuthState::SignedOut;
        return;
    }

    mRuntime.state = AuthState::SignedIn;
    mRuntime.accountId = std::to_string(bootstrap.user.id);
    mRuntime.username = bootstrap.user.username;
    mRuntime.supporterTier = bootstrap.user.supporterTier;
    mRuntime.rememberMe = true;
    AuthSystem::instance().applyBootstrap(refreshed.accessToken, bootstrap);
    setStatus("Signed in as " + bootstrap.user.username);
    Debug::warn(Debug::Category::Auth, "session restored from refresh token: username=%s\n",
           bootstrap.user.username.c_str());
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

    GameRefreshResult refresh = gameRefresh(mRuntime.refreshToken, loadOrCreateDeviceId());

    if (refresh.ok)
    {
        mRuntime.accessToken = refresh.accessToken;
        if (!refresh.refreshToken.empty() && refresh.refreshToken != mRuntime.refreshToken)
        {
            mRuntime.refreshToken = refresh.refreshToken;
            storeRefreshToken(refresh.refreshToken);
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
        clearRefreshToken();
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
        clearSessionToken();
        storeRefreshToken(refreshToken);
    }
    else
    {
        clearSessionToken();
        clearRefreshToken();
    }
}
