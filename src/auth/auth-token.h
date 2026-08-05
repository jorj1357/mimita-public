// 07 19 2026, 12 00
/* purpose
* Declare local auth token and profile cache storage helpers.
* Keep refresh-token persistence separate from access-token runtime state.
* Provide a small cache model for menu display before server validation.
* DOES NOT store user passwords or password-derived secrets.
* DOES NOT validate sessions with the backend.
* DOES NOT define gameplay account data.
*/

#pragma once

#include <string>
#include "vip/vip-appearance.h"

// Session token (secure credential storage)
bool storeSessionToken(const std::string& token);
std::string loadSessionToken();
void clearSessionToken();

// Refresh token (secure credential storage — used for remember-me)
bool storeRefreshToken(const std::string& token);
std::string loadRefreshToken();
void clearRefreshToken();

// Stable local install identity for server-side session/device tracking.
std::string loadOrCreateDeviceId();
std::string getDeviceName();

// Profile cache (local fast-load — server is source of truth)
struct CachedProfile {
    int id = 0;
    std::string username;
    std::string displayName;
    std::string avatarUrl;
    std::string supporterTier;
    MimitaVip::VipAppearance vipAppearance;
    MimitaVip::VipStyleDetail vipStyleDetail;
};

bool storeProfileCache(const CachedProfile& profile);
CachedProfile loadProfileCache();
void clearProfileCache();
