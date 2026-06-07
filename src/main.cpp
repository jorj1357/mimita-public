// C:\important\quiet\n\mimita-priv-v7\src\main.cpp
// feb 10 2026 ultra minimal edit
// purpose:
// main.cpp should only do orchestration.
// No logic. No GLFW. No OpenGL. No physics math. No world iteration.
// Just:
// init
    // loop:
    //   poll input
    //   update audio
    //   update physics
    //   render
// shutdown
// Everything else lives behind headers.
// 6 4 2026
/**
 * like this is my fav idea
 * main.cpp calls renderWorld
 * renderWorld calls renderTextures renderLight renderBlackHoleVisuals etc
 * and THOSE files call like drawLine, visualRelativity, drawLightSpeed etc
 * AND THOOOOOSE files call like super basic boring stuff that is fine to call 1 bilion times per frame
 * and if anything fails, it prints the exact file and place and line etc 
 * with the debug log function BC WE NOT USING PRINTF DONT USE PRINTF JUTS MAKE UR OWN DEBUG LOG ok 
 */

#include <cstdio>
#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "camera.h"
#include "input/input-state.h"
#include "input/input-poll.h"
#include "input/input-frame.h"
#include "input/input-commands.h"
#include "render/render-world.h"
#include "render/render-player.h"
#include "physics/physics-mini.h"
#include "physics/physics-debug-movement.h"
#include "audio/audio.h"
#include "gui/gui-main.h"
#include "gui/ui-system.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "network/net_mode.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include "devtools/dev-menu.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/dev-teleport.h"
#include "devtools/dev-commands.h"
#include "devtools/terminal.h"
#include "devtools/account-config.h"
#include "devtools/npc-spawn-commands.h"
#include "ui/hitmarker.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "sim/sim-context.h"
#include "combat/weapon-hit.h"
#include "combat/weapon-manager.h"
#include "combat/death-system.h"
#include "config/player-settings.h"
#include "render/outfit-atlas.h"
#include "render/lighting-config.h"

// todo sort 6 7 2026 alphabetical
#include "game/duel.h"

static bool rayTriangle(glm::vec3 origin, glm::vec3 direction,
                        const CollisionTriangle& tri, float& distance)
{
    const glm::vec3 e1 = tri.b - tri.a;
    const glm::vec3 e2 = tri.c - tri.a;
    const glm::vec3 p = glm::cross(direction, e2);
    const float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.000001f) return false;
    const float invDet = 1.0f / det;
    const glm::vec3 t = origin - tri.a;
    const float u = glm::dot(t, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    const glm::vec3 q = glm::cross(t, e1);
    const float v = glm::dot(direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    distance = glm::dot(e2, q) * invDet;
    return distance > 0.0f;
}

static glm::vec3 castWorldRay(const World& world, glm::vec3 origin, glm::vec3 direction)
{
    direction = glm::normalize(direction);
    float nearest = 200.0f;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(origin, direction, tri, distance) && distance < nearest)
            nearest = distance;
    }
    return origin + direction * nearest;
}


static int selectWorldTriangle(const World& world, glm::vec3 origin, glm::vec3 direction)
{
    direction = glm::normalize(direction);
    float nearest = std::numeric_limits<float>::max();
    int selected = -1;
    for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i) {
        float distance = 0.0f;
        if (rayTriangle(origin, direction, world.collisionMesh.triangles[i], distance) && distance < nearest) {
            nearest = distance;
            selected = i;
        }
    }
    return selected;
}

