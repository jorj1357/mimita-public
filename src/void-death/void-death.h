#pragma once

#include <string>
#include "entities/player.h"

struct VoidDeathConfig {
    bool enabled = true;
    float killZ = -500.0f;
    std::string command = "explode";
};

VoidDeathConfig& getVoidDeathConfig();
void pollVoidDeathConfig();
void checkVoidDeath(Player& entity, const std::string& entityId, const std::string& entityType);
void registerVoidDeathCommands();
