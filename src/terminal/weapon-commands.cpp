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

// ── World crosshair config loader ──
// Loads config/crosshair-world.json and directly assigns to DebugConfig variables.
// Uses direct assignment instead of Terminal::execute() to eliminate parsing issues.

static const char* WORLD_XH_CONFIG_PATH = "config/crosshair-world.json";

static bool loadWorldCrosshairFromJSON()
{
    using json = nlohmann::json;

    printf("[WorldCrosshair] Loaded config:\n{\n");

    std::ifstream file(WORLD_XH_CONFIG_PATH);
    if (!file.is_open()) {
        printf("  [ERROR] Could not open: %s\n", WORLD_XH_CONFIG_PATH);
        printf("}\n");
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        printf("  [ERROR] Parse failed: %s\n", e.what());
        printf("}\n");
        return false;
    }

    // Float fields
    auto loadFloat = [&](const char* key, float& var) {
        if (j.contains(key)) {
            float v = j[key].get<float>();
            var = v;
            printf("  %s: %.4f\n", key, (double)v);
        } else {
            printf("  %s: (default %.4f)\n", key, (double)var);
        }
    };

    loadFloat("world_xh_alpha",        DebugConfig::WORLD_XH_ALPHA);
    loadFloat("world_xh_length",       DebugConfig::WORLD_XH_LENGTH);
    loadFloat("world_xh_gap",          DebugConfig::WORLD_XH_GAP);
    loadFloat("world_xh_thickness",    DebugConfig::WORLD_XH_THICKNESS);
    loadFloat("world_xh_outline_alpha",DebugConfig::WORLD_XH_OUTLINE_ALPHA);
    loadFloat("world_xh_minsize",      DebugConfig::WORLD_XH_MINSIZE);
    loadFloat("world_xh_maxsize",      DebugConfig::WORLD_XH_MAXSIZE);

    // Boolean fields
    auto loadBool = [&](const char* key, bool& var) {
        if (j.contains(key)) {
            var = j[key].get<int>() != 0;
            printf("  %s: %s\n", key, var ? "true" : "false");
        } else {
            printf("  %s: (default %s)\n", key, var ? "true" : "false");
        }
    };

    loadBool("world_xh_outline",   DebugConfig::WORLD_XH_OUTLINE);
    loadBool("world_xh_dynamic",   DebugConfig::WORLD_XH_DYNAMIC);
    loadBool("world_xh_centerdot", DebugConfig::WORLD_XH_CENTERDOT);

    // Color field — expects [R, G, B] each 0-1
    if (j.contains("world_xh_color")) {
        auto& arr = j["world_xh_color"];
        if (arr.is_array() && arr.size() >= 3) {
            DebugConfig::WORLD_XH_R = arr[0].get<float>();
            DebugConfig::WORLD_XH_G = arr[1].get<float>();
            DebugConfig::WORLD_XH_B = arr[2].get<float>();
            printf("  world_xh_color: [%.4f, %.4f, %.4f]\n",
                (double)DebugConfig::WORLD_XH_R,
                (double)DebugConfig::WORLD_XH_G,
                (double)DebugConfig::WORLD_XH_B);
        } else {
            printf("  world_xh_color: (invalid array, keeping default)\n");
        }
    } else {
        printf("  world_xh_color: (default [%.4f, %.4f, %.4f])\n",
            (double)DebugConfig::WORLD_XH_R,
            (double)DebugConfig::WORLD_XH_G,
            (double)DebugConfig::WORLD_XH_B);
    }

    printf("}\n");
    return true;
}

static void logCurrentCrosshairState()
{
    printf("[WorldCrosshair] Current runtime values:\n{\n");
    printf("  world_xh_alpha: %.4f\n", (double)DebugConfig::WORLD_XH_ALPHA);
    printf("  world_xh_color: [%.4f, %.4f, %.4f]\n",
        (double)DebugConfig::WORLD_XH_R,
        (double)DebugConfig::WORLD_XH_G,
        (double)DebugConfig::WORLD_XH_B);
    printf("  world_xh_gap: %.4f\n", (double)DebugConfig::WORLD_XH_GAP);
    printf("  world_xh_length: %.4f\n", (double)DebugConfig::WORLD_XH_LENGTH);
    printf("  world_xh_outline: %s\n", DebugConfig::WORLD_XH_OUTLINE ? "true" : "false");
    printf("  world_xh_outline_alpha: %.4f\n", (double)DebugConfig::WORLD_XH_OUTLINE_ALPHA);
    printf("  world_xh_thickness: %.4f\n", (double)DebugConfig::WORLD_XH_THICKNESS);
    printf("  world_xh_dynamic: %s\n", DebugConfig::WORLD_XH_DYNAMIC ? "true" : "false");
    printf("  world_xh_minsize: %.4f\n", (double)DebugConfig::WORLD_XH_MINSIZE);
    printf("  world_xh_maxsize: %.4f\n", (double)DebugConfig::WORLD_XH_MAXSIZE);
    printf("  world_xh_centerdot: %s\n", DebugConfig::WORLD_XH_CENTERDOT ? "true" : "false");
    printf("}\n");
}

void loadWorldCrosshairConfig()
{
    printf("[WorldCrosshair] Loading config from: %s\n", WORLD_XH_CONFIG_PATH);
    if (loadWorldCrosshairFromJSON()) {
        printf("[WorldCrosshair] Config loaded and applied.\n");
        logCurrentCrosshairState();
    } else {
        printf("[WorldCrosshair] Using default values.\n");
    }
}

void applyStartupDefaults()
{
    printf("[WorldCrosshair] Applying startup defaults...\n");

    DebugConfig::DEBUG_WPN_SHOT_LINE = false;
    printf("[WorldCrosshair] DEBUG_WPN_SHOT_LINE = false  (wpn_shot_line disabled)\n");
}

// Hot-reload command: re-reads the JSON and applies values immediately.
// Register this after Terminal is initialized.
void registerWorldXhReloadCommand()
{
    Terminal::instance().registerCommand({
        "world_xh_reload",
        "Reload config/crosshair-world.json and apply changes immediately.",
        "world_xh_reload",
        [](const std::vector<std::string>&) {
            printf("[WorldCrosshair] Hot reload triggered.\n");
            Terminal::instance().addLog("[WorldCrosshair] Reloading config...");
            if (loadWorldCrosshairFromJSON()) {
                printf("[WorldCrosshair] Hot reload applied.\n");
                logCurrentCrosshairState();
                Terminal::instance().addLog("[WorldCrosshair] Config reloaded.");
            } else {
                Terminal::instance().addLog("[WorldCrosshair] Reload failed, using previous values.");
            }
        }
    });
}