int main(int argc, char** argv)
{
    MimitaNet::LaunchOptions launchOptions = MimitaNet::parseLaunchOptions(argc, argv);
    if (launchOptions.server && launchOptions.client)
    {
        printf("[MAIN] choose only one mode: --server or --client\n");
        MimitaNet::printLaunchUsage();
        return 1;
    }
    if (launchOptions.server)
        return MimitaNet::runServer(launchOptions);
    if (launchOptions.client)
        return MimitaNet::runClient(launchOptions);

    printf("[MAIN] start\n");

    Engine engine;
    printf("[MAIN] before engine.init\n");
    engine.init(800, 600, "mimita.exe");
    printf("[MAIN] after engine.init\n");

    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Terminal character input callback
    glfwSetCharCallback(engine.window(), [](GLFWwindow*, unsigned int codepoint) {
        Terminal::instance().handleChar(codepoint);
    });
    // Terminal key input callback
    glfwSetKeyCallback(engine.window(), [](GLFWwindow*, int key, int scancode, int action, int mods) {
        (void)scancode;
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            Terminal::instance().handleKey(key, mods);
        }
    });
    glfwSetScrollCallback(engine.window(), [](GLFWwindow*, double, double yOffset) {
        Terminal::instance().handleScroll(yOffset);
    });

    printf("[MAIN] after glfwSetInputMode\n");

    fontInit();   // load .fnt + png
    printf("[MAIN] after fontInit()\n");
    uiInit(engine.window());
    printf("[MAIN] after uiInit()\n");
    DebugVis::init(engine.window());
    Debug::startupReport();
    printf("[MAIN] after DebugVis::init()\n");

    // Dev tools init
    DevConfig::instance().load("config/dev_controls.txt");
    DevOverlay::instance().init(engine.window());
    DevOverlay::instance().showNotification("Dev mode enabled. Press ` to open console.", 5.0f);
    CreateDefaultAccountConfig();
    LoadAccountConfig("default");
    InputCommandSystem::instance().init(engine.window());
    InputCommandSystem::instance().loadBinds("config/accounts/default.json");
    RegisterTeleportCommands();
    Terminal::instance().init(engine.window());
    
    // Effect part system init
    EffectPartSystem::instance().init();
    printf("[MAIN] dev tools initialized\n");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // addd this 6 4 2026     
    glEnable(GL_DEPTH_TEST);

    printf("[MAIN] after glEnable and glBlendFunc()\n");


    // dont include these, fontInit() alread does tem? idk mar 13 2026
    // loadFontAtlas("assets/font/mingliu-mimita-v3_0.png");
    // printf("[MAIN] after loadFontAtlas()\n");

    // loadFontGlyphs("assets/font/mingliu-mimita-v3.fnt");
    // printf("[MAIN] after loadFontGlyphs()\n");

    World world;
    bool worldLoaded = false;
    printf("[MAIN] world object made; world JSON loads when PLAY is pressed so the menu appears first\n");

    Player player;
    player.equippedSlot = GetPlayerSettings().equippedSlot;
    OutfitAtlas::instance().apply(player, GetPlayerSettings().outfitPath);
    printf("[MAIN] player made\n");

    NpcSystem npcSystem;
    bool npcsSpawned = false;
    printf("[MAIN] npc system made\n");

    Camera camera;
    printf("[MAIN] camera made\n");

    engine.bindCamera(&camera);
    // onl do this 1 time, not per frame
    // not in the while X loop
    glfwSetWindowUserPointer(engine.window(), &camera);
    printf("[MAIN] camera bound\n");

    // Global replay recorder/player
    static ReplayRecorder gReplayRecorder;
    static ReplayPlayer gReplayPlayer;

    // also duels
    // todo later, make just like a game mode manager, and make configs
    // using the settings in the game mode manager
    // like number of plrs/npcs, duel time, how much HP, gravity, walkspeed, etc
    static DuelManager gDuelManager;
    static DuelConfig gDuelConfig;

    GameState gameState = GAME_MENU;
    GameState prevState = GAME_MENU;
    bool editorMode = false;
    std::string activeGameMode = "sandbox";
    int selectedEditorObject = -1;
    WeaponManager weapons;
    bool freecamEnabled = false;

    // Gameplay terminal commands
    auto registerActionCommand = [](const char* name, const char* description) {
        Terminal::instance().registerCommand({
            name, description, name,
            [name](const std::vector<std::string>&) {
                InputCommandSystem::instance().pulseAction(name);
                if (DebugConfig::DEBUG_COMMANDS)
                    Debug::log(Debug::Category::General, "[COMMAND] %s\n", name);
                Terminal::instance().addLog(std::string("[GAMEPLAY] ") + name);
            }
        });
    };
    registerActionCommand("walkforward", "Move forward for one simulation tick");
    registerActionCommand("walkback", "Move backward for one simulation tick");
    registerActionCommand("walkleft", "Move left for one simulation tick");
    registerActionCommand("walkright", "Move right for one simulation tick");
    registerActionCommand("jump", "Execute a jump action");
    registerActionCommand("dash", "Execute a dash action");

    Terminal::instance().registerCommand({
        "freeze", "Toggle freeze", "freeze",
        [](const std::vector<std::string>&) {
            gTerminalInputOverride.freezeHeld = !gTerminalInputOverride.freezeHeld;
            Terminal::instance().addLog(
                gTerminalInputOverride.freezeHeld ? "[GAMEPLAY] freeze ON" : "[GAMEPLAY] freeze OFF");
        }
    });

    Terminal::instance().registerCommand({
        "ground_return", "Execute a ground return", "ground_return",
        [](const std::vector<std::string>&) {
            gTerminalInputOverride.groundReturnPressed = true;
            Terminal::instance().addLog("[GAMEPLAY] ground_return");
        }
    });

    Terminal::instance().registerCommand({
        "shoot", "Fire weapon", "shoot",
        [&camera, &player, &npcSystem, &world, &weapons](const std::vector<std::string>&) {
            RevolverShotResult shot = weapons.fire(camera, player, npcSystem, world);
            if (!shot.fired) {
                Terminal::instance().addLog("[WEAPON] dry fire or no active weapon");
                return;
            }
            Terminal::instance().addLog("[REVOLVER] fired");
        }
    });

    Terminal::instance().registerCommand({
        "reload", "Reload weapon", "reload",
        [&player, &weapons](const std::vector<std::string>&) {
            bool loaded = weapons.reload(player);
            if (DebugConfig::DEBUG_INPUT)
                Debug::log(Debug::Category::General, "[INPUT] action=reload command=reload weapon=%s\n",
                           loaded ? "executed" : "ignored");
            Terminal::instance().addLog(loaded ? "[WEAPON] reload complete" : "[WEAPON] reload unavailable");
        }
    });

    Terminal::instance().registerCommand({
        "openinventory", "Toggle inventory", "openinventory",
        [&player](const std::vector<std::string>&) {
            player.inventoryOpen = !player.inventoryOpen;
            Terminal::instance().addLog(player.inventoryOpen ? "[INVENTORY] opened" : "[INVENTORY] closed");
        }
    });

    for (int keySlot = 0; keySlot <= 9; ++keySlot) {
        int slot = keySlot == 0 ? 10 : keySlot;
        std::string name = "equipslot" + std::to_string(keySlot);
        Terminal::instance().registerCommand({
            name, "Equip inventory slot " + std::to_string(slot), name,
            [&player, &weapons, slot](const std::vector<std::string>&) {
                weapons.equip(player, slot);
                GetPlayerSettings().equippedSlot = slot;
                SavePlayerSettings();
                Terminal::instance().addLog("[INVENTORY] equipped slot " + std::to_string(slot));
            }
        });
    }

    Terminal::instance().registerCommand({
        "setoutfit", "Set and save the player outfit PNG", "setoutfit <path>",
        [&player](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: setoutfit <path>");
                return;
            }
            if (OutfitAtlas::instance().apply(player, args[0])) {
                GetPlayerSettings().outfitPath = args[0];
                SavePlayerSettings();
            }
        }
    });
    Terminal::instance().registerCommand({
        "reloadoutfit", "Reload the current outfit PNG from disk", "reloadoutfit",
        [&player](const std::vector<std::string>&) {
            OutfitAtlas::instance().apply(player, GetPlayerSettings().outfitPath, true);
        }
    });
    Terminal::instance().registerCommand({
        "outfitdebug", "Print outfit atlas region mapping", "outfitdebug",
        [](const std::vector<std::string>&) { OutfitAtlas::instance().printDebug(); }
    });
    Terminal::instance().registerCommand({
        "killfeed", "Show recent kills", "killfeed",
        [&weapons](const std::vector<std::string>&) {
            if (weapons.killfeed().empty()) {
                Terminal::instance().addLog("[KILLFEED] no kills");
                return;
            }
            for (const std::string& line : weapons.killfeed())
                Terminal::instance().addLog("[KILLFEED] " + line);
        }
    });
    Terminal::instance().registerCommand({
        "debug_combat", "Enable combat calculation logging", "debug_combat <true|false>",
        [](const std::vector<std::string>& args) {
            bool& enabled = GetPlayerSettings().debugCombat;
            enabled = args.empty() ? !enabled : (args[0] == "true" || args[0] == "1");
            SavePlayerSettings();
            Terminal::instance().addLog(std::string("[DEBUG] debug_combat=") + (enabled ? "true" : "false"));
        }
    });
    Terminal::instance().registerCommand({
        "sound_debug", "Toggle centralized sound logs", "sound_debug <0|1>",
        [](const std::vector<std::string>& args) {
            bool enabled = args.empty() ? !AudioManager::instance().debug() : args[0] != "0";
            AudioManager::instance().setDebug(enabled);
            Terminal::instance().addLog(std::string("[SOUND] debug ") + (enabled ? "enabled" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "dbgvis", "Master toggle for all debug visuals", "dbgvis <0|1>",
        [](const std::vector<std::string>& args) {
            bool enabled = args.empty() ? !DebugVis::masterEnabled() : args[0] != "0";
            DebugVis::setMasterEnabled(enabled);
            Terminal::instance().addLog(std::string("[DEBUG VISUALS] ") + (enabled ? "enabled" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "freecam", "Detach or attach the gameplay camera", "freecam <0|1>",
        [&freecamEnabled, &camera, &player](const std::vector<std::string>& args) {
            freecamEnabled = args.empty() ? !freecamEnabled : args[0] != "0";
            if (freecamEnabled)
                camera.pos = player.pos + glm::vec3(0, 0, 2.0f);
            Terminal::instance().addLog(std::string("[FREECAM] ") + (freecamEnabled ? "enabled" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "freecam_speed", "Set freecam speed in meters per second", "freecam_speed <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: freecam_speed <number>");
                return;
            }
            GetPlayerSettings().freecamSpeed = std::clamp(std::stof(args[0]), 0.1f, 500.0f);
            SavePlayerSettings();
            Terminal::instance().addLog("[FREECAM] speed=" + std::to_string(GetPlayerSettings().freecamSpeed));
        }
    });
    Terminal::instance().registerCommand({
        "weapon_inspect", "Print active weapon module state", "weapon_inspect",
        [&weapons](const std::vector<std::string>&) { weapons.inspect(); }
    });
    Terminal::instance().registerCommand({
        "npc_spawn", "Spawn NPCs in front of the camera", "npc_spawn <count>",
        [&npcSystem, &camera, &player](const std::vector<std::string>& args) {
            int count = args.empty() ? 1 : std::clamp(std::stoi(args[0]), 1, 100);
            for (int i = 0; i < count; ++i)
                npcSystem.spawnNpc(1.0f, camera.pos + camera.front * (5.0f + i * 1.5f) + glm::vec3(0,0,1));
            Terminal::instance().addLog("[NPC COMMAND] npc_spawn count=" + std::to_string(count));
        }
    });
    Terminal::instance().registerCommand({
        "npc_select_all", "Select every NPC", "npc_select_all",
        [&npcSystem](const std::vector<std::string>&) {
            NpcSelectionManager::instance().selectAll(npcSystem);
            Terminal::instance().addLog("[NPC COMMAND] npc_select_all");
        }
    });
    Terminal::instance().registerCommand({
        "npc_delete_selected", "Delete selected NPCs", "npc_delete_selected",
        [&npcSystem](const std::vector<std::string>&) {
            std::vector<std::uint32_t> ids(
                NpcSelectionManager::instance().selectedIds().begin(),
                NpcSelectionManager::instance().selectedIds().end());
            npcSystem.destroySelected(ids);
            Terminal::instance().addLog("[NPC COMMAND] npc_delete_selected");
        }
    });
    Terminal::instance().registerCommand({
        "npc_delete_all", "Delete every NPC", "npc_delete_all",
        [&npcSystem](const std::vector<std::string>&) {
            npcSystem.destroyAll();
            Terminal::instance().addLog("[NPC COMMAND] npc_delete_all");
        }
    });

    Terminal::instance().registerCommand({
        "serverconnect", "Print a server connection request", "serverconnect <ip> [args...]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: serverconnect <ip> [args...]");
                return;
            }
            std::string text = "[SERVER] would connect to " + args[0];
            for (size_t i = 1; i < args.size(); ++i) text += " " + args[i];
            Terminal::instance().addLog(text);
        }
    });
    Terminal::instance().registerCommand({
        "disconnectserver", "Print a server disconnect request", "disconnectserver",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[SERVER] would disconnect");
        }
    });
    Terminal::instance().registerCommand({
        "editormode", "Enter map editor mode", "editormode",
        [&editorMode, &gameState](const std::vector<std::string>&) {
            editorMode = true;
            gameState = GAME_PLAYING;
            Terminal::instance().addLog("[EDITOR] editor mode enabled");
        }
    });
    Terminal::instance().registerCommand({
        "gamemode", "Return to play mode or select sandbox/tdm", "gamemode [sandbox|tdm]",
        [&editorMode, &activeGameMode](const std::vector<std::string>& args) {
            editorMode = false;
            if (!args.empty()) {
                if (args[0] != "sandbox" && args[0] != "tdm") {
                    Terminal::instance().addLog("[ERROR] gamemode must be sandbox or tdm");
                    return;
                }
                activeGameMode = args[0];
            }
            Terminal::instance().addLog("[GAME MODE] " + activeGameMode);
        }
    });
    Terminal::instance().registerCommand({
        "selectobject", "Select an editor block/triangle id", "selectobject <id>",
        [&selectedEditorObject, &world](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: selectobject <id>");
                return;
            }
            selectedEditorObject = std::stoi(args[0]);
            size_t count = world.blocks.empty() ? world.collisionMesh.triangles.size() : world.blocks.size();
            if (selectedEditorObject < 0 || selectedEditorObject >= (int)count) {
                selectedEditorObject = -1;
                Terminal::instance().addLog("[ERROR] object id out of range");
                return;
            }
            Terminal::instance().addLog("[EDITOR] selected object id " + std::to_string(selectedEditorObject));
        }
    });
    Terminal::instance().registerCommand({
        "assignmaterial", "Assign a material name to a selected block", "assignmaterial <name>",
        [&selectedEditorObject, &world](const std::vector<std::string>& args) {
            if (selectedEditorObject < 0 || args.empty()) {
                Terminal::instance().addLog("[ERROR] select a block and provide a material name");
                return;
            }
            if (selectedEditorObject >= (int)world.blocks.size()) {
                Terminal::instance().addLog("[EDITOR] GLB triangle material assignment is a placeholder");
                return;
            }
            world.blocks[selectedEditorObject].texName = args[0];
            Terminal::instance().addLog("[EDITOR] material assigned: " + args[0]);
        }
    });
    Terminal::instance().registerCommand({
        "savemap", "Placeholder editor save", "savemap [name]",
        [](const std::vector<std::string>& args) {
            Terminal::instance().addLog("[EDITOR] would save map " + (args.empty() ? std::string("untitled") : args[0]));
        }
    });

    auto registerDebugToggle = [](const char* name, bool& flag) {
        Terminal::instance().registerCommand({
            name, std::string("Toggle ") + name, std::string(name) + " [0|1]",
            [&flag, name](const std::vector<std::string>& args) {
                flag = args.empty() ? !flag : args[0] != "0";
                Terminal::instance().addLog(std::string("[DEBUG] ") + name + "=" + (flag ? "1" : "0"));
            }
        });
    };
    registerDebugToggle("debug_ticks", DebugConfig::DEBUG_TICKS);
    registerDebugToggle("debug_input", DebugConfig::DEBUG_INPUT);
    registerDebugToggle("debug_collision", DebugConfig::COLLISION_VERBOSE);
    registerDebugToggle("debug_npc", DebugConfig::DEBUG_NPC);
    registerDebugToggle("debug_commands", DebugConfig::DEBUG_COMMANDS);

    // Replay terminal commands
    Terminal::instance().registerCommand({
        "replay.record", "Start replay recording", "replay.record",
        [&world](const std::vector<std::string>&) {
            if (gReplayRecorder.isRecording()) {
                Terminal::instance().addLog("[REPLAY] Already recording");
                return;
            }
            gReplayRecorder.beginRecording(0.0f, "mimita");

            const std::string mapPath = "assets/maps/mimita-aabb-only-interior-small-v4.glb";
            const std::string playerPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
            const std::string revolverPath = "assets/objects/weapons/mimita-revolver-v1.glb";
            gReplayRecorder.registerAsset("map:mimita", "map_glb", mapPath, {}, "basic", "world");
            gReplayRecorder.registerAsset("model:player", "actor_glb", playerPath, {}, "basic", "player");
            gReplayRecorder.registerAsset("model:revolver", "weapon_glb", revolverPath, {}, "basic", "weapon");
            gReplayRecorder.registerAsset("texture:outfit", "texture", GetPlayerSettings().outfitPath, {}, {}, "outfit");
            gReplayRecorder.registerAsset("texture:crosshair-ready", "texture", "assets/crosshair/crosshairready.png", {}, {}, "ui");
            gReplayRecorder.registerAsset("texture:crosshair-delay", "texture", "assets/crosshair/crosshairdelay.png", {}, {}, "ui");
            gReplayRecorder.registerAsset("texture:crosshair-reloading", "texture", "assets/crosshair/crosshairreloading.png", {}, {}, "ui");

            ReplayWorldMetadata replayWorld;
            replayWorld.mapAssetId = "map:mimita";
            replayWorld.mapPath = mapPath;
            for (const Mesh::Batch& batch : world.mesh.batches) {
                const std::string materialName = batch.materialName.empty() ? "default" : batch.materialName;
                bool alreadyRegistered = false;
                for (const ReplayMaterialReference& material : replayWorld.materials) {
                    if (material.materialName == materialName) {
                        alreadyRegistered = true;
                        break;
                    }
                }
                if (!alreadyRegistered)
                    replayWorld.materials.push_back({materialName, "", "basic"});
            }
            gReplayRecorder.setWorldMetadata(replayWorld);

            ReplayLightingState replayLighting;
            replayLighting.directionalLight = gLighting.lightDir;
            replayLighting.ambientStrength = gLighting.ambientStrength;
            replayLighting.diffuseStrength = gLighting.diffuseStrength;
            replayLighting.edgeDarkness = gLighting.edgeDarkness;
            replayLighting.edgeWidth = gLighting.edgeWidth;
            replayLighting.aoDarkness = gLighting.aoDarkness;
            replayLighting.aoContrast = gLighting.aoContrast;
            replayLighting.textureContrast = gLighting.textureContrast;
            replayLighting.textureBrightness = gLighting.textureBrightness;
            gReplayRecorder.setLighting(replayLighting);

            Terminal::instance().addLog("[REPLAY] Recording started");
        }
    });

    Terminal::instance().registerCommand({
        "replay.stop", "Stop replay recording or playback", "replay.stop",
        [](const std::vector<std::string>&) {
            if (gReplayRecorder.isRecording()) {
                gReplayRecorder.stopRecording();
                const std::string path = generateReplayExportPath();
                const bool exported = gReplayRecorder.exportToJSON(path);
                Terminal::instance().addLog(
                    exported
                        ? "[REPLAY] Recording stopped and saved to " + path
                        : "[ERROR] Replay stopped but export failed: " + path
                );
            }
            if (gReplayPlayer.isPlaying()) {
                gReplayPlayer.stopPlayback();
                Terminal::instance().addLog("[REPLAY] Playback stopped");
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay.export", "Export replay to file", "replay.export <path>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: replay.export <path>");
                return;
            }
            std::string path = args[0];
            if (path.find('.') == std::string::npos)
                path += ".json";
            const bool exported = gReplayRecorder.exportToJSON(path);
            Terminal::instance().addLog(
                exported ? "[REPLAY] Exported to " + path
                         : "[ERROR] Failed to export replay to " + path
            );
        }
    });

    Terminal::instance().registerCommand({
        "replay.load", "Load replay file", "replay.load <path>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: replay.load <path>");
                return;
            }
            std::string path = args[0];
            bool ok = gReplayPlayer.loadFromJSON(path);
            Terminal::instance().addLog(ok ? "[REPLAY] Loaded " + path : "[ERROR] Failed to load " + path);
        }
    });

    Terminal::instance().registerCommand({
        "replay.play", "Start replay playback", "replay.play",
        [](const std::vector<std::string>&) {
            if (gReplayPlayer.totalTicks() == 0) {
                Terminal::instance().addLog("[ERROR] No replay loaded");
                return;
            }
            gReplayPlayer.beginPlayback();
            Terminal::instance().addLog("[REPLAY] Playback started");
        }
    });

    Terminal::instance().registerCommand({
        "replay.info", "Show replay info", "replay.info",
        [](const std::vector<std::string>&) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Recording: %d  Playback: %d  Ticks: %u",
                     (int)gReplayRecorder.isRecording(), (int)gReplayPlayer.isPlaying(),
                     gReplayPlayer.totalTicks());
            Terminal::instance().addLog(buf);
        }
    });

    // 6 7 2026 omg todo
    // put ALL these commands into terminal folder like by itself
    Terminal::instance().registerCommand({
        "duel.start",
        "Start duel mode",
        "duel.start [npcCount]",
        [&player, &npcSystem](const std::vector<std::string>& args)
        {
            DuelConfig cfg;

            cfg.numNpcs =
                args.empty()
                ? 3
                : std::clamp(std::stoi(args[0]), 1, 10);

            cfg.killsToWin = 10;
            cfg.duelLengthSeconds = 300;
            cfg.enabled = true;

            gDuelConfig = cfg;

            gDuelManager.start(
                gDuelConfig,
                player,
                npcSystem);

            Terminal::instance().addLog(
                "[DUEL] started");
        }
    });

    // SimContext setup: bundle sim state for replay/deterministic ticks
    SimContext simContext;
    simContext.player = &player;
    simContext.world = &world;
    simContext.npcSystem = &npcSystem;
    simContext.randomSeed = 0.0f;

    constexpr double SIM_DT = 1.0 / 60.0;
    double simAccumulator = 0.0;

        while (engine.running())
    {
        float dt = engine.beginFrame();
        bool worldPassRan = false;

        audioUpdate(dt);
        DebugVis::update();
        uiSetDebug(DebugVis::ui());

        if (gameState != prevState)
        {
            printf("[MAIN] gameState changed %d -> %d\n", (int)prevState, (int)gameState);
            if (gameState == GAME_PLAYING)
            {
                if (!worldLoaded)
                {
                    printf("[MAIN] PLAY requested; loading existing world now\n");
                    // loadWorldFromJSON(
                    //     world,
                    //     "assets/maps/json-converts/mimita-aabb-only-interior-small-v2-converted-v2.json"
                    // );
                    // 5 23 2026 using gltf glb stuff for better strucutres mhm 
                    loadWorldFromGLB(
                        world,
                        "assets/maps/mimita-aabb-only-interior-small-v4.glb"
                    );
                    worldLoaded = true;
                    printf("[MAIN] world load complete blocks=%zu spheres=%zu\n", world.blocks.size(), world.spheres.size());
                }
                if (!npcsSpawned)
                {
                    npcSystem.spawnPrototypeScene();
                    npcsSpawned = true;
                    printf("[MAIN] NPC prototype scene spawned count=%zu\n", npcSystem.all().size());
                }
                glfwSetInputMode(engine.window(), GLFW_CURSOR,
                    Terminal::instance().isOpen() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            }
            else
            {
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            prevState = gameState;
        }

        // Dev tools update
        DevOverlay::instance().update(dt);
        NpcSelectionManager::instance().update();

        // Terminal toggle on grave accent (`/~)
        static bool gravePrev = false;
        bool graveDown = glfwGetKey(engine.window(), GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
        if (graveDown && !gravePrev) {
            Terminal::instance().toggle();
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                Terminal::instance().isOpen() ? GLFW_CURSOR_NORMAL :
                (gameState == GAME_PLAYING ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL));
        }
        gravePrev = graveDown;

        if (gameState == GAME_PLAYING)
        {
            DebugVis::beginCollisionFrame();

            // Fixed-tick simulation accumulator
            // Accumulate real dt and step simulation at SIM_DT rate
            simAccumulator += (double)dt;

            while (simAccumulator >= SIM_DT) {
                InputFrame tickFrame;

                if (gReplayPlayer.isPlaying()) {
                    // Playback: advance from recorded replay
                    const InputFrame* recordedFrame = gReplayPlayer.advanceTick();
                    if (recordedFrame) {
                        tickFrame = *recordedFrame;
                    } else {
                        // Replay finished
                        gReplayPlayer.stopPlayback();
                    }
                }

                if (!gReplayPlayer.isPlaying()) {
                    // Live input: build InputFrame from keyboard + terminal override
                    InputCommandSystem::instance().setKeyboardEnabled(!Terminal::instance().isOpen());
                    // tickFrame = buildInputFrame(engine.window(), camera);

                    // lock mvoemnet if countdown in duels 6 7 2026 
                    tickFrame = buildInputFrame(engine.window(), camera);

                    if (gDuelManager.phase() == DuelPhase::Countdown)
                    {
                        tickFrame.moveX = 0.0f;
                        tickFrame.moveY = 0.0f;

                        tickFrame.jump = false;
                        tickFrame.jumpPressed = false;

                        tickFrame.dashPressed = false;
                        tickFrame.freezeHeld = false;

                        tickFrame.reloadPressed = false;
                    }
                    if (tickFrame.reloadPressed) {
                        if (DebugConfig::DEBUG_INPUT)
                            Debug::log(Debug::Category::General, "[INPUT] key -> action=reload -> command=reload\n");
                        Terminal::instance().execute("reload");
                    }
                }

                const bool recordingReplayTick = gReplayRecorder.isRecording();
                uint32_t replayTick = 0;
                if (recordingReplayTick) {
                    replayTick = gReplayRecorder.currentTick();
                    gReplayRecorder.recordFrame(tickFrame);
                }

                // Run simulation for this tick
                if (!freecamEnabled)
                    simulateTick(simContext, tickFrame);

                if (recordingReplayTick) {
                    ReplaySceneFrame sceneFrame;
                    sceneFrame.tick = (int)replayTick;
                    sceneFrame.time = (float)sceneFrame.tick / 60.0f;

                    // Camera
                    sceneFrame.camera.position = camera.pos;
                    sceneFrame.camera.rotation = glm::vec3(camera.pitch, 0.0f, camera.yaw);
                    sceneFrame.camera.fov = 70.0f;

                    // Player
                    ReplayActorState playerActor;
                    playerActor.id = player.username.empty() ? "admin" : player.username;
                    playerActor.name = player.username;
                    playerActor.type = "player";
                    playerActor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
                    playerActor.position = player.pos;
                    playerActor.rotation = glm::vec3(0.0f, 0.0f, player.yaw);
                    playerActor.velocity = player.vel;
                    playerActor.health = player.currentHp;
                    playerActor.maxHealth = player.maxHp;
                    playerActor.grounded = player.onGround;
                    playerActor.collidable = !player.dead;
                    playerActor.fade = 0.0f;
                    playerActor.weaponName = player.equippedSlot == 1 ? "revolver" : "none";
                    playerActor.weaponModelPath = player.equippedSlot == 1
                        ? "assets/objects/weapons/mimita-revolver-v1.glb"
                        : "";
                    playerActor.reloading = weapons.isReloading();
                    playerActor.shooting = weapons.isShooting();
                    playerActor.animationState = player.onGround
                        ? (glm::length(glm::vec2(player.vel.x, player.vel.y)) > 0.5f ? "move" : "idle")
                        : "air";
                    playerActor.bodyParts = captureReplayBodyParts(player);
                    sceneFrame.actors.push_back(playerActor);

                    // NPCs
                    for (const Npc& npc : npcSystem.all()) {
                        ReplayActorState npcActor;
                        npcActor.id = "npc_" + std::to_string(npc.id);
                        npcActor.name = npc.body.username;
                        npcActor.type = npc.body.dead ? "corpse" : "npc";
                        npcActor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
                        npcActor.position = npc.body.pos;
                        npcActor.rotation = glm::vec3(0.0f, 0.0f, npc.body.yaw);
                        npcActor.velocity = npc.body.vel;
                        npcActor.health = npc.body.currentHp;
                        npcActor.maxHealth = npc.body.maxHp;
                        npcActor.grounded = npc.body.onGround;
                        npcActor.collidable = !npc.body.dead;
                        npcActor.fade = 0.0f;
                        npcActor.weaponName = "none";
                        npcActor.animationState = npc.chosenAction.name;
                        npcActor.bodyParts = captureReplayBodyParts(npc.body);
                        sceneFrame.actors.push_back(npcActor);
                    }
                    DeathSystem::instance().appendReplayActors(sceneFrame.actors);

                    gReplayRecorder.recordSceneFrame(sceneFrame);
                }

                simAccumulator -= SIM_DT;
            }

            // Process NPC spawn commands (from console or F2) — after sim tick
            ProcessNpcSpawnCommands(npcSystem, camera, world, player);
            if (!Terminal::instance().isOpen())
                HandleF2SpawnNpc(npcSystem, camera, world, player, engine.window());

            if (!Terminal::instance().isOpen())
                applyDebugMovement(player, engine.window(), camera, dt);

            camera.updateVectors();
            if (freecamEnabled && !Terminal::instance().isOpen()) {
                glm::vec3 flatForward = camera.front;
                flatForward.z = 0.0f;
                if (glm::length(flatForward) > 0.001f) flatForward = glm::normalize(flatForward);
                glm::vec3 flatRight = glm::normalize(glm::cross(flatForward, glm::vec3(0,0,1)));
                glm::vec3 move(0.0f);
                if (glfwGetKey(engine.window(), GLFW_KEY_W) == GLFW_PRESS) move += flatForward;
                if (glfwGetKey(engine.window(), GLFW_KEY_S) == GLFW_PRESS) move -= flatForward;
                if (glfwGetKey(engine.window(), GLFW_KEY_D) == GLFW_PRESS) move += flatRight;
                if (glfwGetKey(engine.window(), GLFW_KEY_A) == GLFW_PRESS) move -= flatRight;
                if (glfwGetKey(engine.window(), GLFW_KEY_E) == GLFW_PRESS) move.z += 1.0f;
                if (glfwGetKey(engine.window(), GLFW_KEY_Q) == GLFW_PRESS) move.z -= 1.0f;
                if (glm::length(move) > 0.001f)
                    camera.pos += glm::normalize(move) * GetPlayerSettings().freecamSpeed * dt;
            } else {
                camera.follow(player.pos);
            }
            setAudioListener(camera.pos, camera.front);
            weapons.update(camera, player, dt);

            gDuelManager.update(
                dt,
                player,
                npcSystem,
                world,
                camera);

            player.updateAudio(dt);

            // Update effect parts
            EffectPartSystem::instance().update(dt);

            drawHitmarker(dt);

            static bool mousePrev = false;
            bool mouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (!Terminal::instance().isOpen() && mouseDown && !mousePrev) {
                if (editorMode) {
                    selectedEditorObject = selectWorldTriangle(world, camera.pos, camera.front);
                    Terminal::instance().addLog(selectedEditorObject >= 0
                        ? "[EDITOR] selected triangle id " + std::to_string(selectedEditorObject)
                        : "[EDITOR] no object selected");
                } else {
                    Terminal::instance().execute("shoot");
                }
            }
            mousePrev = mouseDown;

            static bool slotPrev[10] = {};
            for (int keySlot = 0; keySlot <= 9; ++keySlot) {
                int key = keySlot == 0 ? GLFW_KEY_0 : GLFW_KEY_0 + keySlot;
                bool down = glfwGetKey(engine.window(), key) == GLFW_PRESS;
                if (!Terminal::instance().isOpen() && down && !slotPrev[keySlot])
                    Terminal::instance().execute("equipslot" + std::to_string(keySlot));
                slotPrev[keySlot] = down;
            }
            renderWorld(world, camera);
            renderPlayer(player, camera);
            npcSystem.render(camera);
            DeathSystem::instance().render(camera);
            if (player.equippedSlot == 1)
                weapons.render(camera, player);
            
            // Render effect parts (world-space visualizations)
            EffectPartSystem::instance().render(camera);
            
            worldPassRan = true;

            npcSystem.drawDebug(camera);
            drawDebugStuff(player, camera, world);

            // Draw NPC selection debug visuals
            if (DebugVis::enabled()) {
                NpcSelectionManager::instance().drawSelection(npcSystem, camera);
            }

            uiBeginFrame(engine.window(), "game-debug-overlay");
            if (gReplayRecorder.isRecording()) {
                const float overlayX = uiScreenW() - 230.0f;
                uiDrawRect({overlayX - 18.0f, 20.0f, 12.0f, 12.0f},
                           {1.0f, 0.05f, 0.05f, 1.0f}, "replay-record-dot");
                uiDrawText("[REPLAY REC]", overlayX, 30.0f, 0.34f,
                           {1.0f, 0.12f, 0.12f, 1.0f});
                char replayTickText[64];
                snprintf(replayTickText, sizeof(replayTickText), "tick: %u",
                         gReplayRecorder.currentTick());
                uiDrawText(replayTickText, overlayX, 58.0f, 0.30f,
                           {1.0f, 0.12f, 0.12f, 1.0f});
            }
            if (player.equippedSlot == 1) {
                const char* crosshairPath = "assets/crosshair/crosshairready.png";
                switch (weapons.crosshairState(player)) {
                    case WeaponCrosshairState::Reloading:
                        crosshairPath = "assets/crosshair/crosshairreloading.png";
                        break;
                    case WeaponCrosshairState::Delay:
                        crosshairPath = "assets/crosshair/crosshairdelay.png";
                        break;
                    case WeaponCrosshairState::Ready:
                        break;
                }
                uiDrawImage(crosshairPath,
                            {uiScreenW() * 0.5f - 50.0f, uiScreenH() * 0.5f - 50.0f, 100.0f, 100.0f});
            }
            uiDrawRect({14, 78, 260, 92}, {0.0f, 0.0f, 0.0f, 0.56f}, "hud-bg");
            uiDrawText(player.username.c_str(), 24, 88, 0.42f, {0.95f, 0.98f, 1.0f, 1.0f});
            char hpText[64];
            snprintf(hpText, sizeof(hpText), "HP: %d/%d", player.currentHp, player.maxHp);
            uiDrawText(hpText, 24, 116, 0.38f, {0.35f, 1.0f, 0.45f, 1.0f});
            if (player.dead) {
                const float centerX = uiScreenW() * 0.5f;
                const float centerY = uiScreenH() * 0.5f;
                std::string deathText = "you died to " +
                    (player.killedBy.empty() ? std::string("unknown") : player.killedBy);
                char respawnText[128];
                snprintf(respawnText, sizeof(respawnText),
                         "respawning automatically in %.3f...", player.respawnTimer);
                uiDrawRect(
                    {centerX - 270.0f, centerY - 80.0f, 540.0f, 160.0f},
                    {0.0f, 0.0f, 0.0f, 0.75f},
                    "death-overlay");
                uiDrawText(deathText.c_str(), centerX - 150.0f, centerY - 42.0f,
                           0.55f, {1.0f, 0.15f, 0.15f, 1.0f});
                uiDrawText(respawnText, centerX - 205.0f, centerY + 2.0f,
                           0.38f, {1.0f, 1.0f, 1.0f, 1.0f});
                uiDrawText("press space to respawn instantly",
                           centerX - 190.0f, centerY + 42.0f,
                           0.38f, {0.85f, 0.9f, 1.0f, 1.0f});
            }
            {
                glm::vec3 totalVel = player.vel;
                float speed = glm::length(totalVel);
                char speedText[64];
                snprintf(speedText, sizeof(speedText), "Speed: %.2f m/s", speed);
                uiDrawText(speedText, 24, 144, 0.38f, {0.75f, 0.9f, 1.0f, 1.0f});
            }
            {
                char modeText[128];
                snprintf(modeText, sizeof(modeText), "%s | %s | slot %d",
                         editorMode ? "EDITOR" : "PLAYING", activeGameMode.c_str(), player.equippedSlot);
                uiDrawText(modeText, 24, 208, 0.32f, {0.8f, 0.85f, 1.0f, 1.0f});
                if (player.equippedSlot == 1) {
                    char ammoText[96];
                    snprintf(ammoText, sizeof(ammoText), "Revolver: %d / %d",
                             player.revolverCylinder, player.revolverReserve);
                    uiDrawText(ammoText, 24, 232, 0.42f, {1.0f, 0.82f, 0.3f, 1.0f});
                }
                if (player.inventoryOpen)
                    uiDrawText("INVENTORY: [1] Revolver [2-10] Empty", 24, 260, 0.36f, {0.9f,0.9f,1.0f,1.0f});
            }
            {
                const float normalSize = 44.0f;
                const float gap = 7.0f;
                const float totalWidth = normalSize * 10.0f + gap * 9.0f;
                float x = uiScreenW() * 0.5f - totalWidth * 0.5f;
                float y = uiScreenH() - 70.0f;
                for (int slot = 1; slot <= 10; ++slot) {
                    bool equipped = player.equippedSlot == slot;
                    float size = equipped ? normalSize * 1.2f : normalSize;
                    float offset = (size - normalSize) * 0.5f;
                    UIRect rect{x - offset, y - offset, size, size};
                    uiDrawRect(rect, slot == 1 ? glm::vec4(0.32f,0.32f,0.36f,0.95f)
                                               : glm::vec4(0.12f,0.12f,0.14f,0.92f), "hotbar-slot");
                    uiDrawRectOutline(rect, equipped ? glm::vec4(1,1,1,1)
                                                     : glm::vec4(0.45f,0.45f,0.48f,1), "hotbar-border");
                    std::string label = slot == 10 ? "0" : std::to_string(slot);
                    uiDrawText(label.c_str(), rect.x + 5, rect.y + 16, 0.30f, {1,1,1,1});
                    uiDrawText(slot == 1 ? "REV" : "-", rect.x + 13, rect.y + 34, 0.20f,
                               slot == 1 ? glm::vec4(1,0.85f,0.35f,1) : glm::vec4(0.55f,0.55f,0.58f,1));
                    x += normalSize + gap;
                }
            }
            {
                float nameX = 0.0f, nameY = 0.0f;
                if (DebugVis::projectToScreen(camera, player.pos + glm::vec3(0,0,PLAYER_HEIGHT * 0.7f),
                                              nameX, nameY)) {
                    float ratio = player.maxHp > 0 ? (float)player.currentHp / player.maxHp : 0.0f;
                    uiDrawRect({nameX - 70, nameY - 8, 140, 12}, {0.55f,0.05f,0.05f,0.95f}, "self-hp-bg");
                    uiDrawRect({nameX - 70, nameY - 8, 140 * ratio, 12}, {0.05f,0.8f,0.15f,0.95f}, "self-hp-current");
                    uiDrawText(player.username.c_str(), nameX - 35, nameY - 32, 0.32f, {1,1,1,1});
                    uiDrawText(hpText, nameX - 35, nameY + 8, 0.28f, {1,1,1,1});
                }
            }
            for (const Npc& npc : npcSystem.all()) {
                float x = 0.0f, y = 0.0f;
                glm::vec3 headPos = npc.body.pos + glm::vec3(0,0,PLAYER_HEIGHT * 0.72f);
                float distance = glm::length(camera.pos - headPos);
                if (distance > 80.0f || !DebugVis::projectToScreen(camera, headPos, x, y))
                    continue;
                float fade = std::clamp(1.0f - distance / 90.0f, 0.25f, 1.0f);
                float ratio = npc.body.maxHp > 0 ? (float)npc.body.currentHp / npc.body.maxHp : 0.0f;
                uiDrawRect({x - 65, y - 8, 130, 11}, {0.55f,0.03f,0.03f,0.9f * fade}, "npc-hp-bg");
                uiDrawRect({x - 65, y - 8, 130 * ratio, 11}, {0.05f,0.8f,0.15f,0.9f * fade}, "npc-hp-current");
                uiDrawText(npc.body.username.c_str(), x - 40, y - 31, 0.28f, {1,1,1,fade});
                char npcHp[48];
                snprintf(npcHp, sizeof(npcHp), "%d/%d", npc.body.currentHp, npc.body.maxHp);
                uiDrawText(npcHp, x - 24, y + 8, 0.25f, {1,1,1,fade});
            }
            {
                char npcText[96];
                snprintf(npcText, sizeof(npcText), "NPCs: %zu difficulties 1/3/5/7/10", npcSystem.all().size());
                uiDrawText(npcText, 24, 168, 0.32f, {1.0f, 0.82f, 0.38f, 1.0f});
            }
            if (DebugVis::render())
            {
                char dbg[256];
                snprintf(dbg, sizeof(dbg), "dt %.3f grounded %d vel %.2f %.2f %.2f cam %.1f %.1f %.1f",
                         dt, (int)player.onGround, player.vel.x, player.vel.y, player.vel.z,
                         camera.pos.x, camera.pos.y, camera.pos.z);
                uiDrawText(dbg, 24, 184, 0.30f, {1.0f, 0.9f, 0.45f, 1.0f});
            }
            gDuelManager.renderHud();
            uiRenderFrameDebugOverlay(engine.window(), "PLAYING", worldPassRan);
            uiEndFrame();

            // Dev overlay notifications (temporary)
            DevOverlay::instance().render();
        }

        if (gameState == GAME_MENU)
        {
            guiMain(engine.window(), gameState);
        }

        static bool escapePrev = false;
        bool escapeDown = glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escapeDown && !escapePrev)
        {
            if (Terminal::instance().isOpen()) {
                Terminal::instance().toggle();
                glfwSetInputMode(engine.window(), GLFW_CURSOR,
                    gameState == GAME_PLAYING ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            } else {
                gameState = GAME_MENU;
            }
        }
        escapePrev = escapeDown;

        // Terminal rendering (on top of everything)
        Terminal::instance().render();

        engine.endFrame();

    }

    printf("[MAIN] loop ended\n");
    engine.shutdown();
    
    return 0;
}
