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
};

struct AuthUser
{
    int id = 0;
    std::string username;
    std::string avatarUrl;
    std::string sessionToken;
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

    void logout();
    void skipLogin();

    std::string generateGuestName() const;

private:
    AuthSystem() = default;
    void finishAuth(const std::string& token);
    void validateStoredToken();

    AuthState mState = AuthState::Checking;
    AuthUser mUser;
    std::string mGuestName;
    std::string mLinkCode;
    std::string mGrantToken;
    std::string mLinkError;
    float mLinkPollTimer = 0.0f;
    int mLinkPollAttempts = 0;
};
