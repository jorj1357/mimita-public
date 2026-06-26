#include "auth/auth-token.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {

const char* TOKEN_PATH = "config/auth-token.json";

}

bool storeSessionToken(const std::string& token)
{
    std::filesystem::create_directories("config");
    std::ofstream out(TOKEN_PATH, std::ios::trunc);
    if (!out)
    {
        printf("[AUTH] failed to write session token\n");
        return false;
    }
    out << "{\"session_token\":\"" << token << "\"}\n";
    printf("[AUTH] session token stored\n");
    return out.good();
}

std::string loadSessionToken()
{
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
    std::filesystem::remove(TOKEN_PATH);
    printf("[AUTH] session token cleared\n");
}
