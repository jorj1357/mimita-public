#pragma once

#include <string>
#include <vector>
#include "auth/auth-system.h"

struct AuthForm
{
    std::string identifier;
    std::string password;
    bool rememberMe = false;
};

struct AuthRuntime
{
    AuthState state = AuthState::SignedOut;
    std::string accountId;
    std::string username;
    std::vector<std::string> permissions;
    std::string supporterTier;
    std::string accessToken;
    std::string refreshToken;
    bool rememberMe = false;
    bool bootstrapLoaded = false;
    std::string statusText;
    std::string errorCode;
    std::string errorMessage;
};

class AuthController
{
public:
    static AuthController& instance();

    AuthForm& form() { return mForm; }
    const AuthRuntime& runtime() const { return mRuntime; }
    AuthRuntime& runtime() { return mRuntime; }

    void signIn();
    void cancel();
    void signOut();
    void retry();

    void checkStoredSession();
    void refreshSession();

    void setStatus(const std::string& text);
    void setError(const std::string& code, const std::string& message);

    void updateFromLoginResult(int accountId, const std::string& username,
                               const std::string& accessToken,
                               const std::string& refreshToken,
                               bool rememberMe);

private:
    AuthController() = default;

    AuthForm mForm;
    AuthRuntime mRuntime;
};
