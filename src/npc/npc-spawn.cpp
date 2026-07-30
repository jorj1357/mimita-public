#include "npc.h"
#include "npc/npc-internal.h"
#include "combat/weapon-runtime.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "config.h"
#include "debug/debug-log.h"
#include "perf/perf-spike.h"
#include "perf/perf-frame.h"
#include "physics/config.h"
#include "game/spawn-override.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "devtools/dev-npc-selection.h"
#include "combat/weapon-registry.h"
#include "perf/perf.h"

float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

float difficulty01(float difficulty)
{
    return clamp01(difficulty / 10.0f);
}

float random01(unsigned int& state)
{
    state = state * 1664525u + 1013904223u;
    return (float)((state >> 8) & 0x00ffffffu) / (float)0x01000000u;
}

glm::vec3 randomPlanarDirection(unsigned int& state)
{
    float angle = random01(state) * glm::two_pi<float>();
    return {std::cos(angle), std::sin(angle), 0.0f};
}

NpcDifficultyTuning tuningForDifficulty(float difficulty)
{
    float d = difficulty01(difficulty);
    NpcDifficultyTuning tuning;
    tuning.reactionDelay = 0.80f - d * 0.68f;
    tuning.actionInterval = 0.90f - d * 0.75f;
    tuning.aggression = 0.15f + d * 0.80f;
    tuning.dodgeChance = d * 0.50f;
    tuning.aimErrorRadians = 0.0f;
    tuning.movementPrecision = 0.15f + d * 0.80f;
    tuning.awarenessRange = 20.0f + d * 130.0f;
    tuning.prediction = 0.02f + d * 0.63f;
    tuning.turnSpeed = 180.0f + d * 900.0f;  // 180 deg/s at diff 0, 1080 deg/s at diff 10
    return tuning;
}

static bool equipNpcWeapon(Npc& npc, const std::string& weaponId, int slot = 1)
{
    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    if (!def) {
        Debug::log(Debug::Category::NpcCombat, "[NPC WEAPON] npc=%u weapon '%s' not found\n",
                   npc.id, weaponId.c_str());
        return false;
    }
    npc.body.equippedWeaponId = weaponId;
    npc.body.equippedSlot = slot;
    npc.body.hasValidWeapon = true;
    npc.body.weaponRuntimes[weaponId] = WeaponRuntime{};
    WeaponRuntimeHelper::initRuntime(npc.body.weaponRuntimes[weaponId], *def);
    resetAllWeaponRuntimesForSpawn(npc.body, "Npc equipWeapon");
    Debug::log(Debug::Category::NpcCombat,
        "[NPC WEAPON] npc=%u equipped weapon=%s slot=%d type=%d\n",
        npc.id, weaponId.c_str(), slot, (int)def->behaviorType);
    return true;
}

Npc::Npc(std::uint32_t npcId, float npcDifficulty, glm::vec3 spawn,
         const std::string& weaponId)
    : id(npcId), difficulty(std::clamp(npcDifficulty, 0.0f, 10.0f))
{
    tuning = tuningForDifficulty(difficulty);
    rngState = 0x9e3779b9u ^ (id * 747796405u);
    body.reset();
    body.username = "npc-" + std::to_string(id);
    body.currentHp = body.maxHp;
    if (DevOverrides::healthOverrideEnabled) {
        body.maxHp = DevOverrides::healthOverrideValue;
        body.currentHp = DevOverrides::healthOverrideValue;
    }
    body.pos = spawn;
    body.respawnPosition = spawn;
    body.vel = {0.0f, 0.0f, 0.0f};
    body.syncLegacyStateToLayers();
    previousPosition = body.pos;

    wakeupTimer = 3.0f;  // 180 ticks @ 60 Hz

    stateMachine.nextDecisionTime = 0.0f;
    stateMachine.wanderTarget = spawn + randomPlanarDirection(rngState) * 5.0f;
    stateMachine.wanderTimer = 2.0f + random01(rngState) * 3.0f;
    stateMachine.orbitAngle = random01(rngState) * glm::two_pi<float>();
    stateMachine.orbitSwapTimer = 0.5f + random01(rngState) * 2.0f;

    aimTimer = 0.0f;
    reactionTimer = 0.0f;
    moveNoiseTimer = 0.1f + random01(rngState) * 0.3f;
    moveOffset = {0.0f, 0.0f};

    // Equip weapon (default revolver, or specified via parameter)
    if (!equipNpcWeapon(*this, weaponId.empty() ? "revolver" : weaponId, 1)) {
        equipNpcWeapon(*this, "revolver", 1);  // fallback to revolver
    }

    // Spawn wakeup visual sphere (30 ticks @ 60 Hz = 0.5s)
    {
        EffectPart sphere;
        sphere.position = spawn;
        sphere.replayType = "npc_spawn_sphere";
        sphere.color = {0.5f, 0.0f, 0.0f};
        sphere.alpha = 0.5f;
        sphere.maxLifetime = 0.5f;
        sphere.scale = PLAYER_RADIUS;
        sphere.endScale = PLAYER_RADIUS * 1.5f;
        sphere.sticky = true;
        sphere.billboardText = false;
        sphere.ownerId = id;
        EffectPartSystem::instance().spawn(sphere);
    }

    Debug::log(Debug::Category::General,
               "[NPC] spawned id=%u difficulty=%.1f reaction=%.2f aggression=%.2f awareness=%.1f\n",
               id,
               difficulty,
               tuning.reactionDelay,
               tuning.aggression,
               tuning.awarenessRange);
}

