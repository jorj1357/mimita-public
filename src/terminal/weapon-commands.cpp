#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "combat/weapon-system.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-data.h"
#include "combat/weapon-config.h"
#include "combat/weapon-registry.h"
#include "network/net_mode.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "entities/player-animation-config.h"

void registerWeaponCommands()
{
    Terminal::instance().registerCommand({
        "shoot", "Fire weapon", "shoot",
        [](const std::vector<std::string>&) {
            Camera& camera = THE_CAMERA;
            Player& player = THE_PLAYER;
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            World& world = THE_WORLD;
            WeaponSystem& weapons = THE_WEAPONS;
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;

            const auto* remotePlayers = mpContext.active
                ? &mpContext.remotePlayers
                : nullptr;
            RevolverShotResult shot = weapons.fire(
                camera, player, npcSystem, world, remotePlayers);
            if (!shot.fired) {
                Terminal::instance().addLog("[WEAPON] dry fire or no active weapon");
                return;
            }

            {
                float pitchKick = GetPlayerSettings().weaponRecoilCameraPitch;
                camera.addPunch(pitchKick, 0.0f);
                if (DebugConfig::DEBUG_RECOIL)
                    Debug::log(Debug::Category::General, "[RECOIL] camera punch=%.2f pitch=%.2f\n",
                               pitchKick, camera.punchPitch);
            }

            if (mpContext.active) {
                const glm::vec3 direction = glm::length(shot.end - shot.start) > 0.001f
                    ? glm::normalize(shot.end - shot.start)
                    : camera.front;
                uint16_t effectFlags =
                    MimitaNet::SHOT_EFFECT_MUZZLE |
                    MimitaNet::SHOT_EFFECT_TRACER |
                    MimitaNet::SHOT_EFFECT_SHOOT_SOUND |
                    MimitaNet::SHOT_EFFECT_WEAPON_TRIGGER;
                uint8_t impactType = MimitaNet::SHOT_IMPACT_NONE;
                uint32_t targetId = 0;
                int damage = 0;
                if (shot.targetIsRemotePlayer) {
                    impactType = MimitaNet::SHOT_IMPACT_ENTITY;
                    targetId = shot.targetId;
                    damage = (int)shot.damage;
                    effectFlags |=
                        MimitaNet::SHOT_EFFECT_ENTITY_IMPACT |
                        MimitaNet::SHOT_EFFECT_BLOOD |
                        MimitaNet::SHOT_EFFECT_HIT_SOUND;
                } else if (shot.hitWorld) {
                    impactType = MimitaNet::SHOT_IMPACT_WORLD;
                    effectFlags |=
                        MimitaNet::SHOT_EFFECT_WORLD_IMPACT |
                        MimitaNet::SHOT_EFFECT_DEBRIS |
                        MimitaNet::SHOT_EFFECT_HIT_SOUND;
                }
                uint8_t netWeapon = MimitaNet::NETWORK_WEAPON_REVOLVER;
                const WeaponDefinition* wdef = weapons.getCurrentDef(player);
                if (wdef) {
                    if (wdef->id == "shotgun")
                        netWeapon = MimitaNet::NETWORK_WEAPON_SHOTGUN;
                    else if (wdef->id == "godball")
                        netWeapon = MimitaNet::NETWORK_WEAPON_GODBALL;
                    else if (wdef->id == "swordsword")
                        netWeapon = MimitaNet::NETWORK_WEAPON_SWORDSWORD;
                    else if (wdef->id == "rocket_launcher")
                        netWeapon = MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER;
                }
                MimitaNet::mpSendShotEvent(
                    mpContext, targetId, damage, shot.damage,
                    effectFlags,
                    netWeapon,
                    impactType,
                    shot.start, shot.end, direction, shot.hitNormal,
                    shot.knockbackImpulse);
            }
            Terminal::instance().addLog("[WEAPON] fired");
        }
    });

    Terminal::instance().registerCommand({
        "reload", "Reload weapon", "reload",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            WeaponSystem& weapons = THE_WEAPONS;
            bool loaded = weapons.reload(player);
            if (DebugConfig::DEBUG_INPUT)
                Debug::log(Debug::Category::General, "[INPUT] action=reload command=reload weapon=%s\n",
                           loaded ? "executed" : "ignored");
            Terminal::instance().addLog(loaded ? "[WEAPON] reload complete" : "[WEAPON] reload unavailable");
        }
    });

    Terminal::instance().registerCommand({
        "openinventory", "Toggle inventory", "openinventory",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            player.inventoryOpen = !player.inventoryOpen;
            Terminal::instance().addLog(player.inventoryOpen ? "[INVENTORY] opened" : "[INVENTORY] closed");
        }
    });

    for (int keySlot = 0; keySlot <= 9; ++keySlot) {
        int slot = keySlot == 0 ? 10 : keySlot;
        std::string name = "equipslot" + std::to_string(keySlot);
        Terminal::instance().registerCommand({
            name, "Equip inventory slot " + std::to_string(slot), name,
            [slot](const std::vector<std::string>&) {
                Player& player = THE_PLAYER;
                WeaponSystem& weapons = THE_WEAPONS;
                if (player.equippedSlot == slot && player.hasValidWeapon) {
                    weapons.unequip(player);
                    GetPlayerSettings().equippedSlot = 0;
                    SavePlayerSettings();
                    Terminal::instance().addLog("[INVENTORY] unequipped slot " + std::to_string(slot));
                } else {
                    weapons.equip(player, slot);
                    GetPlayerSettings().equippedSlot = slot;
                    SavePlayerSettings();
                    Terminal::instance().addLog("[INVENTORY] equipped slot " + std::to_string(slot));
                }
            }
        });
    }

    Terminal::instance().registerCommand({
        "weapon_inspect", "Print active weapon module state", "weapon_inspect",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            WeaponSystem& weapons = THE_WEAPONS;
            weapons.inspect();
        }
    });

    Terminal::instance().registerCommand({
        "weapon_config_reload",
        "Reload config/weapons.json",
        "weapon_config_reload",
        [](const std::vector<std::string>&) {
            WeaponConfig::instance().reloadNow();
            WeaponData::registerBuiltinWeapons();
            Terminal::instance().addLog("[WEAPON CONFIG] Reloaded config/weapons.json");
        },
        std::string(),
        CommandCategory::Weapon
    });

    Terminal::instance().registerCommand({
        "weapon_config_inspect",
        "List registered weapon config data",
        "weapon_config_inspect",
        [](const std::vector<std::string>&) {
            const auto& all = WeaponRegistry::instance().all();
            Terminal::instance().addLog("[WEAPON CONFIG] Registered weapons: " + std::to_string(all.size()));
            for (const auto& pair : all) {
                const WeaponDefinition& def = pair.second;
                Terminal::instance().addLog(
                    "  " + def.id + " slot=" + std::to_string(def.slot) +
                    " model=" + def.modelPath +
                    " damage=" + std::to_string((int)def.damage));
            }
        },
        std::string(),
        CommandCategory::Weapon
    });

    Terminal::instance().registerCommand({
        "animation_config_reload",
        "Reload config/animations.json",
        "animation_config_reload",
        [](const std::vector<std::string>&) {
            if (reloadPlayerProceduralConfig()) {
                Terminal::instance().addLog("[ANIMATION CONFIG] Reloaded config/animations.json");
            } else {
                Terminal::instance().addLog("[ANIMATION CONFIG] Reload failed");
            }
        },
        std::string(),
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "animation_config_inspect",
        "List loaded animation config counts",
        "animation_config_inspect",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(
                "[ANIMATION CONFIG] clips=" + std::to_string(gPlayerProcedural.layers.animations.size()) +
                " weaponPoses=" + std::to_string(gPlayerProcedural.weaponPoses.size()));
            for (const auto& pair : gPlayerProcedural.layers.animations)
                Terminal::instance().addLog("  clip " + pair.first);
        },
        std::string(),
        CommandCategory::Player
    });

}

