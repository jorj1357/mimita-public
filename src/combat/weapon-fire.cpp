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

glm::vec3 computeConfiguredProjectileDirection(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const glm::vec3& baseDir)
{
    if (def.spread <= 0.0f)
        return glm::normalize(baseDir);

    const auto param = [&](const char* key, float fallback) {
        const auto it = def.customParams.find(key);
        return it != def.customParams.end() ? it->second : fallback;
    };
    const int mode = static_cast<int>(param("spreadMode", 0.0f));
    const float blend = glm::clamp(
        mode == 1 ? 1.0f : param("spreadBlend", 0.0f), 0.0f, 1.0f);
    unsigned int rng = static_cast<unsigned int>(
        runtime.customFloats["projectileSpreadRng"]);
    if (rng == 0)
        rng = 0x6d2b79f5u;
    const glm::vec3 randomDir = computeSpreadDirection(baseDir, def.spread, rng);
    runtime.customFloats["projectileSpreadRng"] = static_cast<float>(rng);

    glm::vec3 up = std::fabs(baseDir.z) < 0.99f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
    up = glm::normalize(glm::cross(right, baseDir));
    const float shot = runtime.customFloats["projectileSpreadShot"]++;
    const float cycle = std::max(2.0f, param("fixedPatternCycle", 12.0f));
    const float t = std::fmod(shot, cycle - 1.0f) / std::max(1.0f, cycle - 2.0f);
    const float fixedRadius = std::tan(glm::radians(def.spread)) * (0.25f + 0.75f * t);
    const glm::vec3 fixedDir = glm::normalize(
        baseDir - right * fixedRadius + up * fixedRadius);
    return glm::normalize(randomDir * (1.0f - blend) + fixedDir * blend);
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
    const Player* targetPlayer,
    float damageMultiplier,
    float beamThicknessOverride);

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
