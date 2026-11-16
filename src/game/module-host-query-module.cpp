// 2026-11-16
// Game-owned queries used by the resident ModuleHost bridge.
// Module entry exports are game-owned and resolved locally by the host.

#include "host/abi/game-host-abi.h"

#include <algorithm>
#include <cstdint>

#include "gamemode/map-config.h"
#include "npc/npc-world-query.h"
#include "worldgen/worldgen-library.h"

namespace mimita::game {

std::uint64_t moduleHostMapRevision() {
    return MapConfigRegistry::instance().revision();
}

const void* moduleHostMapObjectiveConfig() {
    return &MapConfigRegistry::instance().current();
}

int moduleHostProfileFootprint(float outFootprint[3]) {
    outFootprint[0] = 10.0f;
    outFootprint[1] = 10.0f;
    outFootprint[2] = 4.0f;
    const std::string mapId = MapConfigRegistry::instance().mapId();
    if (mapId.empty()) {
        return 1;
    }
    MimitaWorldGen::WorldGenMode profile;
    std::string error;
    if (!MimitaWorldGen::WorldGenLibrary::instance().loadMode(
            mapId, profile, &error)) {
        return 1;
    }
    outFootprint[0] = std::max(0.25f, profile.grid.chunkSize.x);
    outFootprint[1] = std::max(0.25f, profile.grid.chunkSize.y);
    outFootprint[2] = std::max(0.25f, profile.grid.chunkSize.z);
    return 1;
}

int moduleHostNpcLineOfSightBlocked(const float from[3], const float to[3]) {
    return npcWorldLineOfSightBlocked(from, to) ? 1 : 0;
}

int moduleHostNpcRaycastBlocked(const float from[3], const float direction[3],
                                float maxDistance, float* outDistance) {
    return npcWorldRaycastBlocked(from, direction, maxDistance, outDistance)
        ? 1 : 0;
}

void moduleHostSetNpcWorld(const void* world) {
    setNpcWorldQueryWorld(static_cast<const World*>(world));
}

}  // namespace mimita::game

extern "C" __declspec(dllexport)
const mimita::host::GameModuleHostQueryV1* GetMimitaGameModuleHostQueryV1(
    std::uint32_t abiVersion) {
    if (abiVersion != mimita::host::kGameModuleHostQueryAbiVersion) {
        return nullptr;
    }
    static const mimita::host::GameModuleHostQueryV1 api = [] {
        mimita::host::GameModuleHostQueryV1 query{};
        query.abiVersion = mimita::host::kGameModuleHostQueryAbiVersion;
        query.structSize = static_cast<std::uint32_t>(sizeof(query));
        query.map_revision = &mimita::game::moduleHostMapRevision;
        query.map_objective_config =
            &mimita::game::moduleHostMapObjectiveConfig;
        query.profile_footprint = &mimita::game::moduleHostProfileFootprint;
        query.npc_line_of_sight_blocked =
            &mimita::game::moduleHostNpcLineOfSightBlocked;
        query.npc_raycast_blocked =
            &mimita::game::moduleHostNpcRaycastBlocked;
        query.set_npc_world = &mimita::game::moduleHostSetNpcWorld;
        return query;
    }();
    return &api;
}
