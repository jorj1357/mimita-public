#pragma once

#include <string>

struct IceServerOptions {
    std::string mapName = "funworldv3";
    bool npcsEnabled = false;
    uint32_t npcCount = 0;
    int timeoutSeconds = 3600;
    bool disableRelay = false;
    int deathRespawnCycles = 0;
    int clientIndex = 0;
    std::string reconnectToken;
    std::string badconnPreset;
};

int runIceServer(const IceServerOptions& opts);
int runIceClient(const std::string& roomCode, const IceServerOptions& opts);
