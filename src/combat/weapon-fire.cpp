#include "weapon-fire.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "world/world.h"
#include "npc/npc.h"
#include "replay/replay.h"
#include "ui/hitmarker.h"

namespace WeaponFire {

glm::vec3 computeSpreadDirection(const glm::vec3& baseDir, float spreadDegrees, unsigned int& rngState) {
    if (spreadDegrees <= 0.0f) return baseDir;
    rngState = rngState * 1103515245u + 12345u;
    float theta = ((float)(rngState & 0x7FFF) / 32767.0f) * 6.2831853f;
    rngState = rngState * 1103515245u + 12345u;
    float radius = ((float)(rngState & 0x7FFF) / 32767.0f) * std::tan(glm::radians(spreadDegrees));
    glm::vec3 up = std::fabs(baseDir.z) < 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
    glm::vec3 fwd = glm::normalize(glm::cross(right, up));
    return glm::normalize(baseDir + (right * std::cos(theta) + fwd * std::sin(theta)) * radius);
}

extern RevolverShotResult tryFireHitscan(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    std::unordered_map<uint32_t, Player>* remoteNpcs);

extern RevolverShotResult tryFireHitscanDir(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& shooter,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& aimDir,
    const Player* targetPlayer);

extern void fireMultiPellet(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    RevolverShotResult& outResult,
    std::unordered_map<uint32_t, Player>* remoteNpcs);

} // namespace WeaponFire
