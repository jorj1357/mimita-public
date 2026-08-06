#include "devtools/terminal.h"
#include "debug/debug-log.h"
#include "devtools/dev-config.h"
#include "entities/player.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

void registerDebugToggleCommands()
{
    auto& term = Terminal::instance();

    term.registerCommand({
        "debug.collision",
        "Toggle collision debug visualization (0=off, 1=on)",
        "debug.collision <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_COLLISION = !DebugConfig::DEBUG_COLLISION;
            } else {
                DebugConfig::DEBUG_COLLISION = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_COLLISION
                ? "[OK] collision debug enabled"
                : "[OK] collision debug disabled");
        }
    });

    term.registerCommand({
        "debug.movement",
        "Toggle movement debug visualization (0=off, 1=on)",
        "debug.movement <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_MOVEMENT = !DebugConfig::DEBUG_MOVEMENT;
            } else {
                DebugConfig::DEBUG_MOVEMENT = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_MOVEMENT
                ? "[OK] movement debug enabled"
                : "[OK] movement debug disabled");
        }
    });

    term.registerCommand({
        "debug.playerarch",
        "Toggle player architecture overlay (0=off, 1=on)",
        "debug.playerarch <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_PLAYERARCH = !DebugConfig::DEBUG_PLAYERARCH;
            } else {
                DebugConfig::DEBUG_PLAYERARCH = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_PLAYERARCH
                ? "[OK] player architecture overlay enabled"
                : "[OK] player architecture overlay disabled");
        }
    });

    term.registerCommand({
        "debug.sound",
        "Toggle sound debug (0=off, 1=on)",
        "debug.sound <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_SOUND = !DebugConfig::DEBUG_SOUND;
            } else {
                DebugConfig::DEBUG_SOUND = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_SOUND
                ? "[OK] sound debug enabled"
                : "[OK] sound debug disabled");
        }
    });

    term.registerCommand({
        "debug.render",
        "Toggle render debug info (0=off, 1=on)",
        "debug.render <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_RENDER = !DebugConfig::DEBUG_RENDER;
            } else {
                DebugConfig::DEBUG_RENDER = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_RENDER
                ? "[OK] render debug enabled"
                : "[OK] render debug disabled");
        }
    });

    term.registerCommand({
        "debug.physics",
        "Toggle physics debug visualization (0=off, 1=on)",
        "debug.physics <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_PHYSICS = !DebugConfig::DEBUG_PHYSICS;
            } else {
                DebugConfig::DEBUG_PHYSICS = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_PHYSICS
                ? "[OK] physics debug enabled"
                : "[OK] physics debug disabled");
        }
    });

    term.registerCommand({
        "debug.npc",
        "Toggle NPC physics logs and visualization (0=off, 1=on)",
        "debug.npc <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC = !DebugConfig::DEBUG_NPC;
            } else {
                DebugConfig::DEBUG_NPC = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_NPC
                ? "[OK] NPC physics debug enabled"
                : "[OK] NPC physics debug disabled");
        }
    });

    term.registerCommand({
        "debug.reset",
        "Reset all debug flags to defaults",
        "debug.reset",
        [](const std::vector<std::string>&) {
            DebugConfig::ResetAll();
            Terminal::instance().addLog("[OK] all debug flags reset to defaults");
        }
    });

    term.registerCommand({
        "godball_debug",
        "Toggle godball collision debug visualization (0=off, 1=on)",
        "godball_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_GODBALL = !DebugConfig::DEBUG_GODBALL;
            } else {
                DebugConfig::DEBUG_GODBALL = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_GODBALL
                ? "[OK] godball debug enabled"
                : "[OK] godball debug disabled");
        }
    });

    term.registerCommand({
        "godball_hitstop_debug",
        "Toggle godball hitstop/slowmo on impact (0=off, 1=on)",
        "godball_hitstop_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_GODBALL_HITSTOP = !DebugConfig::DEBUG_GODBALL_HITSTOP;
            } else {
                DebugConfig::DEBUG_GODBALL_HITSTOP = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_GODBALL_HITSTOP
                ? "[OK] godball hitstop enabled"
                : "[OK] godball hitstop disabled");
        }
    });

    term.registerCommand({
        "npc_combat_debug",
        "Toggle NPC combat debug logging (0=off, 1=on)",
        "npc_combat_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC_COMBAT = !DebugConfig::DEBUG_NPC_COMBAT;
            } else {
                DebugConfig::DEBUG_NPC_COMBAT = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_NPC_COMBAT
                ? "[OK] NPC combat debug enabled"
                : "[OK] NPC combat debug disabled");
        }
    });

    term.registerCommand({
        "anim_debug_arms",
        "Toggle arm animation debug logging (0=off, 1=on)",
        "anim_debug_arms <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_ANIM_ARMS = !DebugConfig::DEBUG_ANIM_ARMS;
            } else {
                DebugConfig::DEBUG_ANIM_ARMS = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_ANIM_ARMS
                ? "[OK] arm animation debug enabled"
                : "[OK] arm animation debug disabled");
        }
    });

    term.registerCommand({
        "swordsword_debug",
        "Toggle swordsword debug visualization (0=off, 1=on)",
        "swordsword_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_SWORDSWORD = !DebugConfig::DEBUG_SWORDSWORD;
            } else {
                DebugConfig::DEBUG_SWORDSWORD = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_SWORDSWORD
                ? "[OK] swordsword debug enabled"
                : "[OK] swordsword debug disabled");
        }
    });

    term.registerCommand({
        "animation_debug",
        "Toggle animation debug logging (0=off, 1=on)",
        "animation_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_ANIMATION = !DebugConfig::DEBUG_ANIMATION;
            } else {
                DebugConfig::DEBUG_ANIMATION = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_ANIMATION
                ? "[OK] animation debug enabled"
                : "[OK] animation debug disabled");
        }
    });

    term.registerCommand({
        "animation_dump_pose",
        "Dump all body part perfectPose and physical pose to console once",
        "animation_dump_pose",
        [](const std::vector<std::string>&) {
            DebugConfig::DEBUG_ANIMATION = true;
            Terminal::instance().addLog("[OK] animation dump queued (will print next frame)");
        }
    });

    term.registerCommand({
        "rotation_debug",
        "Toggle rotation debug logging (0=off, 1=on). Prints cameraYaw, rootYaw, capsuleYaw, modelYaw, weaponYaw, aimYaw every 0.25s",
        "rotation_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_ROTATION = !DebugConfig::DEBUG_ROTATION;
            } else {
                DebugConfig::DEBUG_ROTATION = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_ROTATION
                ? "[OK] rotation debug enabled"
                : "[OK] rotation debug disabled");
        },
        "2026-06-30",
        CommandCategory::Debug
    });

    term.registerCommand({
        "idle_test",
        "Set idle animation debug strength multiplier",
        "idle_test <multiplier>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[OK] idle_test strength = " + std::to_string(gPlayerProcedural.idleDebugStrength));
                return;
            }
            gPlayerProcedural.idleDebugStrength = std::max(0.0f, (float)std::atof(args[0].c_str()));
            Terminal::instance().addLog("[OK] idle_test strength set to " + std::to_string(gPlayerProcedural.idleDebugStrength));
        }
    });
    term.registerCommand({
        "weapon_collision_debug",
        "Toggle weapon collision debug visualization (0=off, 1=on)",
        "weapon_collision_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) DebugConfig::DEBUG_WEAPON_COLLISION = !DebugConfig::DEBUG_WEAPON_COLLISION;
            else DebugConfig::DEBUG_WEAPON_COLLISION = args[0] != "0";
            Terminal::instance().addLog(DebugConfig::DEBUG_WEAPON_COLLISION ? "[OK] weapon collision debug enabled" : "[OK] weapon collision debug disabled");
        },
        "2026-06-30", CommandCategory::Debug
    });

    term.registerCommand({
        "debug_weapon_hitbox",
        "Draw the equipped weapon's collision capsule wireframe (0=off, 1=on)",
        "debug_weapon_hitbox <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) DebugConfig::DEBUG_WEAPON_HITBOX = !DebugConfig::DEBUG_WEAPON_HITBOX;
            else DebugConfig::DEBUG_WEAPON_HITBOX = args[0] != "0";
            Terminal::instance().addLog(DebugConfig::DEBUG_WEAPON_HITBOX ? "[OK] weapon hitbox shown" : "[OK] weapon hitbox hidden");
        },
        "2026-08-06", CommandCategory::Debug
    });

    term.registerCommand({
        "chat_debug",
        "Toggle chat debug logging (0=off, 1=on)",
        "chat_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_CHAT = !DebugConfig::DEBUG_CHAT;
            } else {
                DebugConfig::DEBUG_CHAT = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_CHAT
                ? "[OK] chat debug enabled"
                : "[OK] chat debug disabled");
        }
    });
}