// Helper: split a comma-separated string into floats.
static bool parseFloats(const std::string& s, std::vector<float>& out, int expected)
{
    out.clear();
    size_t start = 0;
    while (out.size() < (size_t)expected) {
        size_t end = s.find(',', start);
        if (end == std::string::npos) end = s.size();
        try { out.push_back(std::stof(s.substr(start, end - start))); }
        catch (...) { return false; }
        if (end == s.size()) break;
        start = end + 1;
    }
    return out.size() == (size_t)expected;
}

static void registerWorldXhCommands()
{
    auto& term = Terminal::instance();

    term.registerCommand({
        "world_xh_alpha",
        "Set world-space crosshair alpha. 0=opaque, 1=invisible. Range [0,1].",
        "world_xh_alpha <0-1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] alpha=" + std::to_string(DebugConfig::WORLD_XH_ALPHA));
                return;
            }
            float v = std::stof(args[0]);
            DebugConfig::WORLD_XH_ALPHA = std::max(0.0f, std::min(1.0f, v));
            Terminal::instance().addLog(
                "[OK] world_xh_alpha=" + std::to_string(DebugConfig::WORLD_XH_ALPHA));
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_length",
        "Set world-space crosshair arm length multiplier. 1=default. Clamped >= 0.",
        "world_xh_length <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] length=" + std::to_string(DebugConfig::WORLD_XH_LENGTH));
                return;
            }
            DebugConfig::WORLD_XH_LENGTH = std::max(0.0f, std::stof(args[0]));
            Terminal::instance().addLog(
                "[OK] world_xh_length=" + std::to_string(DebugConfig::WORLD_XH_LENGTH));
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_gap",
        "Set world-space crosshair center gap multiplier. 1=default. Negative allowed.",
        "world_xh_gap <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] gap=" + std::to_string(DebugConfig::WORLD_XH_GAP));
                return;
            }
            DebugConfig::WORLD_XH_GAP = std::stof(args[0]);
            Terminal::instance().addLog(
                "[OK] world_xh_gap=" + std::to_string(DebugConfig::WORLD_XH_GAP));
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_thickness",
        "Set world-space crosshair arm thickness multiplier. 1=default. Clamped >= 0.",
        "world_xh_thickness <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] thickness=" + std::to_string(DebugConfig::WORLD_XH_THICKNESS));
                return;
            }
            DebugConfig::WORLD_XH_THICKNESS = std::max(0.0f, std::stof(args[0]));
            Terminal::instance().addLog(
                "[OK] world_xh_thickness=" + std::to_string(DebugConfig::WORLD_XH_THICKNESS));
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_color",
        "Set world-space crosshair RGB. Format: R,G,B (each 0-1).",
        "world_xh_color <R,G,B>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[WORLD_XH] color=" +
                    std::to_string(DebugConfig::WORLD_XH_R) + "," +
                    std::to_string(DebugConfig::WORLD_XH_G) + "," +
                    std::to_string(DebugConfig::WORLD_XH_B));
                return;
            }
            std::vector<float> vals;
            if (!parseFloats(args[0], vals, 3)) {
                Terminal::instance().addLog("[ERROR] usage: world_xh_color R,G,B");
                return;
            }
            DebugConfig::WORLD_XH_R = std::max(0.0f, std::min(1.0f, vals[0]));
            DebugConfig::WORLD_XH_G = std::max(0.0f, std::min(1.0f, vals[1]));
            DebugConfig::WORLD_XH_B = std::max(0.0f, std::min(1.0f, vals[2]));
            Terminal::instance().addLog("[OK] world_xh_color set");
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_outline",
        "Enable/disable world-space crosshair outline. 0=off, 1=on.",
        "world_xh_outline <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] outline=" + std::to_string((int)DebugConfig::WORLD_XH_OUTLINE));
                return;
            }
            DebugConfig::WORLD_XH_OUTLINE = args[0] != "0";
            Terminal::instance().addLog(
                DebugConfig::WORLD_XH_OUTLINE
                ? "[OK] world_xh_outline enabled"
                : "[OK] world_xh_outline disabled");
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_outline_alpha",
        "Set world-space crosshair outline alpha. 0=opaque, 1=invisible. Range [0,1].",
        "world_xh_outline_alpha <0-1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] outline_alpha=" + std::to_string(DebugConfig::WORLD_XH_OUTLINE_ALPHA));
                return;
            }
            DebugConfig::WORLD_XH_OUTLINE_ALPHA = std::max(0.0f, std::min(1.0f, std::stof(args[0])));
            Terminal::instance().addLog(
                "[OK] world_xh_outline_alpha=" + std::to_string(DebugConfig::WORLD_XH_OUTLINE_ALPHA));
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_centerdot",
        "Show/hide world-space crosshair center dot. 0=hidden, 1=visible.",
        "world_xh_centerdot <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] centerdot=" + std::to_string((int)DebugConfig::WORLD_XH_CENTERDOT));
                return;
            }
            DebugConfig::WORLD_XH_CENTERDOT = args[0] != "0";
            Terminal::instance().addLog(
                DebugConfig::WORLD_XH_CENTERDOT
                ? "[OK] world_xh_centerdot enabled"
                : "[OK] world_xh_centerdot disabled");
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_dynamic",
        "Enable/disable distance-based crosshair scaling. 0=fixed size, 1=dynamic.",
        "world_xh_dynamic <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] dynamic=" + std::to_string((int)DebugConfig::WORLD_XH_DYNAMIC));
                return;
            }
            DebugConfig::WORLD_XH_DYNAMIC = args[0] != "0";
            Terminal::instance().addLog(
                DebugConfig::WORLD_XH_DYNAMIC
                ? "[OK] world_xh_dynamic enabled (distance-based scaling)"
                : "[OK] world_xh_dynamic disabled (fixed size)");
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_maxsize",
        "Set maximum crosshair scale multiplier. Default 2.0. Clamped >= 0.",
        "world_xh_maxsize <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] maxsize=" + std::to_string(DebugConfig::WORLD_XH_MAXSIZE));
                return;
            }
            DebugConfig::WORLD_XH_MAXSIZE = std::max(0.0f, std::stof(args[0]));
            Terminal::instance().addLog(
                "[OK] world_xh_maxsize=" + std::to_string(DebugConfig::WORLD_XH_MAXSIZE));
        },
        "2026-07-02", CommandCategory::Weapon
    });

    term.registerCommand({
        "world_xh_minsize",
        "Set minimum crosshair scale multiplier. Default 0.5. Clamped >= 0.",
        "world_xh_minsize <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[WORLD_XH] minsize=" + std::to_string(DebugConfig::WORLD_XH_MINSIZE));
                return;
            }
            DebugConfig::WORLD_XH_MINSIZE = std::max(0.0f, std::stof(args[0]));
            Terminal::instance().addLog(
                "[OK] world_xh_minsize=" + std::to_string(DebugConfig::WORLD_XH_MINSIZE));
        },
        "2026-07-02", CommandCategory::Weapon
    });
}

