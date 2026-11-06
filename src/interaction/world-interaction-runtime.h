// 2026-11-05
/* purpose
* Translate authored map interaction definitions into shared InteractionSystem
* targets whose actions are looked up in InteractionActionRegistry.
* Does not own gameplay behavior; owners register named actions.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "gamemode/map-config.h"

class WorldInteractionRuntime
{
public:
    static WorldInteractionRuntime& instance();

    void syncIfChanged(const MapObjectiveConfig& config, uint64_t revision);
    void clear();
    uint32_t targetIdFor(const std::string& interactionId) const;

private:
    WorldInteractionRuntime() = default;

    uint64_t mLastRevision = 0;
    bool mHasSynced = false;
    std::unordered_map<std::string, uint32_t> mTargetIds;
};

void registerBuiltInWorldInteractionActions();
void registerWorldInteractionCommands();
