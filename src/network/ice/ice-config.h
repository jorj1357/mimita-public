#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct IceStunConfig
{
    std::string host = "107.191.48.226";
    uint16_t port = 3478;
};

struct IceTurnConfig
{
    std::string host = "107.191.48.226";
    uint16_t port = 3478;
    std::string username = "mimita-dev";
    std::string password;
};

struct IceConfiguration
{
    IceStunConfig stun;
    IceTurnConfig turn;
};

// Load config from file, optionally overriding TURN credentials
inline IceConfiguration loadIceConfig(const std::string& path = "config/network/ice-dev.json");
inline IceConfiguration loadIceConfigWithTurn(const std::string& turnHost, uint16_t turnPort,
    const std::string& turnUsername, const std::string& turnCredential);

inline IceConfiguration loadIceConfig(const std::string& path)
{
    IceConfiguration config;

    std::ifstream in(path);
    if (!in)
        return config;

    try
    {
        json j;
        in >> j;

        if (j.contains("stun"))
        {
            auto& s = j["stun"];
            if (s.contains("host")) config.stun.host = s["host"].get<std::string>();
            if (s.contains("port")) config.stun.port = s["port"].get<uint16_t>();
        }

        if (j.contains("turn"))
        {
            auto& t = j["turn"];
            if (t.contains("host")) config.turn.host = t["host"].get<std::string>();
            if (t.contains("port")) config.turn.port = t["port"].get<uint16_t>();

            if (t.contains("username"))
                config.turn.username = t["username"].get<std::string>();

            std::string password;
            if (t.contains("password_env"))
            {
                const char* env = std::getenv(t["password_env"].get<std::string>().c_str());
                if (env) password = env;
            }
            if (password.empty() && t.contains("password"))
                password = t["password"].get<std::string>();

            config.turn.password = password;
        }
    }
    catch (...)
    {
    }

    return config;
}

inline IceConfiguration loadIceConfigWithTurn(const std::string& turnHost, uint16_t turnPort,
    const std::string& turnUsername, const std::string& turnCredential)
{
    IceConfiguration config = loadIceConfig();
    if (!turnCredential.empty())
    {
        config.turn.host = turnHost;
        config.turn.port = turnPort;
        config.turn.username = turnUsername;
        config.turn.password = turnCredential;
    }
    return config;
}
