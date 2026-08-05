#pragma once

#include <string>

struct IceTestOptions
{
    bool forceRelay = false;
    bool disableRelay = false;
    bool once = false;
    int timeoutSeconds = 60;
    int expectedClients = 1;
    int deathRespawnCycles = 0;
    int clientIndex = 0;
    std::string reconnectToken;
    std::string badconnPreset;
};

bool runIceHostOnly(const IceTestOptions& opts);
bool runIceJoinOnly(const std::string& roomCode, const IceTestOptions& opts);
bool runIceGameHost(const IceTestOptions& opts);
bool runIceGameClient(const std::string& roomCode, const IceTestOptions& opts);
int runIceServer(const IceTestOptions& opts);
int runIceConnect(const std::string& roomCode, const IceTestOptions& opts);
