#pragma once

#include <cstdint>
#include <string>

namespace MimitaNet {

struct ServerPreset
{
    std::string id;
    std::string serverName = "MiMITA Server";
    std::string mapName = "funworldv3";
    std::string gameMode = "sandbox";
    uint32_t maxPlayers = 999;
    bool startupNpcsEnabled = true;
    uint32_t startupNpcCount = 3;
    bool autoMapRotation = true;
    uint32_t mapRotationMinutes = 15;
    bool discordNotification = true;
    bool joinHost = true;
};

bool loadServerPreset(const std::string& id, ServerPreset& out, std::string& error);

} // namespace MimitaNet
