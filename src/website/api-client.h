#pragma once

#include <string>

struct GameUserInfo
{
    bool valid = false;
    int id = 0;
    std::string username;
    std::string avatarUrl;
};

struct LinkCodeResult
{
    bool ok = false;
    std::string code;
    std::string grantToken;
};

bool websiteReachable();
LinkCodeResult requestAuthLink();
bool pollLinkStatus(const std::string& code);
std::string finalizeLink(const std::string& code, const std::string& grantToken);
GameUserInfo validateSession(const std::string& sessionToken);
