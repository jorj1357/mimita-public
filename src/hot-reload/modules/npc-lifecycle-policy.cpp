// 09 24 2026
/* purpose
* Hot NPC lifecycle provider. Registers the generic `npc.lifecycle` capability
* and owns every NPC lifecycle decision: automatic startup NPC count, spawn
* placement intent, initial health, starting weapon/loadout, difficulty,
* respawn, and reconciliation of stale automatic NPCs.
* Values come from config/weapons.json (the `npc_lifecycle` block plus weapon
* tuning); the multiplier constants here are the hot formula layer. Editing this
* file changes NPC lifecycle policy live; editing the JSON changes the data.
* The cold EXE applies the result and owns entity storage, sockets, and the loop.
* Does NOT own entity storage, transport, damage, or the fixed-tick loop.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-lifecycle.h"
#include "hot-reload/hot-package.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace {

struct NpcLifecycleConfig {
    bool startupEnabled = false;
    std::uint32_t startupCount = 0;
    bool respawnEnabled = true;
    float respawnSeconds = 0.0f;
    std::string startingWeapon = "revolver";
    std::string loadout[8];
    std::uint32_t loadoutCount = 0;
    int health = 100;
};

NpcLifecycleConfig g_config;
bool g_loaded = false;
std::uint64_t g_configWrite = 0;
std::mutex g_mutex;

// Hot formula layer. Editing these multiplies the JSON data live after the next
// hot DLL activation; JSON remains the source of the raw values.
constexpr float kHealthMultiplier = 1.0f;
constexpr float kRespawnDelayMultiplier = 1.0f;

void loadConfigLocked()
{
    std::ifstream f("config/weapons.json");
    if (!f.is_open()) {
        g_loaded = true;
        return;
    }
    try {
        nlohmann::json j;
        f >> j;

        NpcLifecycleConfig cfg = g_loaded ? g_config : NpcLifecycleConfig{};
        if (j.contains("npc_lifecycle") && j["npc_lifecycle"].is_object()) {
            const auto& n = j["npc_lifecycle"];
            if (n.contains("startupNpcsEnabled"))
                cfg.startupEnabled = n["startupNpcsEnabled"].get<bool>();
            if (n.contains("startupNpcCount"))
                cfg.startupCount = n["startupNpcCount"].get<std::uint32_t>();
            if (n.contains("respawnEnabled"))
                cfg.respawnEnabled = n["respawnEnabled"].get<bool>();
            if (n.contains("respawnSeconds"))
                cfg.respawnSeconds = n["respawnSeconds"].get<float>();
            if (n.contains("health"))
                cfg.health = n["health"].get<int>();
            if (n.contains("startingWeapon"))
                cfg.startingWeapon = n["startingWeapon"].get<std::string>();
            cfg.loadoutCount = 0;
            if (n.contains("weaponLoadout") && n["weaponLoadout"].is_array()) {
                for (const auto& w : n["weaponLoadout"]) {
                    if (cfg.loadoutCount >= 8)
                        break;
                    if (w.is_string())
                        cfg.loadout[cfg.loadoutCount++] = w.get<std::string>();
                }
            }
        }
        if (cfg.loadoutCount == 0 && !cfg.startingWeapon.empty())
            cfg.loadout[cfg.loadoutCount++] = cfg.startingWeapon;

        g_config = cfg;
        g_loaded = true;
    } catch (...) {
        // Malformed config keeps the last-good policy.
        g_loaded = true;
    }
}

void ensureConfig()
{
    std::error_code ec;
    auto wt = std::filesystem::last_write_time("config/weapons.json", ec);
    const std::uint64_t writeStamp =
        ec ? 0ull : (std::uint64_t)wt.time_since_epoch().count();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_loaded || writeStamp != g_configWrite) {
        g_configWrite = writeStamp;
        loadConfigLocked();
    }
}

void MIMITA_GAME_CALL npcLifecycle(void* /*host*/, NpcLifecyclePolicyV1* r)
{
    if (!r)
        return;
    ensureConfig();

    std::lock_guard<std::mutex> lock(g_mutex);
    const NpcLifecycleConfig& cfg = g_config;

    r->allowManualSpawn = 1u;
    r->outHealth = (std::uint32_t)(cfg.health > 0 ? cfg.health : 100) *
                   (std::uint32_t)kHealthMultiplier;
    r->outDifficulty = 1.0f;
    std::snprintf(r->startingWeapon, sizeof(r->startingWeapon), "%s",
                  cfg.startingWeapon.c_str());
    r->loadoutCount = 0;
    for (std::uint32_t i = 0; i < cfg.loadoutCount && i < 8; ++i) {
        r->loadout[r->loadoutCount++] =
            (std::uint32_t)gameHash(cfg.loadout[i].c_str());
    }

    switch (r->reason) {
    case GAME_NPC_LIFECYCLE_STARTUP:
    case GAME_NPC_LIFECYCLE_MANUAL_SPAWN:
    default: {
        const std::uint32_t want =
            (cfg.startupEnabled && r->reason == GAME_NPC_LIFECYCLE_STARTUP)
                ? (cfg.startupCount < r->maxSpawn ? cfg.startupCount : r->maxSpawn)
                : 0u;
        r->spawnCount = want;
        r->desiredAutomatic = want;
        r->useSpawnPoints = r->spawnPointCount > 0 ? 1u : 0u;
        break;
    }
    case GAME_NPC_LIFECYCLE_RECONCILE:
    case GAME_NPC_LIFECYCLE_GENERATION: {
        const std::uint32_t desired =
            cfg.startupEnabled ? (cfg.startupCount < r->maxSpawn
                                      ? cfg.startupCount : r->maxSpawn)
                               : 0u;
        r->desiredAutomatic = desired;
        r->spawnCount = desired > r->existingAutomaticCount
                            ? desired - r->existingAutomaticCount : 0u;
        r->destroyAutomatic =
            r->existingAutomaticCount > desired ? 1u : 0u;
        r->useSpawnPoints = r->spawnPointCount > 0 ? 1u : 0u;
        break;
    }
    case GAME_NPC_LIFECYCLE_RESPAWN: {
        r->outSpawnId = r->respawnPlayerId;
        r->outRespawnNow = cfg.respawnEnabled ? 1u : 0u;
        if (cfg.respawnEnabled)
            r->respawnSeconds = cfg.respawnSeconds * kRespawnDelayMultiplier;
        break;
    }
    }

    r->handled = 1u;
    r->result = 1u;
}

const GameCapabilityDescriptorV1 kNpcLifecycleProvider{
    GAME_CAP_NPC_LIFECYCLE, GAME_SIG_NPC_LIFECYCLE, 0,
    reinterpret_cast<void*>(&npcLifecycle), "npc.lifecycle"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcLifecycleProvider{
    kNpcLifecycleProvider};

#endif // MIMITA_GAME_DLL
