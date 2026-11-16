#pragma once

#include <string>

struct IceConnectOptions {
    bool disableRelay = false;
    int timeoutSeconds = 60;
    int deathRespawnCycles = 0;
    int clientIndex = 0;
    std::string reconnectToken;
    std::string badconnPreset;
};

int runIceConnect(const std::string& roomCode, const IceConnectOptions& opts);
