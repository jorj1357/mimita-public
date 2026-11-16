#include "network/ice/ice-connect.h"

#include "network/ice/ice-server.h"

int runIceConnect(const std::string& roomCode, const IceConnectOptions& opts) {
    IceServerOptions server;
    server.timeoutSeconds = opts.timeoutSeconds;
    server.disableRelay = opts.disableRelay;
    server.deathRespawnCycles = opts.deathRespawnCycles;
    server.clientIndex = opts.clientIndex;
    server.reconnectToken = opts.reconnectToken;
    server.badconnPreset = opts.badconnPreset;
    return runIceClient(roomCode, server);
}