void registerWeaponDebugCommand()
{
    registerWorldXhCommands();
    Terminal::instance().registerCommand({
        "wpn_shot_line",
        "Toggle weapon shot debug line (0=off, 1=on). "
        "Shows red beam and sphere. World-space crosshair is independent.",
        "wpn_shot_line <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_WPN_SHOT_LINE = !DebugConfig::DEBUG_WPN_SHOT_LINE;
            } else {
                DebugConfig::DEBUG_WPN_SHOT_LINE = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_WPN_SHOT_LINE
                ? "[OK] wpn_shot_line enabled (red beam + sphere visible)"
                : "[OK] wpn_shot_line disabled (world-space crosshair still visible)");
        },
        "2026-07-02",
        CommandCategory::Weapon
    });

    Terminal::instance().registerCommand({
        "weapon_debug",
        "Toggle weapon debug logging (0=off, 1=on). Prints shotgun stats and pellet directions on each shot.",
        "weapon_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                WeaponFire::setWeaponDebug(!WeaponFire::weaponDebugEnabled());
            } else {
                WeaponFire::setWeaponDebug(args[0] != "0");
            }
            Terminal::instance().addLog(
                WeaponFire::weaponDebugEnabled()
                ? "[OK] weapon debug enabled"
                : "[OK] weapon debug disabled");
        },
        "2026-06-30",
        CommandCategory::Weapon
    });
}

