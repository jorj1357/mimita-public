// 07 19 2026, 12 00
/* purpose
* Store and load local auth tokens and safe profile cache data.
* Prefer Windows Credential Manager for session secrets.
* Fall back to per-user AppData files instead of repository config files.
* DOES NOT store passwords or verify credentials.
* DOES NOT contact the account server.
* DOES NOT decide whether a token is valid.
*/

#include "auth/auth-token.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <windows.h>
#include <wincred.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "credui.lib")

using json = nlohmann::json;

namespace {

const char* TOKEN_PATH = "config/auth-token.json";
const char* CACHE_PATH = "config/auth-cache.json";
const char* REFRESH_PATH = "config/auth-refresh.json";
const char* CRED_TARGET = "MimitaAuthSession";
const char* REFRESH_CRED_TARGET = "MimitaRefreshToken";

std::filesystem::path authDataDir()
{
    const char* local = std::getenv("LOCALAPPDATA");
    if (local && local[0])
        return std::filesystem::path(local) / "Mimita";

    const char* roaming = std::getenv("APPDATA");
    if (roaming && roaming[0])
        return std::filesystem::path(roaming) / "Mimita";

    return std::filesystem::path("config");
}

std::filesystem::path sessionTokenPath()
{
    return authDataDir() / "session.dat";
}

std::filesystem::path refreshTokenPath()
{
    return authDataDir() / "refresh.dat";
}

std::filesystem::path profileCachePath()
{
    return authDataDir() / "profile-cache.json";
}

std::filesystem::path devicePath()
{
    return authDataDir() / "device.json";
}

std::string randomHex(size_t bytes)
{
    static const char* digits = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    std::string out;
    out.reserve(bytes * 2);
    for (size_t i = 0; i < bytes; ++i)
    {
        int v = dist(gen);
        out.push_back(digits[(v >> 4) & 15]);
        out.push_back(digits[v & 15]);
    }
    return out;
}

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

    std::filesystem::create_directories(authDataDir());
    std::ofstream out(sessionTokenPath(), std::ios::trunc);
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

    std::ifstream in(sessionTokenPath());
    if (!in)
        in.open(TOKEN_PATH);
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
    std::filesystem::remove(sessionTokenPath());
    std::filesystem::remove(TOKEN_PATH);
    printf("[AUTH] session token cleared\n");
}

// ── Refresh Token (Remember-Me storage) ────────────────────────────────────────

bool storeRefreshToken(const std::string& token)
{
    if (token.empty()) return false;

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    wchar_t targetName[] = L"MimitaRefreshToken";
    cred.TargetName = targetName;
    cred.CredentialBlobSize = (DWORD)token.size();
    cred.CredentialBlob = (BYTE*)token.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    wchar_t userName[] = L"MimitaUser";
    cred.UserName = userName;
    if (CredWriteW(&cred, 0))
    {
        printf("[AUTH] refresh token stored in Credential Manager\n");
        return true;
    }

    std::filesystem::create_directories(authDataDir());
    std::ofstream out(refreshTokenPath(), std::ios::trunc);
    if (!out)
    {
        printf("[AUTH] failed to write refresh token\n");
        return false;
    }
    out << "{\"refresh_token\":\"" << token << "\"}\n";
    printf("[AUTH] refresh token stored to file\n");
    return out.good();
}

std::string loadRefreshToken()
{
    PCREDENTIALW cred = nullptr;
    if (CredReadW(L"MimitaRefreshToken", CRED_TYPE_GENERIC, 0, &cred))
    {
        std::string token((const char*)cred->CredentialBlob, cred->CredentialBlobSize);
        CredFree(cred);
        return token;
    }

    std::ifstream in(refreshTokenPath());
    if (!in)
        in.open(REFRESH_PATH);
    if (!in)
        return {};

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    auto pos = content.find("\"refresh_token\":\"");
    if (pos == std::string::npos)
        return {};

    pos += 16;
    auto end = content.find('"', pos);
    if (end == std::string::npos)
        return {};

    return content.substr(pos, end - pos);
}

void clearRefreshToken()
{
    CredDeleteW(L"MimitaRefreshToken", CRED_TYPE_GENERIC, 0);
    std::filesystem::remove(refreshTokenPath());
    std::filesystem::remove(REFRESH_PATH);
    printf("[AUTH] refresh token cleared\n");
}

std::string loadOrCreateDeviceId()
{
    std::ifstream in(devicePath());
    if (in)
    {
        try {
            json j;
            in >> j;
            std::string id = j.value("device_id", "");
            if (!id.empty())
                return id;
        } catch (...) {}
    }

    std::string id = "mimita-" + randomHex(16);
    try {
        std::filesystem::create_directories(authDataDir());
        json j;
        j["device_id"] = id;
        std::ofstream out(devicePath(), std::ios::trunc);
        if (out)
            out << j.dump(2);
    } catch (...) {}
    return id;
}

