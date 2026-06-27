#pragma once

#include <string>

// Session token (secure credential storage)
bool storeSessionToken(const std::string& token);
std::string loadSessionToken();
void clearSessionToken();

// Profile cache (local fast-load — server is source of truth)
struct CachedProfile {
    int id = 0;
    std::string username;
    std::string displayName;
    std::string avatarUrl;
    std::string supporterTier;
};

bool storeProfileCache(const CachedProfile& profile);
CachedProfile loadProfileCache();
void clearProfileCache();