void NpcSystem::spawnPrototypeScene()
{
    clear();
    for (int i = 0; i < 5; ++i) {
        float d = 1.0f + i * 2.0f;
        if (i == 4) d = 10.0f;
        spawnNpc(d);
    }
}

void NpcSystem::clear()
{
    for (const Npc& npc : npcs) {
        AudioManager::instance().stopOwner(npc.id);
        EffectPartSystem::instance().destroyOwner(npc.id);
        Debug::log(Debug::Category::General, "[NPC] destroyed id=%u\n", npc.id);
    }
    npcs.clear();
    NpcSelectionManager::instance().clear();
    Debug::log(Debug::Category::General, "[NPC] cleanup complete\n");
}

void NpcSystem::spawnNpc(float difficulty)
{
    MIMITA_PERF_SCOPE("NpcSystem::SpawnNpc");
    Perf::ScopedTimer _spawnTimer("NpcSpawn");
    float d = globalDifficulty_ > 0.0f ? globalDifficulty_ : difficulty;
    uint32_t id = nextNpcId();
    char corrId[32];
    std::snprintf(corrId, sizeof(corrId), "NPC_%05u", id);
    perfSetCorrelation(corrId);
    glm::vec3 spawnPos = npcSpawnPoint;
    glm::vec3 overridePos;
    if (tryGetSpawnOverride(overridePos)) {
        spawnPos = overridePos;
        Debug::log(Debug::Category::General, "[NPC SPAWN] override active spawning at (%.1f %.1f %.1f)\n",
                   overridePos.x, overridePos.y, overridePos.z);
    }
    npcs.emplace_back(id, d, spawnPos);
    AudioManager::instance().play({"npc_spawn", AudioCategory::NPC, true, spawnPos, 0.8f, 1.0f, 35.0f, id});
    Debug::log(Debug::Category::General, "[NPC] spawned id=%u at (%.2f, %.2f, %.2f) (global diff=%.1f)\n",
               id, spawnPos.x, spawnPos.y, spawnPos.z, d);
}

void NpcSystem::spawnNpc(uint32_t id, float difficulty, glm::vec3 spawnPos)
{
    Perf::ScopedTimer _spawnTimer("NpcSpawn");
    float d = globalDifficulty_ > 0.0f ? globalDifficulty_ : difficulty;
    npcs.emplace_back(id, d, spawnPos);
    AudioManager::instance().play({"npc_spawn", AudioCategory::NPC, true, spawnPos, 0.8f, 1.0f, 35.0f, id});
    Debug::log(Debug::Category::General, "[NPC] spawned id=%u at (%.2f, %.2f, %.2f) (network, diff=%.1f)\n",
               id, spawnPos.x, spawnPos.y, spawnPos.z, d);
}

void NpcSystem::destroySelected(const std::vector<std::uint32_t>& ids)
{
    npcs.erase(std::remove_if(npcs.begin(), npcs.end(), [&](const Npc& npc) {
        if (std::find(ids.begin(), ids.end(), npc.id) == ids.end()) return false;
        AudioManager::instance().stopOwner(npc.id);
        EffectPartSystem::instance().destroyOwner(npc.id);
        NpcSelectionManager::instance().deselect(npc.id);
        Debug::log(Debug::Category::General, "[NPC] destroyed id=%u\n", npc.id);
        return true;
    }), npcs.end());
    Debug::log(Debug::Category::General, "[NPC] cleanup complete\n");
}

void NpcSystem::destroyAll() { clear(); }

void NpcSystem::setGlobalDifficulty(float d)
{
    globalDifficulty_ = std::clamp(d, 1.0f, 10.0f);
    for (Npc& npc : npcs)
    {
        npc.difficulty = globalDifficulty_;
        npc.tuning = tuningForDifficulty(globalDifficulty_);
    }
    Debug::log(Debug::Category::General, "[NPC] global difficulty set to %.1f for %zu NPCs\n",
               globalDifficulty_, npcs.size());
}

float weaponEffectiveRange(const Npc& npc)
{
    const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
    if (!def) return 150.0f;
    auto it = def->customParams.find("effectiveRange");
    if (it != def->customParams.end()) return it->second;
    if (def->projectileSpeed > 0.0f)
        return def->projectileSpeed * std::max(def->projectileLifetime, 2.0f);
    return 150.0f;
}