std::string getDeviceName()
{
    char name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = sizeof(name);
    if (GetComputerNameA(name, &size) && size > 0)
        return std::string(name, size);
    return "Windows PC";
}

// ── Profile Cache ────────────────────────────────────────────────────────────

bool storeProfileCache(const CachedProfile& profile)
{
    try {
        std::filesystem::create_directories(authDataDir());
        json j;
        j["id"] = profile.id;
        j["username"] = profile.username;
        j["display_name"] = profile.displayName;
        j["avatar_url"] = profile.avatarUrl;
        j["supporter_tier"] = profile.supporterTier;
        j["vip_appearance"] = {
            {"tier", profile.vipAppearance.tier},
            {"style_kind", profile.vipAppearance.styleKind},
            {"color_r", profile.vipAppearance.colorR},
            {"color_g", profile.vipAppearance.colorG},
            {"color_b", profile.vipAppearance.colorB},
            {"flags", profile.vipAppearance.flags}
        };
        {
            json colors = json::array();
            for (const auto& c : profile.vipStyleDetail.colors)
                colors.push_back({
                    {"r", (int)std::lround(c.r * 255.0f)},
                    {"g", (int)std::lround(c.g * 255.0f)},
                    {"b", (int)std::lround(c.b * 255.0f)}
                });
            j["vip_style"] = {
                {"style_kind", profile.vipStyleDetail.styleKind},
                {"animation", profile.vipStyleDetail.animation},
                {"direction", profile.vipStyleDetail.direction},
                {"rainbow_speed", profile.vipStyleDetail.rainbowSpeed},
                {"colors", colors}
            };
        }

        std::ofstream out(profileCachePath(), std::ios::trunc);
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
    std::ifstream in(profileCachePath());
    if (!in)
        in.open(CACHE_PATH);
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
        profile.vipAppearance = MimitaVip::tierDefaultAppearance(
            MimitaVip::tierFromString(profile.supporterTier));
        if (j.contains("vip_appearance") && j["vip_appearance"].is_object())
        {
            const json& vip = j["vip_appearance"];
            profile.vipAppearance.tier = (uint8_t)vip.value("tier", (int)profile.vipAppearance.tier);
            profile.vipAppearance.styleKind = (uint8_t)vip.value("style_kind", (int)profile.vipAppearance.styleKind);
            profile.vipAppearance.colorR = (uint8_t)vip.value("color_r", (int)profile.vipAppearance.colorR);
            profile.vipAppearance.colorG = (uint8_t)vip.value("color_g", (int)profile.vipAppearance.colorG);
            profile.vipAppearance.colorB = (uint8_t)vip.value("color_b", (int)profile.vipAppearance.colorB);
            profile.vipAppearance.flags = (uint8_t)vip.value("flags", (int)profile.vipAppearance.flags);
            profile.supporterTier = MimitaVip::tierToString(profile.vipAppearance.tier);
        }
        if (j.contains("vip_style") && j["vip_style"].is_object())
        {
            const json& vs = j["vip_style"];
            profile.vipStyleDetail.styleKind = (uint8_t)vs.value("style_kind", 0);
            profile.vipStyleDetail.animation = (uint8_t)vs.value("animation", 0);
            profile.vipStyleDetail.direction = (uint8_t)vs.value("direction", 0);
            profile.vipStyleDetail.rainbowSpeed = vs.value("rainbow_speed", 1.0f);
            if (vs.contains("colors") && vs["colors"].is_array())
            {
                for (const auto& c : vs["colors"])
                {
                    profile.vipStyleDetail.colors.push_back(MimitaVip::colorFromBytes(
                        (uint8_t)c.value("r", 158),
                        (uint8_t)c.value("g", 158),
                        (uint8_t)c.value("b", 158)));
                }
            }
            if (!profile.vipStyleDetail.colors.empty())
                profile.vipStyleDetail.solidColor = profile.vipStyleDetail.colors[0];
        }

        if (!profile.username.empty())
            printf("[AUTH] profile cache loaded: %s\n", profile.username.c_str());
    } catch (const std::exception& e) {
        printf("[AUTH] profile cache read error: %s\n", e.what());
    }
    return profile;
}

void clearProfileCache()
{
    std::filesystem::remove(profileCachePath());
    std::filesystem::remove(CACHE_PATH);
    printf("[AUTH] profile cache cleared\n");
}
