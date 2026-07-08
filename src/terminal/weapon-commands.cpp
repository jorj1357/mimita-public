#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
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

    Terminal::instance().registerCommand({
        "hafsdebug", "Toggle HAFS debug logging", "hafsdebug [0|1]",
        [](const std::vector<std::string>& args) {
            extern bool gHafsDebug;
            bool val = args.empty() ? !gHafsDebug : (args[0] != "0");
            gHafsDebug = val;
            Terminal::instance().addLog(std::string("[HAFS] debug = ") + (val ? "1" : "0"));
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

static float clamp01(float v)
{
    return std::max(0.0f, std::min(1.0f, v));
}

static void setWorldXhAlpha(float v) { DebugConfig::WORLD_XH_ALPHA = clamp01(v); }
static void setWorldXhLength(float v) { DebugConfig::WORLD_XH_LENGTH = std::max(0.0f, v); }
static void setWorldXhGap(float v) { DebugConfig::WORLD_XH_GAP = v; }
static void setWorldXhThickness(float v) { DebugConfig::WORLD_XH_THICKNESS = std::max(0.0f, v); }
static void setWorldXhOutlineAlpha(float v) { DebugConfig::WORLD_XH_OUTLINE_ALPHA = clamp01(v); }
static void setWorldXhMinSize(float v) { DebugConfig::WORLD_XH_MINSIZE = std::max(0.0f, v); }
static void setWorldXhMaxSize(float v) { DebugConfig::WORLD_XH_MAXSIZE = std::max(0.0f, v); }
static void setWorldXhColor(float r, float g, float b)
{
    DebugConfig::WORLD_XH_R = clamp01(r);
    DebugConfig::WORLD_XH_G = clamp01(g);
    DebugConfig::WORLD_XH_B = clamp01(b);
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
            setWorldXhAlpha(std::stof(args[0]));
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
            setWorldXhLength(std::stof(args[0]));
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
            setWorldXhGap(std::stof(args[0]));
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
            setWorldXhThickness(std::stof(args[0]));
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
            setWorldXhColor(vals[0], vals[1], vals[2]);
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
            setWorldXhOutlineAlpha(std::stof(args[0]));
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
            setWorldXhMaxSize(std::stof(args[0]));
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
            setWorldXhMinSize(std::stof(args[0]));
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
static int64_t gWorldXhLastModified = 0;
static bool gWorldXhWatchLogged = false;

struct WorldXhState {
    float alpha = 0.0f;
    float length = 1.0f;
    float gap = 1.0f;
    float thickness = 1.0f;
    float outlineAlpha = 0.0f;
    float minSize = 0.5f;
    float maxSize = 2.0f;
    float r = 1.0f;
    float g = 0.5f;
    float b = 0.0f;
    bool outline = false;
    bool dynamic = true;
    bool centerDot = false;
    // Aim trail
    bool trailEnabled = false;
    float trailLifetimeTicks = 90.0f;
    float trailR = 0.2f;
    float trailG = 1.0f;
    float trailB = 1.0f;
    float trailAlpha = 0.5f;
    float trailSize = 0.1f;
    float trailMaxPoints = 128.0f;
    float trailSpawnInterval = 1.0f;
    bool trailFade = true;
    float trailShape = 0.0f;
    float trailMode = 0.0f;
};

static int64_t worldXhModifiedTime()
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(WORLD_XH_CONFIG_PATH, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

static WorldXhState currentWorldXhState()
{
    WorldXhState s;
    s.alpha = DebugConfig::WORLD_XH_ALPHA;
    s.length = DebugConfig::WORLD_XH_LENGTH;
    s.gap = DebugConfig::WORLD_XH_GAP;
    s.thickness = DebugConfig::WORLD_XH_THICKNESS;
    s.outlineAlpha = DebugConfig::WORLD_XH_OUTLINE_ALPHA;
    s.minSize = DebugConfig::WORLD_XH_MINSIZE;
    s.maxSize = DebugConfig::WORLD_XH_MAXSIZE;
    s.r = DebugConfig::WORLD_XH_R;
    s.g = DebugConfig::WORLD_XH_G;
    s.b = DebugConfig::WORLD_XH_B;
    s.outline = DebugConfig::WORLD_XH_OUTLINE;
    s.dynamic = DebugConfig::WORLD_XH_DYNAMIC;
    s.centerDot = DebugConfig::WORLD_XH_CENTERDOT;
    s.trailEnabled = DebugConfig::WORLD_XH_TRAIL_ENABLED;
    s.trailLifetimeTicks = (float)DebugConfig::WORLD_XH_TRAIL_LIFETIME_TICKS;
    s.trailR = DebugConfig::WORLD_XH_TRAIL_R;
    s.trailG = DebugConfig::WORLD_XH_TRAIL_G;
    s.trailB = DebugConfig::WORLD_XH_TRAIL_B;
    s.trailAlpha = DebugConfig::WORLD_XH_TRAIL_ALPHA;
    s.trailSize = DebugConfig::WORLD_XH_TRAIL_SIZE;
    s.trailMaxPoints = (float)DebugConfig::WORLD_XH_TRAIL_MAX_POINTS;
    s.trailSpawnInterval = (float)DebugConfig::WORLD_XH_TRAIL_SPAWN_INTERVAL;
    s.trailFade = DebugConfig::WORLD_XH_TRAIL_FADE;
    s.trailShape = (float)DebugConfig::WORLD_XH_TRAIL_SHAPE;
    s.trailMode = (float)DebugConfig::WORLD_XH_TRAIL_MODE;
    return s;
}

static void logWorldXh(const std::string& message)
{
    Debug::warn(Debug::Category::Weapons, "%s\n", message.c_str());
    Terminal::instance().addLog(message);
}

static void logWorldXhWatchOnce()
{
    if (gWorldXhWatchLogged) return;
    gWorldXhWatchLogged = true;
    logWorldXh("[WorldCrosshair] Watching: crosshair-world.json");
}

static void applyWorldXhState(const WorldXhState& s)
{
    setWorldXhAlpha(s.alpha);
    setWorldXhLength(s.length);
    setWorldXhGap(s.gap);
    setWorldXhThickness(s.thickness);
    setWorldXhOutlineAlpha(s.outlineAlpha);
    setWorldXhMinSize(s.minSize);
    setWorldXhMaxSize(s.maxSize);
    setWorldXhColor(s.r, s.g, s.b);
    DebugConfig::WORLD_XH_OUTLINE = s.outline;
    DebugConfig::WORLD_XH_DYNAMIC = s.dynamic;
    DebugConfig::WORLD_XH_CENTERDOT = s.centerDot;
    DebugConfig::WORLD_XH_TRAIL_ENABLED = s.trailEnabled;
    DebugConfig::WORLD_XH_TRAIL_LIFETIME_TICKS = (int)s.trailLifetimeTicks;
    DebugConfig::WORLD_XH_TRAIL_R = s.trailR;
    DebugConfig::WORLD_XH_TRAIL_G = s.trailG;
    DebugConfig::WORLD_XH_TRAIL_B = s.trailB;
    DebugConfig::WORLD_XH_TRAIL_ALPHA = s.trailAlpha;
    DebugConfig::WORLD_XH_TRAIL_SIZE = s.trailSize;
    DebugConfig::WORLD_XH_TRAIL_MAX_POINTS = (int)s.trailMaxPoints;
    DebugConfig::WORLD_XH_TRAIL_SPAWN_INTERVAL = (int)s.trailSpawnInterval;
    DebugConfig::WORLD_XH_TRAIL_FADE = s.trailFade;
    DebugConfig::WORLD_XH_TRAIL_SHAPE = (int)s.trailShape;
    DebugConfig::WORLD_XH_TRAIL_MODE = (int)s.trailMode;
}

static bool loadWorldCrosshairFromJSON()
{
    using json = nlohmann::json;

    std::ifstream file(WORLD_XH_CONFIG_PATH);
    if (!file.is_open()) {
        logWorldXh(std::string("[WorldCrosshair] ERROR: could not open ") + WORLD_XH_CONFIG_PATH);
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        logWorldXh(std::string("[WorldCrosshair] Parse error in ") +
                   WORLD_XH_CONFIG_PATH + ": " + e.what());
        return false;
    }

    WorldXhState next = currentWorldXhState();
    std::vector<const char*> applied;

    auto readFloat = [&](const char* key, float& field, auto normalize) -> bool {
        if (!j.contains(key)) return true;
        try {
            field = normalize(j.at(key).get<float>());
            applied.push_back(key);
            return true;
        } catch (const std::exception& e) {
            logWorldXh(std::string("[WorldCrosshair] Parse error for ") +
                       key + ": " + e.what());
            return false;
        }
    };

    auto identity = [](float v) { return v; };
    auto nonNegative = [](float v) { return std::max(0.0f, v); };
    if (!readFloat("world_xh_alpha", next.alpha, clamp01)) return false;
    if (!readFloat("world_xh_length", next.length, nonNegative)) return false;
    if (!readFloat("world_xh_gap", next.gap, identity)) return false;
    if (!readFloat("world_xh_thickness", next.thickness, nonNegative)) return false;
    if (!readFloat("world_xh_outline_alpha", next.outlineAlpha, clamp01)) return false;
    if (!readFloat("world_xh_minsize", next.minSize, nonNegative)) return false;
    if (!readFloat("world_xh_maxsize", next.maxSize, nonNegative)) return false;

    auto readBool = [&](const char* key, bool& field) -> bool {
        if (!j.contains(key)) return true;
        const json& value = j.at(key);
        if (value.is_boolean()) {
            field = value.get<bool>();
        } else if (value.is_number()) {
            field = value.get<float>() != 0.0f;
        } else {
            logWorldXh(std::string("[WorldCrosshair] Parse error for ") +
                       key + ": expected boolean or number");
            return false;
        }
        applied.push_back(key);
        return true;
    };

    if (!readBool("world_xh_outline", next.outline)) return false;
    if (!readBool("world_xh_dynamic", next.dynamic)) return false;
    if (!readBool("world_xh_centerdot", next.centerDot)) return false;

    // Trail fields
    if (!readBool("world_xh_trail_enabled", next.trailEnabled)) return false;
    if (!readFloat("world_xh_trail_lifetime_ticks", next.trailLifetimeTicks, nonNegative)) return false;
    if (!readFloat("world_xh_trail_alpha", next.trailAlpha, clamp01)) return false;
    if (!readFloat("world_xh_trail_size", next.trailSize, nonNegative)) return false;
    if (!readFloat("world_xh_trail_max_points", next.trailMaxPoints, nonNegative)) return false;
    if (!readFloat("world_xh_trail_spawn_interval", next.trailSpawnInterval, nonNegative)) return false;
    if (!readBool("world_xh_trail_fade", next.trailFade)) return false;
    if (!readFloat("world_xh_trail_shape", next.trailShape, nonNegative)) return false;
    if (!readFloat("world_xh_trail_mode", next.trailMode, nonNegative)) return false;

    if (j.contains("world_xh_trail_color")) {
        const json& arr = j.at("world_xh_trail_color");
        if (!arr.is_array() || arr.size() < 3) {
            logWorldXh("[WorldCrosshair] Parse error for world_xh_trail_color: expected [r,g,b]");
            return false;
        }
        try {
            next.trailR = clamp01(arr.at(0).get<float>());
            next.trailG = clamp01(arr.at(1).get<float>());
            next.trailB = clamp01(arr.at(2).get<float>());
            applied.push_back("world_xh_trail_color");
        } catch (const std::exception& e) {
            logWorldXh(std::string("[WorldCrosshair] Parse error for world_xh_trail_color: ") + e.what());
            return false;
        }
    }

    if (j.contains("world_xh_color")) {
        const json& arr = j.at("world_xh_color");
        if (!arr.is_array() || arr.size() < 3) {
            logWorldXh("[WorldCrosshair] Parse error for world_xh_color: expected [r,g,b]");
            return false;
        }
        try {
            next.r = clamp01(arr.at(0).get<float>());
            next.g = clamp01(arr.at(1).get<float>());
            next.b = clamp01(arr.at(2).get<float>());
            applied.push_back("world_xh_color");
        } catch (const std::exception& e) {
            logWorldXh(std::string("[WorldCrosshair] Parse error for world_xh_color: ") + e.what());
            return false;
        }
    }

    applyWorldXhState(next);
    logWorldXh("[WorldCrosshair] Loaded successfully.");
    for (const char* key : applied)
        logWorldXh(std::string("[WorldCrosshair] Applied: ") + key);
    return true;
}

void loadWorldCrosshairConfig()
{
    logWorldXhWatchOnce();
    logWorldXh(std::string("[WorldCrosshair] Loading config from: ") + WORLD_XH_CONFIG_PATH);
    loadWorldCrosshairFromJSON();
    gWorldXhLastModified = worldXhModifiedTime();
}

bool pollWorldCrosshairConfig()
{
    logWorldXhWatchOnce();

    using Clock = std::chrono::steady_clock;
    static Clock::time_point nextCheck;
    const auto now = Clock::now();
    if (now < nextCheck) return false;
    nextCheck = now + std::chrono::milliseconds(100);

    const int64_t current = worldXhModifiedTime();
    if (current == 0) return false;
    if (gWorldXhLastModified == 0) {
        gWorldXhLastModified = current;
        return false;
    }
    if (current == gWorldXhLastModified) return false;

    gWorldXhLastModified = current;
    logWorldXh("[WorldCrosshair] Detected change. Reloading...");
    if (!loadWorldCrosshairFromJSON()) {
        logWorldXh("[WorldCrosshair] Reload failed; keeping previous valid settings.");
        return false;
    }
    return true;
}

void applyStartupDefaults()
{
    DebugConfig::DEBUG_WPN_SHOT_LINE = false;
    Debug::warn(Debug::Category::Weapons,
        "[WorldCrosshair] DEBUG_WPN_SHOT_LINE = false (wpn_shot_line disabled)\n");
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
            logWorldXh("[WorldCrosshair] Detected change. Reloading...");
            if (loadWorldCrosshairFromJSON()) {
                gWorldXhLastModified = worldXhModifiedTime();
            } else {
                logWorldXh("[WorldCrosshair] Reload failed; keeping previous valid settings.");
            }
        }
    });
}