static const char* WORLD_XH_CONFIG_PATH = "config/crosshair-world.json";

void loadWorldCrosshairConfig()
{
    using json = nlohmann::json;
    auto& term = Terminal::instance();

    printf("[WORLD_XH] Loading world crosshair config...\n");
    printf("[WORLD_XH] Config path: %s\n", WORLD_XH_CONFIG_PATH);
    term.addLog("[WORLD_XH] Loading world crosshair config...");

    std::string fullPath = WORLD_XH_CONFIG_PATH;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        printf("[WORLD_XH] ERROR: Could not open config file at '%s'. Using defaults.\n", fullPath.c_str());
        term.addLog(std::string("[WORLD_XH] ERROR: Could not open config file. Using defaults."));
        return;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        printf("[WORLD_XH] ERROR: Failed to parse JSON: %s\n", e.what());
        term.addLog(std::string("[WORLD_XH] ERROR: Failed to parse JSON: ") + e.what());
        return;
    }

    printf("[WORLD_XH] JSON parsed successfully. Checking fields...\n");

    auto apply = [&](const std::string& key, const std::string& cmdPrefix, const std::string& value) {
        std::string fullCmd = cmdPrefix + " " + value;
        printf("[WORLD_XH] Executing: %s\n", fullCmd.c_str());
        term.execute(fullCmd);
        printf("[WORLD_XH] Loaded: %s = %s\n", key.c_str(), value.c_str());
        term.addLog(std::string("[WORLD_XH] Loaded: ") + key);
    };

    // Scalar float fields
    struct FloatField { const char* key; const char* cmd; };
    static const FloatField floatFields[] = {
        {"world_xh_alpha",        "world_xh_alpha"},
        {"world_xh_length",       "world_xh_length"},
        {"world_xh_gap",          "world_xh_gap"},
        {"world_xh_thickness",    "world_xh_thickness"},
        {"world_xh_outline_alpha","world_xh_outline_alpha"},
        {"world_xh_minsize",      "world_xh_minsize"},
        {"world_xh_maxsize",      "world_xh_maxsize"},
    };

    for (const auto& f : floatFields) {
        if (j.contains(f.key)) {
            try {
                float v = j[f.key].get<float>();
                apply(f.key, f.cmd, std::to_string(v));
            } catch (const std::exception& e) {
                printf("[WORLD_XH] WARNING: skipping invalid %s (%s)\n", f.key, e.what());
                term.addLog(std::string("[WORLD_XH] WARNING: skipping invalid ") + f.key);
            }
        } else {
            printf("[WORLD_XH] Field not present (using default): %s\n", f.key);
        }
    }

    // Boolean fields
    struct BoolField { const char* key; const char* cmd; };
    static const BoolField boolFields[] = {
        {"world_xh_outline",   "world_xh_outline"},
        {"world_xh_dynamic",   "world_xh_dynamic"},
        {"world_xh_centerdot", "world_xh_centerdot"},
    };

    for (const auto& b : boolFields) {
        if (j.contains(b.key)) {
            try {
                int v = j[b.key].get<int>();
                apply(b.key, b.cmd, std::to_string(v));
            } catch (const std::exception& e) {
                printf("[WORLD_XH] WARNING: skipping invalid %s (%s)\n", b.key, e.what());
                term.addLog(std::string("[WORLD_XH] WARNING: skipping invalid ") + b.key);
            }
        } else {
            printf("[WORLD_XH] Field not present (using default): %s\n", b.key);
        }
    }

    // Color field — expects array of 3 floats
    if (j.contains("world_xh_color")) {
        try {
            auto& arr = j["world_xh_color"];
            if (arr.is_array() && arr.size() >= 3) {
                float r = arr[0].get<float>();
                float g = arr[1].get<float>();
                float b = arr[2].get<float>();
                std::string val = std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b);
                apply("world_xh_color", "world_xh_color", val);
            } else {
                printf("[WORLD_XH] WARNING: world_xh_color must be an array of 3 floats\n");
                term.addLog("[WORLD_XH] WARNING: world_xh_color must be an array of 3 floats");
            }
        } catch (const std::exception& e) {
            printf("[WORLD_XH] WARNING: skipping invalid world_xh_color (%s)\n", e.what());
            term.addLog("[WORLD_XH] WARNING: skipping invalid world_xh_color");
        }
    } else {
        printf("[WORLD_XH] Field not present (using default): world_xh_color\n");
    }

    printf("[WORLD_XH] World crosshair config loaded successfully.\n");
    term.addLog("[WORLD_XH] World crosshair config loaded successfully.");
}

void applyStartupDefaults()
{
    auto& term = Terminal::instance();

    printf("[WORLD_XH] Applying startup defaults...\n");

    // Disable the weapon shot debug line by default.
    // The red debug beam and sphere are noisy; the world-space crosshair is sufficient.
    printf("[WORLD_XH] Executing: wpn_shot_line 0\n");
    term.execute("wpn_shot_line 0");
    printf("[WORLD_XH] Applied: wpn_shot_line = false\n");
    term.addLog("[WORLD_XH] Applied: wpn_shot_line = false");
}
