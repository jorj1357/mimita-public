// 08 16 2026, 18 00
/* purpose
* Hot-reloadable JSON source of truth for NPC difficulty (config/npc-difficulty.json).
* Exposes accuracy, damage, fire rate, force-hit toggles, weapon loadout, panic,
* and movement expressiveness for NPC combat.
* Follows the GameplayConfig pollReload pattern: re-reads the file when it changes on disk.
* Does NOT own NPC state machines or weapon switching logic.
* Does NOT write config files unless save() is explicitly called by a terminal command.
* Does NOT touch the hot-reload DLL system.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "physics/movement/movement-types.h"

struct NpcDifficultySettings {
    float maxAngularErrorDegrees = 4.0f;  // aim miss-cone in degrees; lower = deadlier
    float difficultyErrorScale = 0.5f;    // 0-1: how much NPC difficulty shrinks the cone
    float damageMultiplier = 1.0f;        // scales weapon damage when an NPC hits the player
    float fireDelayMin = 0.3f;            // fastest shot interval override (0 = use weapon)
    float fireDelayMax = 0.7f;            // slowest shot interval
    float aggressionBonus = 0.05f;        // pushes shot timing toward fireDelayMin
    float npcHitRadius = 0.4f;            // NPC bullet radius; thin so aim error actually matters
    bool forceHit = false;                // debug: zero aim error, every shot connects
    bool npcDebugVisuals = false;         // draw NPC aim/LOS debug lines in-game

    // Facing/turn tuning (hot-reloaded). The NPC switches between two facing
    // modes: "aim at target" (dominant, long stretches) and "face movement"
    // (brief). Turn speed no longer depends on being grounded or in the air.
    float turnSpeed = 270.0f;             // degrees per second the gun/model can rotate
    float aimAtTargetMin = 5.0f;          // min seconds spent facing the target per cycle
    float aimAtTargetMax = 10.0f;         // max seconds spent facing the target per cycle
    float faceMovementMin = 0.5f;         // min seconds spent facing movement per cycle
    float faceMovementMax = 1.5f;         // max seconds spent facing movement per cycle

    // Which config/movement/*.json preset NPCs use for their physics.
    // "follow" (default) = same global config as the player. Any preset name
    // (e.g. "default", "source", "counterstrike") overrides NPC physics.
    std::string movementPreset = "follow";

    // Weapon loadout: which weapons an NPC can hold and switch between.
    // The NPC spawns with startingWeapon and switches based on distance.
    std::vector<std::string> weaponLoadout = {"revolver", "shotgun", "rocket_launcher", "grenade_launcher"};
    std::string startingWeapon = "revolver";
    float switchCooldown = 2.0f;          // min seconds between weapon switches
    float closeSwitchDist = 8.0f;         // below this: prefer shotgun
    float farSwitchDist = 20.0f;          // above this: prefer rocket/grenade

    // Panic toggle: when enabled, getting hit forces the NPC into Recover state
    // (freezes movement). Disable for harder-to-hit NPCs.
    bool hitReactionEnabled = false;
    float hitReactionDurationScale = 1.0f;

    // Movement expressiveness (multipliers on existing behavior frequencies).
    // 1.0 = current default, higher = more frequent, lower = less frequent.
    float dashChance = 1.0f;
    float downDashChance = 1.0f;
    float freezeChance = 1.0f;
    float movementNoiseScale = 1.0f;
    float jukeFrequency = 1.0f;

    // Force a specific weapon. When non-empty, NPCs always use this weapon
    // and ignore distance-based switching. Set to a weapon id to test it.
    std::string forceWeapon = "";

    // Mirror movement: NPCs alternate between normal AI movement and replaying
    // the player's recent movement inputs (direction, dash, freeze, jump) while
    // continuing to aim at the player.
    bool mirrorMovementEnabled = false;
    float mirrorNormalDuration = 5.0f;     // seconds in normal phase
    float mirrorReplayDuration = 5.0f;     // seconds replaying player inputs
    float mirrorHistorySeconds = 5.0f;     // how far back to look in player history
    bool mirrorKeepAimingAtTarget = true;  // keep aiming at player during mirror
    bool mirrorDashEnabled = true;         // mirror player dashes
    bool mirrorDownDashEnabled = true;     // mirror player down-dashes
    bool mirrorFreezeEnabled = true;       // mirror player freezes
    bool mirrorJumpEnabled = true;         // mirror player jumps
};

class NpcDifficultyConfig {
public:
    static NpcDifficultyConfig& instance();

    bool load(const std::string& path = "config/npc-difficulty.json");
    bool pollReload();
    bool save(const std::string& path = "config/npc-difficulty.json");
    uint64_t revision() const { return mRevision; }

    const NpcDifficultySettings& settings() const { return mData; }
    NpcDifficultySettings& settings() { return mData; }

    // Returns the MovementConfig NPCs should use for physics, or nullptr when
    // movementPreset is "follow" (NPCs use the player's global config).
    const MovementConfig* npcMovementConfig() const
    {
        return mHasNpcMovement ? &mNpcMovement : nullptr;
    }

    const std::string& npcMovementPresetName() const { return mData.movementPreset; }

private:
    NpcDifficultyConfig() = default;

    NpcDifficultySettings mData;
    nlohmann::json mRoot;
    std::string mPath = "config/npc-difficulty.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
    uint64_t mRevision = 0;

    // Cached NPC movement preset (only used when movementPreset != "follow").
    MovementConfig mNpcMovement;
    bool mHasNpcMovement = false;
    std::string mNpcPresetPath;
    std::filesystem::file_time_type mNpcPresetWrite{};
};
