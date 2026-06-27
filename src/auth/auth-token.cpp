#include "auth/auth-token.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <wincred.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "credui.lib")

using json = nlohmann::json;

namespace {

const char* TOKEN_PATH = "config/auth-token.json";
const char* CACHE_PATH = "config/auth-cache.json";
const char* CRED_TARGET = "MimitaAuthSession";

bool storeCredentialManager(const std::string& token)
{
    bool result = false;
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    wchar_t targetName[] = L"MimitaAuthSession";
    cred.TargetName = targetName;
    cred.CredentialBlobSize = (DWORD)token.size();
    cred.CredentialBlob = (BYTE*)token.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    wchar_t userName[] = L"MimitaUser";
    cred.UserName = userName;
    if (CredWriteW(&cred, 0))
    {
        printf("[AUTH] session token stored in Credential Manager\n");
        result = true;
    }
    else
    {
        printf("[AUTH] Credential Manager write failed (error=%lu), falling back to file\n", GetLastError());
    }
    return result;
}

std::string loadCredentialManager()
{
    PCREDENTIALW cred = nullptr;
    if (CredReadW(L"MimitaAuthSession", CRED_TYPE_GENERIC, 0, &cred))
    {
        std::string token((const char*)cred->CredentialBlob, cred->CredentialBlobSize);
        CredFree(cred);
        return token;
    }
    return {};
}

void clearCredentialManager()
{
    if (CredDeleteW(L"MimitaAuthSession", CRED_TYPE_GENERIC, 0))
        printf("[AUTH] session token removed from Credential Manager\n");
}

}

bool storeSessionToken(const std::string& token)
{
    if (storeCredentialManager(token))
        return true;

    std::filesystem::create_directories("config");
    std::ofstream out(TOKEN_PATH, std::ios::trunc);
    if (!out)
    {
        printf("[AUTH] failed to write session token\n");
        return false;
    }
    out << "{\"session_token\":\"" << token << "\"}\n";
    printf("[AUTH] session token stored to file\n");
    return out.good();
}

std::string loadSessionToken()
{
    std::string token = loadCredentialManager();
    if (!token.empty())
        return token;

    std::ifstream in(TOKEN_PATH);
    if (!in)
        return {};

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    auto pos = content.find("\"session_token\":\"");
    if (pos == std::string::npos)
        return {};

    pos += 17;
    auto end = content.find('"', pos);
    if (end == std::string::npos)
        return {};

    return content.substr(pos, end - pos);
}

void clearSessionToken()
{
    clearCredentialManager();
    std::filesystem::remove(TOKEN_PATH);
    printf("[AUTH] session token cleared\n");
}

// ── Profile Cache ────────────────────────────────────────────────────────────

bool storeProfileCache(const CachedProfile& profile)
{
    try {
        std::filesystem::create_directories("config");
        json j;
        j["id"] = profile.id;
        j["username"] = profile.username;
        j["display_name"] = profile.displayName;
        j["avatar_url"] = profile.avatarUrl;
        j["supporter_tier"] = profile.supporterTier;

        std::ofstream out(CACHE_PATH, std::ios::trunc);
        if (!out)
        {
            printf("[AUTH] failed to write profile cache\n");
            return false;
        }
        out << j.dump(2);
        printf("[AUTH] profile cache saved: %s\n", profile.username.c_str());
        return out.good();
    } catch (const std::exception& e) {
        printf("[AUTH] profile cache write error: %s\n", e.what());
        return false;
    }
}

CachedProfile loadProfileCache()
{
    CachedProfile profile;
    std::ifstream in(CACHE_PATH);
    if (!in)
        return profile;

    try {
        json j;
        in >> j;
        profile.id = j.value("id", 0);
        profile.username = j.value("username", "");
        profile.displayName = j.value("display_name", "");
        profile.avatarUrl = j.value("avatar_url", "");
        profile.supporterTier = j.value("supporter_tier", "free");

        if (!profile.username.empty())
            printf("[AUTH] profile cache loaded: %s\n", profile.username.c_str());
    } catch (const std::exception& e) {
        printf("[AUTH] profile cache read error: %s\n", e.what());
    }
    return profile;
}

void clearProfileCache()
{
    std::filesystem::remove(CACHE_PATH);
    printf("[AUTH] profile cache cleared\n");
}
