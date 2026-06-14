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

// 6 12 2026 todo plz make main like literal 100 lines
// every file ideally is 100 lines or less, and just exposes 1 function 
//like main just calls functions from other files, and we condense etc
// bc this is so much lines arghhhhhhhh
// although idk i just want it to prefromr good and be simple ish enough  to aedit and code
// etc
// so  it might not be  big deal bc ai is so strong now  Ok Ai Andy 

#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <random>
#include <filesystem>
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
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
#include "gui/gui-layout.h"
#include "gui/gui-editor.h"
#include "gui/hud/player-nameplates.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/transform-debug.h"
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
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
#include "gui/hud/chat-bubble.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-factory.h"
#include "sim/sim-context.h"
#include "combat/weapon-hit.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "config/player-settings.h"
#include "render/outfit-atlas.h"
#include "render/lighting-config.h"
#include "hot-reload/hot-reload-system.h"
#include "profile/local-profile-system.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"

// todo sort 6 7 2026 alphabetical
#include "game/duel.h"
#include "gui/menus/duel-config-menu.h"
#include "terminal/terminal-state.h"
#include "terminal/replay-commands.h"

// 6 9 2026 sort and be more aweosme
// duelamanger should be  a game manager, with specific modes in it
// not all in main todo 
DuelManager gDuelManager;

// Global game objects (pointers set by main() for terminal command access)
Player* gpPlayer = nullptr;
Camera* gpCamera = nullptr;
World* gpWorld = nullptr;
NpcSystem* gpNpcSystem = nullptr;
WeaponSystem* gpWeapons = nullptr;
bool* gpFreecamEnabled = nullptr;
glm::vec3* gpDeathPosition = nullptr;
int* gpSelectedEditorObject = nullptr;
bool* gpEditorMode = nullptr;
std::string* gpActiveGameMode = nullptr;
std::string* gpActiveMapPath = nullptr;
bool* gpWorldLoaded = nullptr;

ReplayRingBuffer* gpReplayRecorder = nullptr;
ReplayPlayer* gpReplayPlayer = nullptr;
ReplayClipSaver* gpReplayClipSaver = nullptr;
ReplayFactory* gpReplayFactory = nullptr;
ReplayBrowser* gpReplayBrowser = nullptr;
ReplayTimeline* gpReplayTimeline = nullptr;
std::unordered_map<std::string, ActorChatState>* gpReplayChatStates = nullptr;
std::vector<std::string>* gpReplayClipsCache = nullptr;
std::unordered_map<int, std::string>* gpCommandBinds = nullptr;
std::unordered_map<int, bool>* gpBindPrev = nullptr;

DuelConfig* gpDuelConfig = nullptr;
MimitaNet::MultiplayerContext* gpMpContext = nullptr;

GameState* gpGameState = nullptr;


// TODO(main-cleanup): move to src/physics/ray-utils.h — also deduplicate with WeaponFire::rayTriangle
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

// TODO(main-cleanup): move to devtools/dev-teleport.cpp
static bool parseTeleportPosition(
    const std::vector<std::string>& args,
    glm::vec3& position)
{
    if (args.size() == 1)
        return std::sscanf(
            args[0].c_str(), "%f,%f,%f",
            &position.x, &position.y, &position.z) == 3;
    if (args.size() == 3)
    {
        try
        {
            position = {
                std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    return false;
}

// TODO(main-cleanup): move to src/physics/ray-utils.h
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


// TODO(main-cleanup): move to gui/gui-editor.cpp (only used by editor mode)
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
    if (argc > 1 && std::string(argv[1]) == "--replay-selftest") {
        ReplayClip clip;
        clip.header.tickRate = 60;
        clip.header.tickCount = 2;
        clip.mapPath = "assets/maps/mimita-duels-map-v3.glb";
        clip.killerId = "player";
        clip.victimId = "npc_100";
        clip.killTick = 30;

        ReplayActorState actorA;
        actorA.id = "player";
        actorA.name = "player";
        actorA.type = "player";
        actorA.position = {0.0f, 0.0f, 0.0f};
        actorA.weaponName = "revolver";
        ReplaySceneFrame frameA;
        frameA.tick = 0;
        frameA.actors.push_back(actorA);

        ReplayActorState actorB = actorA;
        actorB.position = {10.0f, 0.0f, 0.0f};
        ReplaySceneFrame frameB;
        frameB.tick = 60;
        frameB.time = 1.0f;
        frameB.actors.push_back(actorB);
        clip.sceneFrames = {frameA, frameB};

        const std::filesystem::path path =
            std::filesystem::path("build") / "replay-selftest.mclip.json";
        ReplayPlayer playerTest;
        const bool saved = clip.save(path.string());
        const bool loaded = saved && playerTest.loadFromJSON(path.string());
        playerTest.setTimescale(0.25f);
        playerTest.beginPlayback();
        playerTest.update(1.0f);
        const ReplaySceneFrame* interpolated =
            playerTest.currentSceneFrame();
        const bool interpolationOk =
            interpolated && !interpolated->actors.empty() &&
            std::fabs(interpolated->actors.front().position.x - 2.5f) < 0.01f;
        const bool camerasOk =
            playerTest.cameraController().setMode("fp") &&
            playerTest.cameraController().setMode("victim") &&
            playerTest.cameraController().setMode("orbit") &&
            playerTest.cameraController().setMode("freecam");
        std::error_code removeError;
        std::filesystem::remove(path, removeError);
        printf("[REPLAY SELFTEST] save=%d load=%d interpolation=%d cameras=%d\n",
               (int)saved, (int)loaded, (int)interpolationOk, (int)camerasOk);
        return saved && loaded && interpolationOk && camerasOk ? 0 : 1;
    }

    LocalProfileSystem::instance().init();
    MimitaNet::LaunchOptions launchOptions = MimitaNet::parseLaunchOptions(argc, argv);
    if (launchOptions.name.empty())
        launchOptions.name = LocalProfileSystem::instance().currentUsername();
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
        signInMenuHandleChar(codepoint);
        serverInfoMenuHandleChar(codepoint);
        onlineMenuHandleChar(codepoint);
        Terminal::instance().handleChar(codepoint);
    });
    // Terminal key input callback
    glfwSetKeyCallback(engine.window(), [](GLFWwindow*, int key, int scancode, int action, int mods) {
        (void)scancode;
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            signInMenuHandleKey(key, action);
            serverInfoMenuHandleKey(key, action);
            onlineMenuHandleKey(key, action);
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
    LoadDuelStats("default");

    {
        const std::string& res = GetPlayerSettings().resolution;
        int resW = 0, resH = 0;
        if (sscanf(res.c_str(), "%dx%d", &resW, &resH) == 2 && resW > 0 && resH > 0)
        {
            printf("[MAIN] Applying resolution: %s\n", res.c_str());
            glfwSetWindowSize(engine.window(), resW, resH);
            if (engine.renderer)
            {
                engine.renderer->width = resW;
                engine.renderer->height = resH;
            }
        }
    }
    InputCommandSystem::instance().init(engine.window());
    InputCommandSystem::instance().loadBinds("config/accounts/default.json");
    RegisterTeleportCommands();
    Terminal::instance().init(engine.window());
    
    // Effect part system init
    EffectPartSystem::instance().init();
    HotReloadSystem::instance().loadGameDLL();
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
    player.username = LocalProfileSystem::instance().currentUsername();
    player.equippedSlot = GetPlayerSettings().equippedSlot;
    OutfitAtlas::instance().apply(player, GetPlayerSettings().outfitPath);
    printf("[MAIN] player made\n");

    NpcSystem npcSystem;
    bool npcsSpawned = false;
    printf("[MAIN] npc system made\n");

    Camera camera;
    printf("[MAIN] camera made\n");
    printf("[CAMERA] Smoothing default: %.1f\n", camera.smoothness);
    printf("[CAMERA] Raw 1:1 camera enabled\n");

    engine.bindCamera(&camera);
    // onl do this 1 time, not per frame
    // not in the while X loop
    glfwSetWindowUserPointer(engine.window(), &camera);
    printf("[MAIN] camera bound\n");

    // Global replay recorder/player
    static ReplayRingBuffer gReplayRecorder;
    static ReplayPlayer gReplayPlayer;
    static ReplayClipSaver gReplayClipSaver(gReplayRecorder);
    setActiveReplayClipSaver(&gReplayClipSaver);
    static ReplayFactory gReplayFactory(gReplayRecorder);
    static ReplayBrowser gReplayBrowser;
    static ReplayTimeline gReplayTimeline;

    // Game state (declared early because many lambdas capture it)
    GameState gameState = GAME_MENU;
    GameState prevState = GAME_MENU;

    // Connect ReplayFactory to kill notifications
    setReplayFactoryNotifyFn([](const std::string& killerId,
                                 const std::string& victimId,
                                 bool killerAirborne,
                                 bool victimAirborne,
                                 bool roundWinning) {
        gReplayFactory.notifyKill(killerId, victimId, killerAirborne, victimAirborne, roundWinning);
    });

    // Set browser play callback
    gReplayBrowser.setPlayCallback([&gameState](const std::string& path) {
        if (!gReplayPlayer.loadFromJSON(path)) {
            Terminal::instance().addLog("[ERROR] failed to load: " + path);
            return;
        }
        gReplayPlayer.preloadAssets();
        gReplayPlayer.beginPlayback();
        gameState = GAME_PLAYING;
    });
    static std::unordered_map<std::string, ActorChatState> gReplayChatStates;
    static std::vector<std::string> G_REPLAY_CLIPS_CACHE;
    static std::unordered_map<int, std::string> G_COMMAND_BINDS;
    static std::unordered_map<int, bool> G_BIND_PREV;
    static bool gReplayCinematicMode = false;

    // Random number generator for spawn selection
    static std::mt19937 rng(std::random_device{}());

    // also duels
    // todo later, make just like a game mode manager, and make configs
    // using the settings in the game mode manager
    // like number of plrs/npcs, duel time, how much HP, gravity, walkspeed, etc
    // 6 9 2026 todo duel manager needs to be in game manager and not like in main bruh 
    static DuelConfig gDuelConfig;

    static MimitaNet::MultiplayerContext mpContext;

    bool editorMode = false;
    std::string activeGameMode = "sandbox";
    const std::string defaultMapPath =
        "assets/maps/mimita-aabb-only-interior-small-v4.glb";
    std::string activeMapPath;
    int selectedEditorObject = -1;
    WeaponSystem weapons;
    std::unordered_map<std::string, std::unique_ptr<Player>> replayActorModels;
    std::unordered_map<std::string, WeaponViewModel> replayWeaponModels;
    bool freecamEnabled = false;
    glm::vec3 deathPosition{0.0f};
    struct ReplayTestState {
        bool active = false;
        uint32_t tick = 0;
        uint32_t npcId = 0;
    } replayTest;

    // Set global pointers for terminal command access
    gpReplayRecorder = &gReplayRecorder;
    gpReplayPlayer = &gReplayPlayer;
    gpReplayClipSaver = &gReplayClipSaver;
    gpReplayFactory = &gReplayFactory;
    gpReplayBrowser = &gReplayBrowser;
    gpReplayTimeline = &gReplayTimeline;
    gpReplayChatStates = &gReplayChatStates;
    gpReplayClipsCache = &G_REPLAY_CLIPS_CACHE;
    gpCommandBinds = &G_COMMAND_BINDS;
    gpBindPrev = &G_BIND_PREV;
    gpGameState = &gameState;
    gpWorld = &world;
    gpActiveMapPath = &activeMapPath;
    gpNpcSystem = &npcSystem;
    gpFreecamEnabled = &freecamEnabled;
    gpDeathPosition = &deathPosition;
    gpEditorMode = &editorMode;
    gpActiveGameMode = &activeGameMode;
    gpWorldLoaded = &worldLoaded;
    gpDuelConfig = &gDuelConfig;
    gpMpContext = &mpContext;

    // TODO(main-cleanup): move all command registrations to subsystem files
    //   - input/input-commands.cpp → registerInputCommands()
    //   - combat/weapon-system.cpp → registerWeaponCommands()
    //   - devtools/dev-commands.cpp → registerDevCommands()
    //   - replay/replay-commands.cpp → registerReplayCommands()
    //   - gui/gui-editor.cpp → registerGuiEditorCommands()
    //   - game/duel.cpp → registerDuelCommands()
    //   - debug/debug-commands.cpp → registerDebugCommands()
    //   - network/net-commands.cpp → registerNetworkCommands()

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
        "teleport", "Teleport the local player", "teleport x,y,z",
        [&player](const std::vector<std::string>& args) {
            glm::vec3 destination(0.0f);
            if (!parseTeleportPosition(args, destination) ||
                !std::isfinite(destination.x) ||
                !std::isfinite(destination.y) ||
                !std::isfinite(destination.z))
            {
                Terminal::instance().addLog("[ERROR] Usage: teleport x,y,z");
                return;
            }

            player.pos = destination;
            player.vel = glm::vec3(0.0f);
            player.externalImpulse = glm::vec3(0.0f);
            player.inputWishMove = glm::vec2(0.0f);
            player.onGround = false;
            player.jumpHeldPrev = false;
            player.moveHeldPrev = false;
            player.dashHeldPrev = false;
            player.freezeHeldPrev = false;
            player.syncLegacyStateToLayers();
            player.updateModelWorldTransforms();

            if (mpContext.active)
                MimitaNet::mpRequestTeleport(mpContext, destination);

            char line[128];
            snprintf(line, sizeof(line),
                     "[GAMEPLAY] teleported to %.2f,%.2f,%.2f",
                     destination.x, destination.y, destination.z);
            Terminal::instance().addLog(line);
        }
    });

    Terminal::instance().registerCommand({
        "explode", "Instantly kill the local player", "explode",
        [&player](const std::vector<std::string>&) {
            if (player.dead)
            {
                Terminal::instance().addLog("[GAMEPLAY] already dead");
                return;
            }

            DeathSystem::instance().kill(
                player,
                player.username,
                "player",
                "explode",
                glm::vec3(0.0f, 0.0f, 1.0f),
                24.0f);
            if (mpContext.active)
                MimitaNet::mpRequestExplode(mpContext);
            Terminal::instance().addLog("[GAMEPLAY] explode");
        }
    });

    // TODO: Terminal command registration should be moved out of main.cpp.
    // main.cpp should only call registration functions like:
    //   registerReplayCommands(); registerWeaponCommands(); etc.
    // Feature files should expose registration functions that main.cpp calls.
    // This keeps main.cpp as an orchestrator, not a feature container.

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
                // Determine network weapon type from the current equipped weapon
                uint8_t netWeapon = MimitaNet::NETWORK_WEAPON_REVOLVER;
                const WeaponDefinition* wdef = weapons.getCurrentDef(player);
                if (wdef) {
                    if (wdef->id == "shotgun")
                        netWeapon = MimitaNet::NETWORK_WEAPON_SHOTGUN;
                    else if (wdef->id == "godball")
                        netWeapon = MimitaNet::NETWORK_WEAPON_GODBALL;
                    else if (wdef->id == "swordsword")
                        netWeapon = MimitaNet::NETWORK_WEAPON_SWORDSWORD;
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
        "chat", "Send a chat message visible above your character", "chat <message>",
        [&player](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog("[CHAT] usage: chat <message>");
                return;
            }
            std::string message;
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0) message += " ";
                message += args[i];
            }
            if (message.size() > 240)
            {
                message.resize(240);
                Terminal::instance().addLog("[CHAT] message truncated to 240 characters");
            }

            printf("[CHAT] %s: %s\n", player.username.c_str(), message.c_str());
            Terminal::instance().addLog("[CHAT] " + player.username + ": " + message);
            Terminal::instance().addLog("[CHAT] bubble added");

            addChatMessage(player.chatState, message, player.username);
            playChatSound((int)message.size());

            {
                ReplayEffectEvent chatEvent;
                chatEvent.type = "chat";
                chatEvent.sourceActorId = player.username;
                chatEvent.assetId = message;
                chatEvent.lifetime = computeChatDuration((int)message.size());
                captureReplayEffect(chatEvent);
                Terminal::instance().addLog("[CHAT] replay event recorded");
            }

            if (mpContext.active && mpContext.localPlayerId != 0)
            {
                MimitaNet::ChatPacket chatPacket{};
                chatPacket.header.type = MimitaNet::PACKET_CHAT_MESSAGE;
                chatPacket.header.tick = mpContext.tick;
                chatPacket.header.playerId = mpContext.localPlayerId;
                std::memset(chatPacket.senderName, 0, sizeof(chatPacket.senderName));
                std::strncpy(chatPacket.senderName, player.username.c_str(),
                             sizeof(chatPacket.senderName) - 1);
                std::memset(chatPacket.text, 0, sizeof(chatPacket.text));
                std::strncpy(chatPacket.text, message.c_str(), sizeof(chatPacket.text) - 1);
                MimitaNet::mpSendPacket(mpContext, &chatPacket, sizeof(chatPacket));
                Terminal::instance().addLog("[CHAT] replicated");
            }
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
            // During replay playback, also sync with replay freecam
            if (gReplayPlayer.isPlaying()) {
                if (freecamEnabled)
                    gReplayPlayer.cameraController().setMode("freecam");
                else
                    gReplayPlayer.cameraController().setMode("fp");
                Terminal::instance().addLog(std::string("[REPLAY] ") +
                    (freecamEnabled ? "Replay Freecam Enabled" : "Replay Freecam Disabled"));
            } else {
                Terminal::instance().addLog(std::string("[FREECAM] ") + (freecamEnabled ? "enabled" : "disabled"));
            }
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
        "settings_camera_smoothness", "Camera follow smoothness 0-10 (0=locked 5=default 10=floaty)",
        "settings_camera_smoothness <0-10>",
        [&camera](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("camera_smoothness = " + std::to_string(camera.smoothness));
                return;
            }
            float val = std::clamp(std::stof(args[0]), 0.0f, 10.0f);
            camera.smoothness = val;
            Terminal::instance().addLog("camera_smoothness set to " + std::to_string(val));
        }
    });
    Terminal::instance().registerCommand({
        "scm", "Shorter version of settings_camera_smoothness, camera follow smoothness 0-10 (0=locked 5=default 10=floaty)",
        "settings_camera_smoothness <0-10>",
        [&camera](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("camera_smoothness = " + std::to_string(camera.smoothness));
                return;
            }
            float val = std::clamp(std::stof(args[0]), 0.0f, 10.0f);
            camera.smoothness = val;
            Terminal::instance().addLog("camera_smoothness set to " + std::to_string(val));
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
            for (int i = 0; i < count; ++i) {
                glm::vec3 spawnPos = camera.pos + camera.front * (5.0f + i * 1.5f) + glm::vec3(0,0,1);
                npcSystem.spawnNpc(1.0f, spawnPos);
                if (mpContext.active)
                    MimitaNet::mpRequestNpcSpawn(mpContext, spawnPos, 1.0f);
            }
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
        "npc_damage_debug", "Toggle verbose NPC damage/raycast logging", "npc_damage_debug <0|1>",
        [](const std::vector<std::string>& args) {
            DebugConfig::DEBUG_NPC_COMBAT = args.empty() ? !DebugConfig::DEBUG_NPC_COMBAT : args[0] != "0";
            Terminal::instance().addLog(std::string("[DEBUG] NPC combat logging ") +
                (DebugConfig::DEBUG_NPC_COMBAT ? "enabled" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "npc_force_hit", "Force NPC raycast to always hit (debug)", "npc_force_hit <0|1>",
        [](const std::vector<std::string>& args) {
            gNpcForceHit = args.empty() ? !gNpcForceHit : args[0] != "0";
            Terminal::instance().addLog(std::string("[DEBUG] NPC force hit ") +
                (gNpcForceHit ? "ENABLED" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "player_capsule_debug", "Toggle player capsule debug visualization", "player_capsule_debug <0|1>",
        [](const std::vector<std::string>& args) {
            DebugConfig::DEBUG_COLLISION = args.empty() ? !DebugConfig::DEBUG_COLLISION : args[0] != "0";
            Terminal::instance().addLog(std::string("[DEBUG] player capsule debug ") +
                (DebugConfig::DEBUG_COLLISION ? "enabled" : "disabled"));
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
        "npc_difficulty_all", "Set difficulty for all NPCs (1-10)", "npc_difficulty_all <1-10>",
        [&npcSystem](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[NPC COMMAND] usage: npc_difficulty_all <1-10> (current: " + std::to_string((int)npcSystem.globalDifficulty()) + ")");
                return;
            }
            float d = std::clamp(std::stof(args[0]), 1.0f, 10.0f);
            npcSystem.setGlobalDifficulty(d);
            Terminal::instance().addLog("[NPC COMMAND] npc_difficulty_all set to " + std::to_string((int)d));
        }
    });
    Terminal::instance().registerCommand({
        "npc_debug", "Toggle NPC debug overlay (0=off, 1=on)", "npc_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC = !DebugConfig::DEBUG_NPC;
            } else {
                DebugConfig::DEBUG_NPC = args[0] != "0";
            }
            Terminal::instance().addLog(DebugConfig::DEBUG_NPC
                ? "[OK] NPC debug enabled"
                : "[OK] NPC debug disabled");
        }
    });

    Terminal::instance().registerCommand({
        "ragdoll_debug", "Toggle ragdoll debug (0=off, 1=on)", "ragdoll_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_RAGDOLL = !DebugConfig::DEBUG_RAGDOLL;
            } else {
                DebugConfig::DEBUG_RAGDOLL = args[0] != "0";
            }
            Terminal::instance().addLog(DebugConfig::DEBUG_RAGDOLL
                ? "[OK] Ragdoll debug enabled"
                : "[OK] Ragdoll debug disabled");
        }
    });

    Terminal::instance().registerCommand({
        "npc_death_debug", "Deep NPC death debug overlay (0=off, 1=on)", "npc_death_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC_DEATH = !DebugConfig::DEBUG_NPC_DEATH;
            } else {
                DebugConfig::DEBUG_NPC_DEATH = args[0] != "0";
            }
            Terminal::instance().addLog(DebugConfig::DEBUG_NPC_DEATH
                ? "[OK] NPC death debug enabled"
                : "[OK] NPC death debug disabled");
        }
    });
    Terminal::instance().registerCommand({
        "npc_death_debug_freeze", "Freeze corpse simulation for frame-by-frame stepping", "npc_death_debug_freeze <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC_DEATH_FREEZE = !DebugConfig::DEBUG_NPC_DEATH_FREEZE;
            } else {
                DebugConfig::DEBUG_NPC_DEATH_FREEZE = args[0] != "0";
            }
            // Apply freeze to all active corpses
            auto& corpses = DeathSystem::instance().corpses();
            for (const auto& body : corpses) {
                const_cast<DeadBody&>(body).debugFreeze = DebugConfig::DEBUG_NPC_DEATH_FREEZE;
            }
            if (DebugConfig::DEBUG_NPC_DEATH_FREEZE)
                Terminal::instance().addLog("[OK] NPC death freeze enabled — corpses frozen");
            else
                Terminal::instance().addLog("[OK] NPC death freeze disabled — corpses resumed");
        }
    });
    Terminal::instance().registerCommand({
        "npc_death_step", "Advance one physics tick on frozen corpses", "npc_death_step",
        [](const std::vector<std::string>&) {
            if (!DebugConfig::DEBUG_NPC_DEATH_FREEZE) {
                Terminal::instance().addLog("[ERROR] npc_death_debug_freeze must be enabled first");
                return;
            }
            auto& corpses = DeathSystem::instance().corpses();
            for (auto& body : const_cast<std::vector<DeadBody>&>(corpses)) {
                body.debugFreeze = false;
            }
            Terminal::instance().addLog("[OK] stepping one frame — corpses will resume next tick");
        }
    });

    Terminal::instance().registerCommand({
        "transform_debug", "Enable/disable transform write logging (0=off, 1=on)", "transform_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                TransformDebug::instance().setEnabled(!TransformDebug::instance().isEnabled());
            } else {
                TransformDebug::instance().setEnabled(args[0] != "0");
            }
            Terminal::instance().addLog(TransformDebug::instance().isEnabled()
                ? "[OK] transform debug enabled"
                : "[OK] transform debug disabled");
        }
    }, "2026-06-13");
    Terminal::instance().registerCommand({
        "entity_debug", "Filter transform logging to a specific entity ID", "entity_debug <entityId>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                TransformDebug::instance().setTargetEntity("");
                Terminal::instance().addLog("[OK] entity debug filter cleared");
                return;
            }
            TransformDebug::instance().setTargetEntity(args[0]);
            Terminal::instance().addLog("[OK] entity debug filter set to: " + args[0]);
        }
    }, "2026-06-13");
    Terminal::instance().registerCommand({
        "transform_history", "Show recent transform writes for an entity", "transform_history <entityId>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] usage: transform_history <entityId>");
                return;
            }
            const auto* history = TransformDebug::instance().getHistory(args[0]);
            if (!history || history->empty()) {
                Terminal::instance().addLog("[ERROR] no history for: " + args[0]);
                return;
            }
            char buf[512];
            snprintf(buf, sizeof(buf), "=== TRANSFORM HISTORY for %s (%zu events) ===",
                     args[0].c_str(), history->size());
            Terminal::instance().addLog(buf);
            int i = 0;
            for (const auto& ev : *history) {
                snprintf(buf, sizeof(buf), "  %d. %s  pos=(%.1f %.1f %.1f)->(%.1f %.1f %.1f)",
                         ++i, ev.system.c_str(),
                         ev.oldPos.x, ev.oldPos.y, ev.oldPos.z,
                         ev.newPos.x, ev.newPos.y, ev.newPos.z);
                Terminal::instance().addLog(buf);
            }
        }
    }, "2026-06-13");

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

    // TODO(main-cleanup): move registerDebugToggle lambdas + calls to debug/debug-commands.cpp
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
    registerDebugToggle("debug_blood_rays", DebugConfig::DEBUG_BLOOD_RAYS);
    registerDebugToggle("debug_blood_hits", DebugConfig::DEBUG_BLOOD_HITS);
    registerDebugToggle("debug_blood_force", DebugConfig::DEBUG_BLOOD_FORCE);
    registerDebugToggle("debug_debris", DebugConfig::DEBUG_DEBRIS);
    registerDebugToggle("godball_debug", DebugConfig::DEBUG_GODBALL);
    registerDebugToggle("final_kill_debug", DebugConfig::DEBUG_NPC_DEATH);  // reuse existing flag
    registerDebugToggle("collision_debug", DebugConfig::DEBUG_COLLISION_SYSTEM);
    registerDebugToggle("npc_damage_debug", DebugConfig::DEBUG_NPC_COMBAT);
    registerDebugToggle("npc_movement_debug", DebugConfig::DEBUG_NPC_MOVEMENT);
    registerDebugToggle("ragdoll_debug", DebugConfig::DEBUG_RAGDOLL);
    registerDebugToggle("replay_debug", DebugConfig::DEBUG_REPLAY);
    registerDebugToggle("networking_debug", DebugConfig::DEBUG_NETWORKING);
    registerDebugToggle("duel_debug", DebugConfig::DEBUG_DUEL);
    registerDebugToggle("animation_debug", DebugConfig::DEBUG_ANIMATION);
    registerDebugToggle("ui_debug", DebugConfig::DEBUG_UI);
    registerDebugToggle("physics_debug", DebugConfig::DEBUG_PHYSICS);
    registerDebugToggle("combat_debug", DebugConfig::DEBUG_NPC_COMBAT);
    registerDebugToggle("render_debug", DebugConfig::DEBUG_RENDER);

    Terminal::instance().registerCommand({
        "fakelag_mode", "Set fake lag mode (0=off, 1=random, 2=static)",
        "fakelag_mode <0|1|2>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[FAKELAG] mode=" + std::to_string(mpContext.fakeLagMode));
                return;
            }
            MimitaNet::mpSetFakeLagMode(mpContext, std::stoi(args[0]));
            Terminal::instance().addLog(
                "[FAKELAG] mode=" + std::to_string(mpContext.fakeLagMode));
        }
    });
    Terminal::instance().registerCommand({
        "fakelag_amount_static", "Set static fake lag in milliseconds",
        "fakelag_amount_static <ms>",
        [](const std::vector<std::string>& args) {
            if (!args.empty())
                MimitaNet::mpSetFakeLagStatic(mpContext, std::stoi(args[0]));
            Terminal::instance().addLog(
                "[FAKELAG] static=" + std::to_string(mpContext.fakeLagStaticMs));
        }
    });
    Terminal::instance().registerCommand({
        "fakelag_amount_min", "Set random fake lag minimum",
        "fakelag_amount_min <ms>",
        [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                mpContext.fakeLagMinMs = std::clamp(std::stoi(args[0]), 0, 5000);
                if (mpContext.fakeLagMaxMs < mpContext.fakeLagMinMs)
                    mpContext.fakeLagMaxMs = mpContext.fakeLagMinMs;
                mpContext.fakeLagNextRandomizeMs = 0;
            }
            Terminal::instance().addLog(
                "[FAKELAG] min=" + std::to_string(mpContext.fakeLagMinMs));
        }
    });
    Terminal::instance().registerCommand({
        "fakelag_amount_max", "Set random fake lag maximum",
        "fakelag_amount_max <ms>",
        [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                mpContext.fakeLagMaxMs = std::clamp(std::stoi(args[0]), 0, 5000);
                if (mpContext.fakeLagMinMs > mpContext.fakeLagMaxMs)
                    mpContext.fakeLagMinMs = mpContext.fakeLagMaxMs;
                mpContext.fakeLagNextRandomizeMs = 0;
            }
            Terminal::instance().addLog(
                "[FAKELAG] max=" + std::to_string(mpContext.fakeLagMaxMs));
        }
    });

    // TODO(main-cleanup): move registerReplayCommands() to replay/replay-commands.cpp
    // Replay terminal commands
    // Register replay terminal commands (moved to src/terminal/replay-commands.cpp)
    registerReplayCommands();
    Terminal::instance().registerCommand({
        "gui_edit", "Toggle GUI editor mode", "gui_edit [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                GuiEditor::instance().toggle();
            } else {
                GuiEditor::instance().setEnabled(args[0] == "1");
            }
            const bool on = GuiEditor::instance().isEnabled();
            printf("[GUI EDIT] %s\n", on ? "enabled" : "disabled");
            Terminal::instance().addLog(on
                ? "[GUI] Edit mode ON: buttons disabled, drag layout items only"
                : "[GUI] Edit mode OFF: buttons work normally");
        }
    });
    Terminal::instance().registerCommand({
        "gui_save", "Save all GUI layout positions to JSON files in config/gui/", "gui_save",
        [](const std::vector<std::string>&) {
            GuiLayoutManager::instance().saveAll();
            const std::vector<std::string> unsaved = GuiLayoutManager::instance().unsavedLayouts();
            if (unsaved.empty() && !GuiLayoutManager::instance().hasUnsaved()) {
                Terminal::instance().addLog("[GUI] no unsaved layouts");
            }
        }
    });
    Terminal::instance().registerCommand({
        "gui_save_menu", "Save only the current menu's layout", "gui_save_menu",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GUI] usage: gui_save_menu <filepath>");
                Terminal::instance().addLog("[GUI] example: gui_save_menu config/gui/main-menu.json");
                return;
            }
            if (GuiLayoutManager::instance().saveLayout(args[0])) {
                Terminal::instance().addLog("[GUI] saved " + args[0]);
            } else {
                Terminal::instance().addLog("[GUI] failed to save " + args[0]);
            }
        }
    });
    Terminal::instance().registerCommand({
        "gui_load", "Reload a GUI layout JSON from disk", "gui_load <filepath>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GUI] usage: gui_load <filepath>");
                return;
            }
            if (GuiLayoutManager::instance().reloadLayout(args[0])) {
                Terminal::instance().addLog("[GUI] reloaded " + args[0]);
            } else {
                Terminal::instance().addLog("[GUI] failed to reload " + args[0]);
            }
        }
    });
    Terminal::instance().registerCommand({
        "gui_reset", "Reset a menu to built-in defaults", "gui_reset <filepath>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GUI] usage: gui_reset <filepath>");
                Terminal::instance().addLog("[GUI] example: gui_reset config/gui/main-menu.json");
                return;
            }
            GuiLayoutManager::instance().resetLayout(args[0]);
            Terminal::instance().addLog("[GUI] reset " + args[0] + " to defaults");
        }
    });
    Terminal::instance().registerCommand({
        "gui_reset_all", "Reset all menus to built-in defaults", "gui_reset_all",
        [](const std::vector<std::string>&) {
            GuiLayoutManager::instance().resetAll();
            Terminal::instance().addLog("[GUI] all layouts reset to defaults");
        }
    });
    Terminal::instance().registerCommand({
        "gui_debug_overlap", "Toggle overlap debug visualization", "gui_debug_overlap [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                uiSetOverlapDebug(!uiOverlapDebugEnabled());
            } else {
                uiSetOverlapDebug(args[0] == "1");
            }
            const bool on = uiOverlapDebugEnabled();
            printf("[GUI OVERLAP DEBUG] %s\n", on ? "enabled" : "disabled");
            Terminal::instance().addLog(on
                ? "[GUI] Overlap debug ON: overlapping widgets highlighted in red"
                : "[GUI] Overlap debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "replay_test",
        "Record a deterministic gameplay replay and validate it in Blender",
        "replay_test",
        [&gameState, &activeMapPath, &camera, &player, &npcSystem,
         &replayTest](const std::vector<std::string>&) {
            if (gameState != GAME_PLAYING || activeMapPath.empty()) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] Enter a loaded game map first");
                return;
            }
            if (mpContext.active) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] Disabled during multiplayer");
                return;
            }
            if (replayTest.active) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] A recording is already active");
                return;
            }

            npcSystem.destroyAll();
            if (gReplayRecorder.isRecording())
                gReplayRecorder.stopRecording();
            Terminal::instance().execute("replay.record");
            if (!gReplayRecorder.isRecording()) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] Could not start recording");
                return;
            }

            glm::vec3 forward = camera.front;
            if (glm::length(forward) < 0.001f)
                forward = glm::vec3(0.0f, 1.0f, 0.0f);
            forward = glm::normalize(forward);
            const glm::vec3 spawnPosition =
                camera.pos + forward * 6.0f + glm::vec3(0.0f, 0.0f, 1.0f);
            replayTest.npcId = npcSystem.nextNpcId();
            npcSystem.spawnNpc(replayTest.npcId, 1.0f, spawnPosition);
            if (!npcSystem.all().empty()) {
                Npc& npc = npcSystem.all().back();
                npc.trainingMode = 0;
                npc.body.maxHp = 500;
                npc.body.currentHp = 500;
            }

            player.dead = false;
            player.currentHp = player.maxHp;
            replayTest.tick = 0;
            replayTest.active = true;
            Terminal::instance().addLog(
                "[REPLAY TEST] Running 300-tick automated scenario");
        }
    });

    // 6 7 2026 omg todo
    // put ALL these commands into terminal folder like by itself
    Terminal::instance().registerCommand({
        "duel.start",
        "Start duel mode",
        "duel.start [npcCount]",
        [&player, &npcSystem, &world](const std::vector<std::string>& args)
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
                npcSystem,
                world);

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

    // TODO(main-cleanup): extract main loop body into tickGame(engine, ...) function
    while (engine.running())
    {
        HotReloadSystem::instance().reloadGameDLLIfChanged();
        float dt = engine.beginFrame();
        updatePlayerProceduralHotReload(dt);
        bool worldPassRan = false;

        audioUpdate(dt);
        DebugVis::update();
        uiSetDebug(DebugVis::ui());

        if (gameState != prevState)
        {
            printf("[MAIN] gameState changed %d -> %d\n", (int)prevState, (int)gameState);
            if (gameState == GAME_PLAYING)
            {
                SandboxMapSelection sandboxSelection =
                    getPendingSandboxMapSelection();
                if (sandboxSelection.shouldStart)
                {
                    const std::string selectedPath = sandboxSelection.mapPath;
                    clearPendingSandboxMapSelection();
                    printf("[SANDBOX MAP] selected path=%s\n", selectedPath.c_str());

                    if (!loadWorldFromGLB(world, selectedPath.c_str()))
                    {
                        const std::string message =
                            "Failed to load: " + selectedPath;
                        printf("[SANDBOX MAP ERROR] %s\n", message.c_str());
                        reportSandboxMapLoadResult(message, false);
                        gameState = GAME_MENU;
                    }
                    else
                    {
                        activeMapPath = selectedPath;
                        worldLoaded = true;
                        npcSystem.destroyAll();
                        npcsSpawned = false;
                        player.reset();

                        if (!world.spawnPoints.empty())
                        {
                            std::uniform_int_distribution<size_t> dist(0, world.spawnPoints.size() - 1);
                            world.selectedSpawnIndex = (int)dist(rng);
                            const SpawnPoint& spawn = world.spawnPoints[world.selectedSpawnIndex];
                            player.pos = spawn.position;
                            player.respawnPosition = spawn.position;
                            printf("[SANDBOX SPAWN] selected index=%d name=%s world=(%.3f %.3f %.3f)\n",
                                   world.selectedSpawnIndex, spawn.tag.c_str(), spawn.position.x,
                                   spawn.position.y, spawn.position.z);
                        }
                        else
                        {
                            world.selectedSpawnIndex = -1;
                            const glm::vec3 fallback{1.0f, 5.0f, 60.0f};
                            player.pos = fallback;
                            player.respawnPosition = fallback;
                            printf("[SANDBOX SPAWN WARNING] no GLB spawns; fallback=(%.1f %.1f %.1f)\n",
                                   fallback.x, fallback.y, fallback.z);
                        }

                        reportSandboxMapLoadResult(
                            "Loaded: " + selectedPath, true);
                        printf("[SANDBOX MAP] load success path=%s spawns=%zu\n",
                               activeMapPath.c_str(), world.spawnPoints.size());
                    }
                }

                if (gameState == GAME_PLAYING && !worldLoaded)
                {
                    printf("[MAIN] PLAY requested without sandbox selection; loading default world\n");
                    if (loadWorldFromGLB(world, defaultMapPath.c_str()))
                    {
                        worldLoaded = true;
                        activeMapPath = defaultMapPath;
                    }
                    else
                    {
                        printf("[MAIN ERROR] default world failed to load: %s\n",
                               defaultMapPath.c_str());
                        gameState = GAME_MENU;
                    }
                }

                // Handle duel config from menu
                {
                    DuelConfigResult dcr = getPendingDuelConfig();
                    if (dcr.startDuel) {
                        DuelConfig cfg;
                        cfg.numNpcs = dcr.numNpcs;
                        cfg.killsToWin = dcr.killsToWin;
                        cfg.duelLengthSeconds = dcr.duelLengthSeconds;
                        cfg.npcDifficulty = dcr.npcDifficulty;
                        cfg.enabled = true;
                        gDuelManager.start(cfg, player, npcSystem, world);
                        activeMapPath = cfg.mapPath;
                        worldLoaded = !world.mesh.verts.empty();
                        clearPendingDuelConfig();
                    }
                }
                // Handle multiplayer connect from menu
                {
                    MultiplayerConnectInfo mci = getPendingMultiplayerConnect();
                    if (mci.shouldConnect) {
                        if (activeMapPath != defaultMapPath &&
                            loadWorldFromGLB(world, defaultMapPath.c_str()))
                        {
                            activeMapPath = defaultMapPath;
                            worldLoaded = true;
                            printf("[MAIN NET] restored server-compatible map=%s\n",
                                   activeMapPath.c_str());
                        }
                        player.username = LocalProfileSystem::instance().currentUsername();
                        if (MimitaNet::mpInit(mpContext, mci.address, player.username)) {
                            printf("[MAIN] multiplayer connected to %s\n", mci.address.c_str());
                            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        }
                        clearPendingMultiplayerConnect();
                    }
                }
                if (gameState == GAME_PLAYING && worldLoaded &&
                    !gReplayRecorder.isRecording()) {
                    Terminal::instance().execute("replay.record");
                    Terminal::instance().addLog(
                        "[REPLAY] 60 second ring buffer active");
                }
                bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
                glfwSetInputMode(engine.window(), GLFW_CURSOR,
                    gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !duelMatchOver
                        ? GLFW_CURSOR_DISABLED
                        : GLFW_CURSOR_NORMAL);
            }
            else
            {
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            prevState = gameState;
        }

        // Update cursor mode when duel phase changes (e.g. Active → MatchEnd)
        {
            static DuelPhase prevDuelPhase = DuelPhase::Off;
            if (gDuelManager.phase() != prevDuelPhase) {
                bool enteredMatchEnd = gDuelManager.phase() == DuelPhase::MatchEnd;
                if (enteredMatchEnd && !gDuelManager.finalKillReplayLoaded) {
                    Debug::log(Debug::Category::Duel, "[DUEL] final kill detected");
                    Debug::log(Debug::Category::Duel, "[DUEL] match winner determined%s",
                        gDuelManager.matchWinner() == DuelTeam::Player ? " (PLAYER)" : " (NPC)");
                    uint32_t now = gReplayRecorder.currentTick();
                    uint32_t start = now > 480 ? now - 480 : 0; // 8 seconds back
                    uint32_t end = now;
                    ReplayClip clip = gReplayRecorder.makeClip(start, end, start, "", "");
                    if (!clip.sceneFrames.empty()) {
                        std::string tmpPath = "replays/_final_kill_temp.json";
                        clip.save(tmpPath);
                        Debug::log(Debug::Category::Replay,
                            "[REPLAY START CHECK] path=%s frames=%zu",
                            tmpPath.c_str(), clip.sceneFrames.size());
                        if (gReplayPlayer.loadFromJSON(tmpPath)) {
                            gDuelManager.finalKillReplayLoaded = true;
                            gDuelManager.finalKillReplayPath = tmpPath;
                            gDuelManager.finalKillReplayTime = 0.0f;
                            Debug::log(Debug::Category::Replay,
                                "[REPLAY] loading clip=%s", tmpPath.c_str());
                            Debug::log(Debug::Category::Replay,
                                "[REPLAY] load success=1 frames=%u",
                                gReplayPlayer.totalTicks());
                        } else {
                            Debug::log(Debug::Category::Replay,
                                "[REPLAY] load failed reason=loadFromJSON returned false");
                            Debug::log(Debug::Category::Replay,
                                "[REPLAY START CHECK] FAILED replay clip load error");
                        }
                    } else {
                        Debug::log(Debug::Category::Replay,
                            "[REPLAY START CHECK] FAILED zero scene frames");
                        Debug::log(Debug::Category::Replay,
                            "[REPLAY START CHECK] path=%s", "replays/_final_kill_temp.json");
                    }
                }
                prevDuelPhase = gDuelManager.phase();
                bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
                glfwSetInputMode(engine.window(), GLFW_CURSOR,
                    gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !duelMatchOver
                        ? GLFW_CURSOR_DISABLED
                        : GLFW_CURSOR_NORMAL);
            }
        }

        // Dev tools update
        DevOverlay::instance().update(dt);
        NpcSelectionManager::instance().update();

        // Terminal toggle on grave accent (`/~)
        static bool gravePrev = false;
        bool graveDown = glfwGetKey(engine.window(), GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
        if (graveDown && !gravePrev) {
            Terminal::instance().toggle();
            bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                Terminal::instance().isOpen() ? GLFW_CURSOR_NORMAL :
                (gameState == GAME_PLAYING && !duelMatchOver ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL));
        }
        gravePrev = graveDown;

        if (gameState == GAME_PLAYING)
        {
            DebugVis::beginCollisionFrame();
            gReplayPlayer.update(dt);
            const bool replayPlaybackActive = gReplayPlayer.isPlaying();
            setReplayCaptureEnabled(!replayPlaybackActive);

            // Fixed-tick simulation accumulator
            // Accumulate real dt and step simulation at SIM_DT rate
            simAccumulator += (double)dt;

            while (simAccumulator >= SIM_DT) {
                InputFrame tickFrame;

                if (!replayPlaybackActive) {
                    // Live input: build InputFrame from keyboard + terminal override
                    InputCommandSystem::instance().setKeyboardEnabled(!Terminal::instance().isOpen());
                    // tickFrame = buildInputFrame(engine.window(), camera);

                    // lock mvoemnet if countdown in duels 6 7 2026 
                    tickFrame = buildInputFrame(engine.window(), camera);

                    if (gDuelManager.phase() == DuelPhase::Countdown ||
                        gDuelManager.phase() == DuelPhase::MatchEnd)
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

                    if (replayTest.active) {
                        tickFrame = {};
                        if (replayTest.tick < 45) {
                            tickFrame.moveY = 1.0f;
                            tickFrame.movementPressed = true;
                        }
                        if (replayTest.tick >= 20 &&
                            replayTest.tick < 24) {
                            tickFrame.jump = true;
                            tickFrame.jumpPressed =
                                replayTest.tick == 20;
                        }
                        if (replayTest.tick == 55) {
                            tickFrame.moveY = 1.0f;
                            tickFrame.movementPressed = true;
                            tickFrame.dashPressed = true;
                        }

                        if (replayTest.tick == 90) {
                            weapons.equip(player, 1);
                            Terminal::instance().execute("shoot");
                        } else if (replayTest.tick == 150) {
                            weapons.equip(player, 3);
                            Terminal::instance().execute("shoot");
                        } else if (replayTest.tick == 180) {
                            tickFrame.reloadPressed = true;
                            Terminal::instance().execute("reload");
                        } else if (replayTest.tick == 220) {
                            for (Npc& npc : npcSystem.all()) {
                                if (npc.id != replayTest.npcId ||
                                    npc.body.dead)
                                    continue;
                                DeathSystem::instance().kill(
                                    npc.body,
                                    "npc_" + std::to_string(npc.id),
                                    "npc",
                                    player.username,
                                    camera.front,
                                    24.0f);
                                break;
                            }
                        }
                    }
                }

                const bool recordingReplayTick =
                    gReplayRecorder.isRecording() && !replayPlaybackActive;
                uint32_t replayTick = 0;
                if (recordingReplayTick) {
                    replayTick = gReplayRecorder.currentTick();
                    gReplayRecorder.recordFrame(tickFrame);
                }

                // Run simulation for this tick
                if (!freecamEnabled && !replayPlaybackActive)
                    simulateTick(simContext, tickFrame);

                // Capture death position for camera orbit (player.pos stays at death location)
                if (player.dead && glm::length(deathPosition) < 0.1f)
                    deathPosition = player.pos;
                // Reset death position on respawn
                if (!player.dead)
                    deathPosition = glm::vec3(0.0f);

                if (recordingReplayTick) {
                    ReplaySceneFrame sceneFrame;
                    sceneFrame.tick = (int)replayTick;
                    sceneFrame.time = (float)sceneFrame.tick / 60.0f;

                    // Camera
                    sceneFrame.camera.position = camera.pos;
                    sceneFrame.camera.rotation = glm::vec3(camera.pitch, 0.0f, camera.yaw);
                    sceneFrame.camera.fov = camera.fov;

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
                    playerActor.dead = player.dead;
                    playerActor.grounded = player.onGround;
                    playerActor.collidable = !player.dead;
                    playerActor.fade = 0.0f;
                    playerActor.outfitPath = GetPlayerSettings().outfitPath;
                    {
                        const WeaponDefinition* wdef = weapons.getCurrentDef(player);
                        if (wdef) {
                            playerActor.weaponName = wdef->id;
                            playerActor.weaponModelPath = wdef->modelPath;
                        } else {
                            playerActor.weaponName = "none";
                            playerActor.weaponModelPath = "";
                        }
                        auto wit = player.weaponRuntimes.find(player.equippedWeaponId);
                        if (wit != player.weaponRuntimes.end()) {
                            playerActor.currentAmmo = wit->second.currentAmmo;
                            playerActor.reserveAmmo = wit->second.reserveAmmo;
                        }
                    }
                    playerActor.reloading = weapons.isReloading(player);
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
                        npcActor.dead = npc.body.dead;
                        npcActor.grounded = npc.body.onGround;
                        npcActor.collidable = !npc.body.dead;
                        npcActor.fade = 0.0f;
                        npcActor.outfitPath = "";
                        {
                            const WeaponDefinition* wdef = weapons.getDefForSlot(npc.body.equippedSlot);
                            if (wdef) {
                                npcActor.weaponName = wdef->id;
                                npcActor.weaponModelPath = wdef->modelPath;
                            } else {
                                npcActor.weaponName = "none";
                                npcActor.weaponModelPath = "";
                            }
                        }
                        {
                            auto wit = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
                            if (wit != npc.body.weaponRuntimes.end()) {
                                npcActor.currentAmmo = wit->second.currentAmmo;
                                npcActor.reserveAmmo = wit->second.reserveAmmo;
                            }
                        }
                        npcActor.animationState = npcStateName(npc.stateMachine.currentState);
                        npcActor.bodyParts = captureReplayBodyParts(npc.body);
                        sceneFrame.actors.push_back(npcActor);
                    }
                    DeathSystem::instance().appendReplayActors(sceneFrame.actors);

                    gReplayRecorder.recordSceneFrame(sceneFrame);
                    gReplayClipSaver.update();
                    gReplayFactory.update();
                    GuiLayoutManager::instance().pollReload();

                    if (replayTest.active) {
                        ++replayTest.tick;
                        if (replayTest.tick >= 300) {
                            gReplayRecorder.stopRecording();
                            const std::string path =
                                generateReplayExportPath();
                            const bool exported =
                                gReplayRecorder.exportToJSON(path);
                            replayTest.active = false;
                            if (!exported) {
                                Terminal::instance().addLog(
                                    "[REPLAY TEST] Replay export failed");
                            } else {
                                const std::string absolutePath =
                                    std::filesystem::absolute(path).string();
                                const std::string command =
                                    "python devscripts/replay-validation-runner.py "
                                    "--replay \"" + absolutePath + "\"";
                                std::thread([command, absolutePath]() {
                                    printf(
                                        "[REPLAY TEST] Starting validation for %s\n",
                                        absolutePath.c_str());
                                    const int result =
                                        std::system(command.c_str());
                                    printf(
                                        "[REPLAY TEST] Validation process exit=%d\n",
                                        result);
                                }).detach();
                                Terminal::instance().addLog(
                                    "[REPLAY TEST] Replay exported; "
                                    "headless validation started");
                            }
                            Terminal::instance().execute("replay.record");
                        }
                    }
                }

                simAccumulator -= SIM_DT;
            }

            // Process local NPC commands only outside authoritative multiplayer.
            if (!mpContext.active) {
                ProcessNpcSpawnCommands(npcSystem, camera, world, player);
                ProcessNpcTrainingSpawnCommands(npcSystem, camera, world, player);
            }

            // Multiplayer tick - receive snapshots
            if (mpContext.active) {
                MimitaNet::mpTick(mpContext, player.username, dt);
                if (!mpContext.approvedLocalName.empty())
                    player.username = mpContext.approvedLocalName;

                for (const MimitaNet::NetworkShotEvent& event : mpContext.shotEvents)
                {
                    const bool localShooter =
                        event.shooterPlayerId == mpContext.localPlayerId;
                    const bool localTarget =
                        event.targetPlayerId == mpContext.localPlayerId;
                    const auto shooterInfo =
                        mpContext.playerRegistry.find(event.shooterPlayerId);
                    const auto targetInfo =
                        mpContext.playerRegistry.find(event.targetPlayerId);
                    const std::string shooterName =
                        shooterInfo != mpContext.playerRegistry.end()
                        ? shooterInfo->second.name
                        : "player_" + std::to_string(event.shooterPlayerId);
                    const std::string targetName =
                        targetInfo != mpContext.playerRegistry.end()
                        ? targetInfo->second.name
                        : "player_" + std::to_string(event.targetPlayerId);

                    printf("[NET SHOT APPLY] shooter=%u serial=%u target=%u "
                           "damage=%d weapon=%u impact=%u damageConfirmed=%d\n",
                           event.shooterPlayerId, event.shotSerial,
                           event.targetPlayerId, event.damage, event.weapon,
                           event.impactType, (int)event.damageConfirmed);

                    if (event.damageConfirmed && localTarget)
                    {
                        player.currentHp = event.targetHealth;
                        mpContext.localServerHealth = event.targetHealth;
                    }
                    else if (event.damageConfirmed)
                    {
                        auto remote = mpContext.remotePlayers.find(
                            event.targetPlayerId);
                        if (remote != mpContext.remotePlayers.end())
                            remote->second.currentHp = event.targetHealth;
                        auto interpolation =
                            mpContext.remotePlayerInterpolation.find(
                                event.targetPlayerId);
                        if (interpolation !=
                            mpContext.remotePlayerInterpolation.end())
                            interpolation->second.target.health =
                                event.targetHealth;
                    }

                    if (!localShooter && glm::length(event.knockback) > 0.001f)
                    {
                        if (localTarget)
                            player.vel += event.knockback;
                        else
                        {
                            auto remote = mpContext.remotePlayers.find(
                                event.targetPlayerId);
                            if (remote != mpContext.remotePlayers.end())
                                remote->second.vel += event.knockback;
                        }
                    }

                    if (!localShooter)
                    {
                        auto remoteShooter = mpContext.remotePlayers.find(
                            event.shooterPlayerId);
                        if (remoteShooter != mpContext.remotePlayers.end() &&
                            (event.effectFlags &
                             MimitaNet::SHOT_EFFECT_WEAPON_TRIGGER))
                        {
                            remoteShooter->second.networkShootEffectTimer = 0.1f;
                            remoteShooter->second.networkWeaponState |= 1u;
                        }

                        if (event.weapon ==
                            MimitaNet::NETWORK_WEAPON_REVOLVER)
                        {
                            ReplayEffectEvent gunshotEvent;
                            gunshotEvent.type = "gunshot";
                            gunshotEvent.position = event.origin;
                            gunshotEvent.direction = event.direction;
                            gunshotEvent.from = event.origin;
                            gunshotEvent.to = event.hit;
                            gunshotEvent.normal = event.normal;
                            gunshotEvent.sourceActorId = shooterName;
                            captureReplayEffect(gunshotEvent);
                        }

                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_MUZZLE)
                            EffectPartSystem::instance().spawnMuzzleFlash(
                                event.origin, shooterName);
                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_TRACER)
                            EffectPartSystem::instance().spawnTracer(
                                event.origin, event.hit, shooterName);
                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_SHOOT_SOUND)
                        {
                            playWorldSound(
                                "revolvershoot", event.origin,
                                1.0f, 1.0f, 80.0f);
                        }

                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_WORLD_IMPACT)
                        {
                            EffectPartSystem::instance().spawnWorldImpact(
                                event.hit, event.normal);
                            EffectPartSystem::instance().spawnBulletImpact(
                                event.hit);
                        }
                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_DEBRIS) {
                            float debrisForce = std::clamp(event.power / 40.0f, 0.1f, 5.0f);
                            EffectPartSystem::instance().spawnWorldDebris(
                                event.hit, event.normal, debrisForce);
                        }
                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_ENTITY_IMPACT)
                        {
                            EffectPartSystem::instance().spawnEntityImpact(
                                event.hit, event.normal,
                                shooterName, targetName);
                        }
                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_BLOOD)
                        {
                            EffectPartSystem::instance().spawnDamage(
                                event.hit, targetName, event.damage);
                            EffectPartSystem::instance().spawnBloodEffect(
                                event.hit, event.direction, event.power,
                                shooterName, targetName);
                        }
                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_HIT_SOUND)
                        {
                            playWorldSound(
                                event.impactType == MimitaNet::SHOT_IMPACT_WORLD
                                    ? "hitworld" : "player_hurt",
                                event.hit, 0.9f, 1.0f, 40.0f);
                        }
                        printf("[NET SHOT RECONSTRUCT] shooter=%u serial=%u "
                               "localShooter=0 impact=%u origin=(%.2f %.2f %.2f) "
                               "hit=(%.2f %.2f %.2f)\n",
                               event.shooterPlayerId, event.shotSerial,
                               event.impactType,
                               event.origin.x, event.origin.y, event.origin.z,
                               event.hit.x, event.hit.y, event.hit.z);
                    }
                    else
                    {
                        printf("[NET SHOT OWNERSHIP] shooter=%u serial=%u "
                               "visualsSkipped=1 reason=local-prediction\n",
                               event.shooterPlayerId, event.shotSerial);
                    }

                    if (event.damageConfirmed && event.killed && !localTarget)
                    {
                        auto remote = mpContext.remotePlayers.find(
                            event.targetPlayerId);
                        if (remote != mpContext.remotePlayers.end())
                        {
                            remote->second.dead = false;
                            DeathSystem::instance().kill(
                                remote->second,
                                "net_player_" + std::to_string(event.targetPlayerId),
                                "player",
                                shooterName,
                                event.direction,
                                event.weapon == MimitaNet::NETWORK_WEAPON_GODBALL
                                    ? 18.0f : 12.0f);
                        }
                    }
                }
                mpContext.shotEvents.clear();

                for (const auto& chatMsg : mpContext.incomingChatMessages)
                {
                    printf("[CHAT] %s: %s\n", chatMsg.senderName.c_str(), chatMsg.text.c_str());
                    for (auto& kv : mpContext.remotePlayers)
                    {
                        if (kv.second.username == chatMsg.senderName)
                        {
                            addChatMessage(kv.second.chatState, chatMsg.text, chatMsg.senderName);
                            break;
                        }
                    }
                    playChatSound((int)chatMsg.text.size());

                    ReplayEffectEvent chatEvent;
                    chatEvent.type = "chat";
                    chatEvent.sourceActorId = chatMsg.senderName;
                    chatEvent.assetId = chatMsg.text;
                    chatEvent.lifetime = computeChatDuration((int)chatMsg.text.size());
                    captureReplayEffect(chatEvent);
                }
                mpContext.incomingChatMessages.clear();

                MimitaNet::mpReconcileLocalPlayer(mpContext, player, dt);

                // Sync server NPCs to local NpcSystem for AI
                {
                    static std::unordered_set<uint32_t> spawnedNpcIds;
                    for (const auto& kv : mpContext.remoteNpcs) {
                        const uint32_t entityId = kv.first;
                        if (spawnedNpcIds.find(entityId) == spawnedNpcIds.end()) {
                            spawnedNpcIds.insert(entityId);
                            float diff = 1.0f;
                            npcSystem.spawnNpc(entityId, diff, kv.second.pos);
                        }
                    }
                    // Remove local NPCs whose server entity no longer exists
                    for (auto it = spawnedNpcIds.begin(); it != spawnedNpcIds.end(); ) {
                        if (mpContext.remoteNpcs.find(*it) == mpContext.remoteNpcs.end()) {
                            npcSystem.destroySelected({*it});
                            it = spawnedNpcIds.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                // Send input to server if we have an assigned ID
                if (mpContext.localPlayerId != 0) {
                    InputFrame mpInput = buildInputFrame(engine.window(), camera);
                    MimitaNet::InputPacket in{};
                    in.header.type = MimitaNet::PACKET_INPUT;
                    in.header.tick = mpContext.tick;
                    in.header.playerId = mpContext.localPlayerId;
                    in.wishX = mpInput.moveX;
                    in.wishY = mpInput.moveY;
                    in.camForwardX = camera.front.x;
                    in.camForwardY = camera.front.y;
                    in.camForwardZ = camera.front.z;
                    in.yaw = camera.yaw;
                    in.clientPx = player.pos.x;
                    in.clientPy = player.pos.y;
                    in.clientPz = player.pos.z;
                    in.clientVx = player.vel.x;
                    in.clientVy = player.vel.y;
                    in.clientVz = player.vel.z;
                    in.equippedSlot = (int16_t)player.equippedSlot;
                    in.weaponState =
                        (weapons.isShooting() ? 1u : 0u) |
                        (weapons.isReloading(player) ? 2u : 0u);
                    in.clientPingMs = mpContext.localPingMs;
                    in.jumpHeld = mpInput.jump ? 1 : 0;
                    in.dashPressed = mpInput.dashPressed ? 1 : 0;
                    in.attackPressed = 0;
                    in.freezeHeld = mpInput.freezeHeld ? 1 : 0;
                    MimitaNet::mpSendPacket(mpContext, &in, sizeof(in));
                }

                // TAB player list
                mpContext.showPlayerList = glfwGetKey(engine.window(), GLFW_KEY_TAB) == GLFW_PRESS;
                // F3 debug overlay
                static bool f3Prev = false;
                bool f3Down = glfwGetKey(engine.window(), GLFW_KEY_F3) == GLFW_PRESS;
                if (f3Down && !f3Prev)
                    mpContext.showDebugOverlay = !mpContext.showDebugOverlay;
                f3Prev = f3Down;
            }

            if (!Terminal::instance().isOpen())
                applyDebugMovement(player, engine.window(), camera, dt);

            camera.decayPunch(dt);
            camera.updateVectors();
            const bool replayFreecam =
                replayPlaybackActive &&
                gReplayPlayer.cameraController().mode() ==
                    ReplayCameraMode::Freecam;
            // Camera ownership: only ONE system may modify camera per frame.
            // Freecam (normal or replay) takes priority; replay controller runs only
            // when freecam is inactive.
            const bool anyFreecam = (freecamEnabled || replayFreecam) &&
                                    !Terminal::instance().isOpen();
            if (replayPlaybackActive && !anyFreecam) {
                if (const ReplaySceneFrame* replayFrame =
                        gReplayPlayer.currentSceneFrame()) {
                    gReplayPlayer.cameraController().update(
                        camera, *replayFrame,
                        gReplayPlayer.killerId(),
                        gReplayPlayer.victimId(), dt);
                }
            }
            if (anyFreecam) {
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
            } else if (replayPlaybackActive) {
                // ReplayCameraController owns the camera (already applied above).
            } else if (gDuelManager.phase() == DuelPhase::MatchEnd) {
                camera.follow(gDuelManager.winnerCameraTarget());
                camera.smoothCollision(gDuelManager.winnerCameraTarget(), world.collisionMesh.triangles, dt);
            } else {
                camera.follow(player.pos);
                camera.smoothCollision(player.pos, world.collisionMesh.triangles, dt);
            }
            setAudioListener(camera.pos, camera.front);
            EffectPartSystem::instance().setWorld(world);
            if (replayPlaybackActive) {
                for (const ReplayEffectEvent& effect :
                     gReplayPlayer.takeTriggeredEffects()) {
                    if (effect.type == "chat") {
                        ActorChatState& chatState = gReplayChatStates[effect.sourceActorId];
                        addChatMessage(chatState, effect.assetId, effect.sourceActorId);
                        playChatSound((int)effect.assetId.size());
                    } else if (effect.type == "gunshot") {
                        EffectPartSystem::instance().spawnMuzzleFlash(
                            effect.from, effect.sourceActorId);
                        EffectPartSystem::instance().spawnTracer(
                            effect.from, effect.to, effect.sourceActorId);
                    } else if (effect.type == "blood_spurt_emitter") {
                        EffectPartSystem::instance().spawnBloodEffect(
                            effect.position, effect.direction, 50.0f,
                            effect.sourceActorId, effect.targetActorId);
                        // Only show hitmarker when the viewed actor (killer) lands a hit
                        if (effect.sourceActorId == gReplayPlayer.killerId())
                            hitmarker();
                    } else if (effect.type == "dash") {
                        EffectPartSystem::instance().spawnDash(effect.position);
                    } else if (effect.type == "footstep") {
                        EffectPartSystem::instance().spawnFootstep(effect.position);
                    } else if (effect.type == "impact_world") {
                        EffectPartSystem::instance().spawnWorldImpact(
                            effect.position, effect.normal);
                        EffectPartSystem::instance().spawnWorldDebris(
                            effect.position, effect.normal, 1.0f);
                    } else if (effect.type == "debris_block") {
                        EffectPartSystem::instance().spawnWorldDebris(
                            effect.position, effect.normal, 1.5f);
                    } else if (effect.type == "impact_entity") {
                        EffectPartSystem::instance().spawnEntityImpact(
                            effect.position, effect.normal,
                            effect.sourceActorId, effect.targetActorId);
                    } else if (effect.type == "muzzle_flash") {
                        EffectPartSystem::instance().spawnMuzzleFlash(
                            effect.position, effect.sourceActorId);
                    } else if (effect.type == "tracer") {
                        EffectPartSystem::instance().spawnTracer(
                            effect.from, effect.to, effect.sourceActorId);
                    } else if (!effect.type.empty() &&
                               effect.type != "corpse_spawn") {
                        EffectPartSystem::instance().spawnCustom(
                            effect.position, glm::vec3(effect.color),
                            std::max(effect.lifetime, 0.1f),
                            effect.type.c_str());
                    }
                }
                for (const ReplaySoundEvent& sound :
                     gReplayPlayer.takeTriggeredSounds()) {
                    playWorldSound(
                        sound.soundPath, sound.position,
                        sound.volume, sound.pitch,
                        sound.maxDistance > 0.0f ? sound.maxDistance : 40.0f);
                }
            }
            if (!replayPlaybackActive)
                weapons.update(camera, player, npcSystem, dt);
            if (mpContext.active)
            {
                const std::vector<RevolverShotResult> godballHits =
                    weapons.collectRemoteGodballHits(
                        player, mpContext.remotePlayers, dt);
                for (const RevolverShotResult& hit : godballHits)
                {
                    const glm::vec3 direction =
                        glm::length(hit.end - hit.start) > 0.001f
                        ? glm::normalize(hit.end - hit.start)
                        : glm::vec3(0.0f, 1.0f, 0.0f);
                    MimitaNet::mpSendShotEvent(
                        mpContext, hit.targetId, (int)hit.damage, hit.damage,
                        MimitaNet::SHOT_EFFECT_ENTITY_IMPACT |
                            MimitaNet::SHOT_EFFECT_BLOOD |
                            MimitaNet::SHOT_EFFECT_HIT_SOUND,
                        MimitaNet::NETWORK_WEAPON_GODBALL,
                        MimitaNet::SHOT_IMPACT_ENTITY,
                        hit.start, hit.end, direction, -direction,
                        hit.knockbackImpulse);
                }
            }

            if (!replayPlaybackActive) {
                gDuelManager.update(
                    dt,
                    player,
                    npcSystem,
                    world,
                    camera);
                player.updateAudio(dt);

                // After result screen timer expires, start the final kill replay
                if (gDuelManager.phase() == DuelPhase::MatchEnd &&
                    gDuelManager.finalKillReplayLoaded &&
                    !gDuelManager.finalKillReplayActive &&
                    gDuelManager.matchOverRemaining() <= 0.0f)
                {
                    Debug::log(Debug::Category::Duel, "[DUEL] result screen complete");
                    Debug::log(Debug::Category::Duel, "[DUEL] starting final kill replay");
                    Debug::log(Debug::Category::Replay, "[REPLAY] playback begin");
                    gReplayPlayer.beginPlayback();
                    gDuelManager.finalKillReplayActive = true;
                    gDuelManager.finalKillReplayTime = 0.0f;
                    Debug::log(Debug::Category::Replay,
                        "[REPLAY] playback success=1");
                }
            }

            // Update effect parts
            EffectPartSystem::instance().update(dt);

            updateChatBubbles(player.chatState, dt);
            for (auto& kv : mpContext.remotePlayers)
                updateChatBubbles(kv.second.chatState, dt);
            for (auto& kv : gReplayChatStates)
                updateChatBubbles(kv.second, dt);

            static bool mousePrev = false;
            bool mouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            bool duelEndVisible = gDuelManager.phase() == DuelPhase::MatchEnd;
            if (duelEndVisible && mouseDown && !mousePrev) {
                Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] mouseClick=1 owner=duel_end_ui consumed=1");
                Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] weaponInputBlocked=1 reason=duel_end_ui_visible");
            }
            if (!replayPlaybackActive && !duelEndVisible &&
                !Terminal::instance().isOpen() && mouseDown) {
                const WeaponDefinition* curDef = weapons.getCurrentDef(player);
                bool isAuto = curDef && curDef->fireMode == WeaponFireMode::Automatic;
                bool shouldFire = isAuto || (!isAuto && mouseDown && !mousePrev);
                if (shouldFire) {
                    if (editorMode) {
                        selectedEditorObject = selectWorldTriangle(world, camera.pos, camera.front);
                        Terminal::instance().addLog(selectedEditorObject >= 0
                            ? "[EDITOR] selected triangle id " + std::to_string(selectedEditorObject)
                            : "[EDITOR] no object selected");
                    } else {
                        Terminal::instance().execute("shoot");
                    }
                }
            }
            mousePrev = mouseDown;

            static bool rightMousePrev = false;
            bool rightMouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (!replayPlaybackActive &&
                !Terminal::instance().isOpen() && rightMouseDown && !rightMousePrev) {
                if (!editorMode) {
                    weapons.fireAlt(camera, player, npcSystem, world);
                }
            }
            rightMousePrev = rightMouseDown;

            static bool slotPrev[10] = {};
            for (int keySlot = 0; keySlot <= 9; ++keySlot) {
                int key = keySlot == 0 ? GLFW_KEY_0 : GLFW_KEY_0 + keySlot;
                bool down = glfwGetKey(engine.window(), key) == GLFW_PRESS;
                if (!replayPlaybackActive &&
                    !Terminal::instance().isOpen() && down && !slotPrev[keySlot])
                    Terminal::instance().execute("equipslot" + std::to_string(keySlot));
                slotPrev[keySlot] = down;
            }
            // Process command keybinds (rising edge)
            {
                static std::unordered_map<int, bool> bindPrev;
                for (const auto& pair : G_COMMAND_BINDS) {
                    bool down = glfwGetKey(engine.window(), pair.first) == GLFW_PRESS;
                    if (down && !bindPrev[pair.first] && !Terminal::instance().isOpen())
                        Terminal::instance().execute(pair.second);
                    bindPrev[pair.first] = down;
                }
            }
            // Replay playback keyboard shortcuts (only while replay is active)
            if (replayPlaybackActive && !Terminal::instance().isOpen()) {
                static bool spacePrev = false;
                bool spaceDown = glfwGetKey(engine.window(), GLFW_KEY_SPACE) == GLFW_PRESS;
                if (spaceDown && !spacePrev) {
                    if (gReplayPlayer.isPaused()) gReplayPlayer.resume();
                    else gReplayPlayer.pause();
                }
                spacePrev = spaceDown;

                static bool leftPrev = false;
                bool leftDown = glfwGetKey(engine.window(), GLFW_KEY_LEFT) == GLFW_PRESS;
                if (leftDown && !leftPrev) {
                    uint32_t t = gReplayPlayer.currentTick();
                    uint32_t seekTo = t > 300 ? t - 300 : 0;
                    gReplayPlayer.seekToTick(seekTo);
                }
                leftPrev = leftDown;

                static bool rightPrev = false;
                bool rightDown = glfwGetKey(engine.window(), GLFW_KEY_RIGHT) == GLFW_PRESS;
                if (rightDown && !rightPrev) {
                    uint32_t t = gReplayPlayer.currentTick();
                    uint32_t total = gReplayPlayer.totalTicks();
                    gReplayPlayer.seekToTick(std::min(t + 300, total));
                }
                rightPrev = rightDown;

                static bool lPrev = false;
                bool lDown = glfwGetKey(engine.window(), GLFW_KEY_L) == GLFW_PRESS;
                if (lDown && !lPrev) {
                    gReplayCinematicMode = !gReplayCinematicMode;
                    printf("[CINEMATIC] %s\n", gReplayCinematicMode ? "Enabled" : "Disabled");
                    Terminal::instance().addLog(std::string("[CINEMATIC] ") + (gReplayCinematicMode ? "Enabled" : "Disabled"));
                }
                lPrev = lDown;
            }
            renderWorld(world, camera);
            if (replayPlaybackActive) {
                if (const ReplaySceneFrame* replayFrame =
                        gReplayPlayer.currentSceneFrame()) {
                    const glm::mat4 replayView = camera.getView();
                    const glm::mat4 replayProj = camera.getProj(
                        (float)engine.renderer->width,
                        (float)engine.renderer->height);
                    for (const ReplayActorState& actorState :
                         replayFrame->actors) {
                        std::unique_ptr<Player>& actor =
                            replayActorModels[actorState.id];
                        if (!actor) {
                            actor = std::make_unique<Player>();
                            // Restore per-actor outfit
                            const std::string& outfitToUse =
                                !actorState.outfitPath.empty()
                                    ? actorState.outfitPath
                                    : gReplayPlayer.outfitPath();
                            if (!outfitToUse.empty())
                                OutfitAtlas::instance().apply(*actor, outfitToUse);
                        }
                        actor->username = actorState.name;
                        actor->currentHp = actorState.health;
                        actor->maxHp = actorState.maxHealth;
                        actor->dead = actorState.dead;
                        actor->vel = actorState.velocity;
                        actor->onGround = actorState.grounded;
                        actor->equippedWeaponId = actorState.weaponName;
                        actor->applyReplayPose(
                            actorState.position,
                            actorState.rotation.z,
                            actorState.bodyParts);

                        const bool hideFirstPersonActor =
                            gReplayPlayer.cameraController().mode() ==
                                ReplayCameraMode::FirstPerson &&
                            actorState.id == gReplayPlayer.killerId();
                        if (!hideFirstPersonActor) {
                            actor->renderCurrentPose(
                                engine.renderer->shaderProgram,
                                replayView, replayProj);
                        }

                        const WeaponDefinition* definition =
                            WeaponRegistry::instance().get(
                                actorState.weaponName);
                        if (definition && !definition->modelPath.empty()) {
                            actor->equippedSlot = definition->slot;
                            const std::string weaponKey =
                                actorState.id + ":" + definition->id;
                            WeaponViewModel& viewModel =
                                replayWeaponModels[weaponKey];
                            viewModel.update(
                                camera, *actor, dt, definition, false);
                            viewModel.render(
                                camera, *actor, definition->slot);
                        }
                    }
                }
            } else {
                renderPlayer(player, camera);
                if (mpContext.active) {
                    for (auto& kv : mpContext.remotePlayers)
                        renderNetworkPlayer(kv.second, camera, kv.first, false);
                }
                npcSystem.render(camera);
            }
            {   static float rlogTimer = 0.0f; rlogTimer -= dt;
                if (rlogTimer <= 0.0f && replayPlaybackActive) {
                    rlogTimer = 1.0f;
                    const auto* rframe = gReplayPlayer.currentSceneFrame();
                    printf("[REPLAY] cameraMode=%s freecam=%d paused=%d "
                           "viewingActor=%s tick=%u actorCount=%zu effectCount=%zu\n",
                           gReplayPlayer.cameraController().modeName(),
                           (int)(gReplayPlayer.cameraController().mode() == ReplayCameraMode::Freecam),
                           (int)gReplayPlayer.isPaused(),
                           rframe && !rframe->actors.empty() ? rframe->actors[0].name.c_str() : "none",
                           gReplayPlayer.currentTick(),
                           rframe ? rframe->actors.size() : 0,
                           gReplayPlayer.totalEffectCount());
                }
            }
            if (mpContext.active && !replayPlaybackActive)
            {
                static uint64_t lastReplicaRenderLogMs = 0;
                const uint64_t renderLogNow = MimitaNet::nowMs();
                if (renderLogNow - lastReplicaRenderLogMs >= 1000)
                {
                    for (const auto& kv : mpContext.remotePlayers)
                        printf("[CLIENT RENDER ENTITY] entityId=%u type=Player visible=%d mesh=%s "
                               "position=(%.2f,%.2f,%.2f)\n",
                               kv.first, (int)!kv.second.dead,
                               kv.second.modelLoaded ? "player-glb" : "fallback-capsule",
                               kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
                    for (const auto& kv : mpContext.remoteNpcs)
                        printf("[CLIENT RENDER ENTITY] entityId=%u type=NPC visible=%d mesh=%s "
                               "position=(%.2f,%.2f,%.2f)\n",
                               kv.first, (int)!kv.second.dead,
                               kv.second.modelLoaded ? "player-glb" : "fallback-capsule",
                               kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
                    lastReplicaRenderLogMs = renderLogNow;
                }
            }
            if (!replayPlaybackActive) {
                DeathSystem::instance().render(camera);
                weapons.render(camera, player);
            }
            
            // Render effect parts (world-space visualizations)
            EffectPartSystem::instance().render(camera);
            DebugVis::flushTris(camera);
            
            worldPassRan = true;

            npcSystem.drawDebug(camera);
            drawDebugStuff(player, camera, world);

            if (mpContext.active && mpContext.showDebugOverlay)
            {
                const glm::vec4 predictedColor{0.1f, 1.0f, 0.25f, 1.0f};
                const glm::vec4 serverColor{1.0f, 0.15f, 0.1f, 1.0f};
                const glm::vec4 remoteColor{0.15f, 0.55f, 1.0f, 1.0f};
                const glm::vec4 npcColor{0.1f, 1.0f, 0.9f, 1.0f};
                const auto drawReplicaCapsule =
                    [&camera](const Player& replica, const glm::vec4& color)
                    {
                        const Capsule capsule = replica.getCapsule();
                        const float giantRadius = capsule.r * 1.8f;
                        DebugVis::drawDiagnosticWireSphere(
                            camera, capsule.a, giantRadius, color);
                        DebugVis::drawDiagnosticWireSphere(
                            camera, capsule.b, giantRadius, color);
                        DebugVis::drawDiagnosticLine(
                            camera, capsule.a, capsule.b, color);
                    };

                DebugVis::drawWireSphere(camera, player.pos, 0.72f, predictedColor);
                DebugVis::drawWorldLabel(
                    player.pos + glm::vec3(0.0f, 0.0f, 2.0f),
                    "LOCAL PREDICTED", predictedColor);
                if (mpContext.hasLocalServerPosition)
                {
                    DebugVis::drawWireSphere(
                        camera, mpContext.localServerPosition, 0.78f, serverColor);
                    DebugVis::drawLine(
                        camera, player.pos, mpContext.localServerPosition, serverColor);
                    char serverLabel[128];
                    snprintf(serverLabel, sizeof(serverLabel),
                             "SERVER id=%u error=%.2fm",
                             mpContext.localPlayerId,
                             glm::length(player.pos - mpContext.localServerPosition));
                    DebugVis::drawWorldLabel(
                        mpContext.localServerPosition + glm::vec3(0.0f, 0.0f, 2.3f),
                        serverLabel, serverColor);
                }

                for (const auto& kv : mpContext.remotePlayers)
                {
                    drawReplicaCapsule(kv.second, remoteColor);
                    bool usedHeadTransform = false;
                    const glm::vec3 healthbarAnchor =
                        playerHealthbarAnchor(
                            kv.second, &usedHeadTransform);
                    DebugVis::drawDiagnosticWireSphere(
                        camera, healthbarAnchor, 0.16f,
                        usedHeadTransform
                            ? glm::vec4(1.0f, 0.75f, 0.1f, 1.0f)
                            : glm::vec4(1.0f, 0.2f, 0.8f, 1.0f));
                    const auto interpolation = mpContext.remotePlayerInterpolation.find(kv.first);
                    if (interpolation != mpContext.remotePlayerInterpolation.end() &&
                        interpolation->second.hasTarget)
                    {
                        DebugVis::drawDiagnosticWireSphere(
                            camera, interpolation->second.target.position, 0.36f, serverColor);
                        DebugVis::drawDiagnosticLine(
                            camera, kv.second.pos, interpolation->second.target.position, serverColor);
                    }
                    char label[128];
                    snprintf(
                        label, sizeof(label),
                        "REMOTE PLAYER id=%u HP=%d anchor=%s interp=100ms",
                        kv.first, kv.second.currentHp,
                        usedHeadTransform ? "head" : "fallback");
                    DebugVis::drawDiagnosticWorldLabel(
                        healthbarAnchor + glm::vec3(0.0f, 0.0f, 0.25f),
                        label, remoteColor);
                }
                for (const auto& kv : mpContext.remoteNpcs)
                {
                    drawReplicaCapsule(kv.second, npcColor);
                    const auto interpolation = mpContext.remoteNpcInterpolation.find(kv.first);
                    if (interpolation != mpContext.remoteNpcInterpolation.end() &&
                        interpolation->second.hasTarget)
                    {
                        DebugVis::drawDiagnosticWireSphere(
                            camera, interpolation->second.target.position, 0.36f, serverColor);
                        DebugVis::drawDiagnosticLine(
                            camera, kv.second.pos, interpolation->second.target.position, serverColor);
                    }
                    char label[128];
                    snprintf(label, sizeof(label), "NPC id=%u HP=%d interp=100ms",
                             kv.first, kv.second.currentHp);
                    DebugVis::drawDiagnosticWorldLabel(
                        kv.second.pos + glm::vec3(0.0f, 0.0f, 2.0f),
                        label, npcColor);
                }
            }

            // Draw NPC selection debug visuals
            if (DebugVis::enabled()) {
                NpcSelectionManager::instance().drawSelection(npcSystem, camera);
            }

            uiBeginFrame(engine.window(), "game-debug-overlay");
            drawHitmarker(dt);
            if (mpContext.active && !mpContext.connected)
            {
                const float boxW = 360.0f;
                const float boxX = (uiScreenW() - boxW) * 0.5f;
                uiDrawRect({boxX, 32.0f, boxW, 58.0f},
                           {0.02f, 0.025f, 0.035f, 0.92f}, "connection-status");
                uiDrawText(mpContext.connectionStatus.c_str(), boxX + 18.0f, 54.0f,
                           0.4f,
                           mpContext.connectFailed
                               ? glm::vec4(1.0f, 0.25f, 0.2f, 1.0f)
                               : glm::vec4(0.3f, 0.75f, 1.0f, 1.0f));
            }
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
            if (replayPlaybackActive && !gReplayCinematicMode) {
                const float rOverlayX = uiScreenW() - 280.0f;
                const float rOverlayY = 20.0f;
                const auto* rFrame = gReplayPlayer.currentSceneFrame();
                const uint32_t totalTicks = gReplayPlayer.totalTicks();
                const float currentTime = gReplayPlayer.currentTick() / 60.0f;
                const float totalTime = (float)totalTicks / 60.0f;
                const char* camMode = gReplayPlayer.cameraController().modeName();
                const bool paused = gReplayPlayer.isPaused();

                const float labelX = rOverlayX + 12.0f;
                const float valueX = rOverlayX + 100.0f;
                const float lineH = 20.0f;
                const float bgW = 250.0f;
                float bgH = lineH * 5.0f + 16.0f;

                uiDrawRect({rOverlayX, rOverlayY, bgW, bgH},
                           {0.0f, 0.0f, 0.0f, 0.70f}, "replay-hud-bg");

                float y = rOverlayY + 8.0f;
                uiDrawText("Viewing:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                if (rFrame && !rFrame->actors.empty())
                    uiDrawText(rFrame->actors[0].name.c_str(), valueX, y, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});
                y += lineH;

                uiDrawText("Camera:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                {
                    char camBuf[64];
                    snprintf(camBuf, sizeof(camBuf), "%s%s", camMode,
                             paused ? " [PAUSED]" : "");
                    uiDrawText(camBuf, valueX, y, 0.28f,
                               paused ? glm::vec4{1.0f, 0.9f, 0.3f, 1.0f}
                                      : glm::vec4{0.7f, 0.85f, 1.0f, 1.0f});
                }
                y += lineH;

                uiDrawText("Tick:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                {
                    char tickBuf[64];
                    snprintf(tickBuf, sizeof(tickBuf), "%u / %u",
                             (unsigned)gReplayPlayer.currentTick(), (unsigned)totalTicks);
                    uiDrawText(tickBuf, valueX, y, 0.28f, {0.9f, 0.9f, 0.3f, 1.0f});
                }
                y += lineH;

                uiDrawText("Time:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                {
                    auto formatTime = [](float seconds) -> std::string {
                        int totalSec = (int)seconds;
                        int mins = totalSec / 60;
                        int secs = totalSec % 60;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                        return std::string(buf);
                    };
                    std::string curStr = formatTime(currentTime);
                    std::string totStr = formatTime(totalTime);
                    char timeBuf[64];
                    snprintf(timeBuf, sizeof(timeBuf), "%s / %s", curStr.c_str(), totStr.c_str());
                    uiDrawText(timeBuf, valueX, y, 0.28f, {0.9f, 0.9f, 0.3f, 1.0f});
                }
                y += lineH;

                if (totalTicks > 0) {
                    const float barX = rOverlayX + 8.0f;
                    const float barY = y + 4.0f;
                    const float barW = bgW - 16.0f;
                    const float barH = 6.0f;
                    uiDrawRect({barX, barY, barW, barH}, {0.3f, 0.3f, 0.3f, 0.7f}, "seek-bg");
                    float progress = std::clamp((float)gReplayPlayer.currentTick() / (float)std::max(totalTicks, 1u), 0.0f, 1.0f);
                    uiDrawRect({barX, barY, barW * progress, barH}, {0.9f, 0.9f, 0.3f, 0.9f}, "seek-fill");
                }

                // Crosshair for the first armed actor
                if (rFrame && !rFrame->actors.empty()) {
                    const ReplayActorState& primary = rFrame->actors[0];
                    if (!primary.weaponName.empty() && primary.weaponName != "none") {
                        const char* crosshairPath = "assets/crosshair/crosshairready.png";
                        if (primary.reloading)
                            crosshairPath = "assets/crosshair/crosshairreloading.png";
                        else if (primary.shooting)
                            crosshairPath = "assets/crosshair/crosshairdelay.png";
                        float cs = 100.0f;
                        const WeaponDefinition* rdef = WeaponRegistry::instance().get(primary.weaponName);
                        if (rdef) cs = rdef->crosshairSize;
                        uiDrawImage(crosshairPath,
                                    {uiScreenW() * 0.5f - cs * 0.5f,
                                     uiScreenH() * 0.5f - cs * 0.5f, cs, cs});
                        char ammoLine[48];
                        snprintf(ammoLine, sizeof(ammoLine), "%d / %d",
                                 primary.currentAmmo, primary.reserveAmmo);
                        const float ammoW = uiMeasureText(ammoLine, 0.34f);
                        uiDrawText(ammoLine, uiScreenW() * 0.5f - ammoW * 0.5f,
                                   uiScreenH() * 0.5f - cs * 1.1f,
                                   0.34f, {1.0f, 0.82f, 0.3f, 1.0f});
                    }
                }
                // Healthbars for all replay actors
                for (const auto& kv : replayActorModels) {
                    if (!kv.second || kv.second->dead) continue;
                    drawPlayerHealthbar(*kv.second, camera, "replay-hp");
                }

                // Replay controls help panel (bottom right)
                const float helpX = uiScreenW() - 220.0f;
                const float helpY = uiScreenH() - 140.0f;
                const float helpW = 200.0f;
                const float helpH = 110.0f;
                uiDrawRect({helpX, helpY, helpW, helpH},
                           {0.0f, 0.0f, 0.0f, 0.65f}, "replay-help-bg");
                float hy = helpY + 6.0f;
                uiDrawText("REPLAY CONTROLS", helpX + 8.0f, hy, 0.28f,
                           {0.9f, 0.9f, 0.3f, 1.0f}); hy += 18.0f;
                uiDrawText("SPACE    Pause/Resume", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("<-       Back 5s", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("->       Forward 5s", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("L        Cinematic", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("freecam  Free Camera", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f});
            } else {
                const WeaponDefinition* crosshairDef = weapons.getCurrentDef(player);
                if (crosshairDef && !crosshairDef->crosshairId.empty()) {
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
                    float size = crosshairDef->crosshairSize;
                    uiDrawImage(crosshairPath,
                                {uiScreenW() * 0.5f - size * 0.5f, uiScreenH() * 0.5f - size * 0.5f, size, size});
                }
            }
            uiDrawRect({14, 78, 260, 92}, {0.0f, 0.0f, 0.0f, 0.56f}, "hud-bg");
            uiDrawText(player.username.c_str(), 24, 88, 0.42f, {0.95f, 0.98f, 1.0f, 1.0f});
            char hpText[64];
            snprintf(hpText, sizeof(hpText), "HP: %d/%d", player.currentHp, player.maxHp);
            uiDrawText(hpText, 24, 116, 0.38f, {0.35f, 1.0f, 0.45f, 1.0f});
            if (player.dead && gDuelManager.phase() != DuelPhase::MatchEnd) {
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
                // Multiplayer HUD
                if (mpContext.active) {
                    char mpText[128];
                    snprintf(mpText, sizeof(mpText), "MP id=%u players=%zu server=%s",
                             mpContext.localPlayerId,
                             mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1 : 0),
                             mpContext.serverAddress.c_str());
                    uiDrawText(mpText, 24, 232, 0.32f, {0.7f, 0.9f, 1.0f, 1.0f});
                }
                {
                    const WeaponDefinition* curDef = nullptr;
                    for (const auto& pair : WeaponRegistry::instance().all()) {
                        if (pair.second.slot == player.equippedSlot) {
                            curDef = &pair.second;
                            break;
                        }
                    }
                    if (curDef) {
                        auto it = player.weaponRuntimes.find(curDef->id);
                        if (it != player.weaponRuntimes.end()) {
                            const WeaponRuntime& rt = it->second;
                            char ammoText[96];
                            int displayReserve = std::max(0, rt.reserveAmmo);
                            snprintf(ammoText, sizeof(ammoText), "%s: %d / %d",
                                     curDef->displayName.c_str(),
                                     rt.currentAmmo, displayReserve);
                            uiDrawText(ammoText, 24, 232, 0.42f, {1.0f, 0.82f, 0.3f, 1.0f});

                            if (rt.isReloading) {
                                char reloadText[96];
                                snprintf(reloadText, sizeof(reloadText),
                                         "no bullets! reloading... %.2f",
                                         std::max(0.0f, rt.reloadTimer));
                                uiDrawText(reloadText, 24, 248, 0.36f, {1.0f, 0.5f, 0.2f, 1.0f});
                            }
                        }
                    }
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
                    // Show weapon name for this slot
                    const WeaponDefinition* slotDef = nullptr;
                    for (const auto& pair : WeaponRegistry::instance().all()) {
                        if (pair.second.slot == slot) {
                            slotDef = &pair.second;
                            break;
                        }
                    }
                    if (slotDef) {
                        std::string shortName = slotDef->id.substr(0, 3);
                        std::transform(shortName.begin(), shortName.end(), shortName.begin(), ::toupper);
                        uiDrawText(shortName.c_str(), rect.x + 13, rect.y + 34, 0.20f,
                                   equipped ? glm::vec4(1,0.85f,0.35f,1) : glm::vec4(0.55f,0.55f,0.58f,1));
                    } else {
                        uiDrawText("-", rect.x + 13, rect.y + 34, 0.20f, glm::vec4(0.55f,0.55f,0.58f,1));
                    }
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
            for (const Npc& npc : npcSystem.all())
                drawPlayerHealthbar(npc.body, camera, "npc-hp");

            renderChatBubbles(player.chatState, player, camera);
            if (!replayPlaybackActive)
            {
                for (auto& kv : mpContext.remotePlayers)
                    renderChatBubbles(kv.second.chatState, kv.second, camera);
            }
            else
            {
                for (const auto& kv : gReplayChatStates)
                {
                    auto actorIt = replayActorModels.find(kv.first);
                    if (actorIt != replayActorModels.end() && actorIt->second)
                        renderChatBubbles(kv.second, *actorIt->second, camera);
                }
            }

            if (mpContext.active)
            {
                static uint64_t lastHealthbarLogMs = 0;
                const uint64_t healthbarNowMs = MimitaNet::nowMs();
                const bool logHealthbars =
                    mpContext.showDebugOverlay &&
                    healthbarNowMs - lastHealthbarLogMs >= 1000;

                for (const auto& kv : mpContext.remotePlayers)
                {
                    const HealthbarRenderResult result =
                        drawPlayerHealthbar(
                            kv.second, camera, "network-player-hp");
                    if (logHealthbars)
                    {
                        printf(
                            "[NET HEALTHBAR] entityId=%u owner=remote "
                            "health=%d/%d anchor=%s "
                            "world=(%.2f %.2f %.2f) screen=(%.1f %.1f) "
                            "distance=%.1f rendered=%d cull=%s\n",
                            kv.first,
                            kv.second.currentHp, kv.second.maxHp,
                            result.usedHeadTransform ? "head" : "fallback",
                            result.anchor.x, result.anchor.y, result.anchor.z,
                            result.screen.x, result.screen.y,
                            result.distance, (int)result.rendered,
                            healthbarCullReasonName(result.cullReason));
                    }
                }

                if (logHealthbars)
                    lastHealthbarLogMs = healthbarNowMs;
            }
            {
                static float hpLogTimer = 0.0f; hpLogTimer -= 0.016f;
                if (hpLogTimer <= 0.0f && !npcSystem.all().empty()) {
                    hpLogTimer = 1.0f;
                    printf("[PLAYER HP FRAME] hp=%d/%d pos=(%.1f %.1f %.1f)\n",
                           player.currentHp, player.maxHp,
                           player.pos.x, player.pos.y, player.pos.z);
                }
            }
            {
                char npcText[96];
                snprintf(npcText, sizeof(npcText), "NPCs: %zu", npcSystem.all().size());
                uiDrawText(npcText, 24, 168, 0.32f, {1.0f, 0.82f, 0.38f, 1.0f});
                if (!npcSystem.all().empty()) {
                    const Npc& first = npcSystem.all().front();
                    char tuneText[256];
                    snprintf(tuneText, sizeof(tuneText),
                             "  Diff=%.0f aimErr=%.1fdeg reaction=%.2fs moveVar=%.2f",
                             first.difficulty,
                             NpcCombat::aimErrorDegrees(first.difficulty),
                             first.tuning.reactionDelay,
                             first.tuning.movementPrecision);
                    uiDrawText(tuneText, 24, 184, 0.28f, {0.8f, 0.9f, 1.0f, 1.0f});
                }
            }
            if (DebugVis::render())
            {
                char dbg[256];
                snprintf(dbg, sizeof(dbg), "dt %.3f grounded %d vel %.2f %.2f %.2f cam %.1f %.1f %.1f",
                         dt, (int)player.onGround, player.vel.x, player.vel.y, player.vel.z,
                         camera.pos.x, camera.pos.y, camera.pos.z);
                uiDrawText(dbg, 24, 184, 0.30f, {1.0f, 0.9f, 0.45f, 1.0f});
            }
            // Final kill replay: slow motion and overlay
            if (gDuelManager.finalKillReplayActive) {
                gDuelManager.finalKillReplayTime += dt;
                // Slow motion curve: 0.15x for 2s, ramp to 1x over 1.5s
                float t = gDuelManager.finalKillReplayTime;
                if (t < 2.0f) {
                    gDuelManager.finalKillSlowMoFactor = 0.15f;
                } else if (t < 3.5f) {
                    float p = (t - 2.0f) / 1.5f;
                    gDuelManager.finalKillSlowMoFactor = 0.15f + p * 0.85f;
                } else {
                    gDuelManager.finalKillSlowMoFactor = 1.0f;
                }
                // Apply slow motion to replay playback
                gReplayPlayer.setTimescale(gDuelManager.finalKillSlowMoFactor);

                // Draw the final kill overlay on top of match end screen
                float fkW = (float)uiScreenW(), fkH = (float)uiScreenH();
                uiDrawRect({0, fkH * 0.3f, fkW, 80.0f}, {0.0f, 0.0f, 0.0f, 0.6f}, "fk-bg");
                uiDrawText("FINAL KILL", fkW * 0.5f - 160.0f, fkH * 0.3f + 8.0f,
                           1.2f, {1.0f, 0.9f, 0.1f, 1.0f});
                uiDrawText("MATCH WINNING ELIMINATION", fkW * 0.5f - 220.0f, fkH * 0.3f + 50.0f,
                           0.6f, {1.0f, 1.0f, 1.0f, 0.9f});

                // Save Replay button
                GuiLayout& duelLayout = GuiLayoutManager::instance().getLayout("config/gui/duel-match-end.json");
                UIRect saveBtn = duelLayout.getRect("SAVE REPLAY!", {fkW * 0.5f - 120.0f, fkH * 0.75f, 240.0f, 50.0f});
                if (uiButton(engine.window(), "SAVE REPLAY!", saveBtn,
                             {0.2f, 0.6f, 0.3f, 1.0f}).clicked)
                {
                    printf("[FINAL KILL] saveReplayClicked\n");
                    if (!gDuelManager.finalKillReplayPath.empty()) {
                        printf("[FINAL KILL] replay already saved: %s\n",
                               gDuelManager.finalKillReplayPath.c_str());
                    } else {
                        std::string path;
                        if (gReplayFactory.saveLastKill(&path) ||
                            gReplayClipSaver.saveLastKill(&path)) {
                            gDuelManager.finalKillReplayPath = path;
                            printf("[FINAL KILL] jsonSaved=%s\n", path.c_str());
                            // Launch ffmpeg in separate process
                            std::string mp4Path = path + ".mp4";
                            std::string cmd = "start cmd /c ffmpeg -i \"" + path +
                                "\" \"" + mp4Path + "\" 2>&1";
                            std::thread([cmd]() {
                                system(cmd.c_str());
                            }).detach();
                            printf("[FINAL KILL] ffmpegStarted=%s\n", mp4Path.c_str());
                        }
                    }
                }

                // Replay looping: after slow-motion phase, loop the kill replay in background
                if (t > 6.0f) {
                    // When replay reaches the end (playback stopped), loop back
                    if (!gReplayPlayer.isPlaying()) {
                        gReplayPlayer.beginPlayback();
                        Debug::log(Debug::Category::Replay,
                            "[REPLAY] final kill replay looping as background");
                    }
                    // 50% dim overlay when replay loops (buttons will be shown)
                    float fkW2 = (float)uiScreenW(), fkH2 = (float)uiScreenH();
                    uiDrawRect({0, 0, fkW2, fkH2}, {0.0f, 0.0f, 0.0f, 0.5f}, "fk-dim");
                    Debug::logThrottled(Debug::Category::Duel, "fk_loop", 3.0f,
                        "[DUEL] final kill replay looping as background");
                }
            }

            if (gDuelManager.phase() == DuelPhase::MatchEnd) {
                DuelMenuAction action = gDuelManager.renderMatchOverScreen(engine.window());
                if (action == DuelMenuAction::PlayAgain) {
                    Debug::log(Debug::Category::Duel, "[DUEL] restarting same settings");
                    gDuelManager.restartDuel(player, npcSystem, world);
                } else if (action == DuelMenuAction::ExitToMenu) {
                    Debug::log(Debug::Category::Duel, "[DUEL] exit to main menu requested");
                    Debug::log(Debug::Category::Duel, "[DUEL] duel UI cleared");
                    gDuelManager.stopDuel();
                    npcSystem.destroyAll();
                    gameState = GAME_MENU;
                    Debug::log(Debug::Category::Duel, "[MAIN MENU] entered");
                } else if (action == DuelMenuAction::SaveReplay) {
                    if (!gDuelManager.finalKillReplayPath.empty()) {
                        Debug::log(Debug::Category::Duel, "[DUEL] replay already saved: %s",
                                   gDuelManager.finalKillReplayPath.c_str());
                    } else {
                        std::string path;
                        if (gReplayFactory.saveLastKill(&path) ||
                            gReplayClipSaver.saveLastKill(&path)) {
                            gDuelManager.finalKillReplayPath = path;
                            Debug::log(Debug::Category::Duel, "[DUEL] replay saved: %s", path.c_str());
                            std::string mp4Path = path + ".mp4";
                            std::string cmd = "start cmd /c ffmpeg -i \"" + path +
                                "\" \"" + mp4Path + "\" 2>&1";
                            std::thread([cmd]() {
                                system(cmd.c_str());
                            }).detach();
                            Debug::log(Debug::Category::Duel, "[DUEL] ffmpeg started for: %s", mp4Path.c_str());
                        }
                    }
                }
            } else {
                gDuelManager.renderHud();
            }

            // TAB Player List overlay
            if (mpContext.active && mpContext.showPlayerList)
            {
                float listX = uiScreenW() * 0.5f - 160.0f;
                float listY = uiScreenH() * 0.25f;
                float listW = 320.0f;
                float lineH = 24.0f;
                float headerH = 30.0f;

                // Count all players (local + remote)
                size_t totalPlayers = mpContext.playerRegistry.size();
                float listH = headerH + (totalPlayers + 1) * lineH + 10.0f;

                uiDrawRect({listX, listY, listW, listH}, {0.0f, 0.0f, 0.0f, 0.85f}, "player-list-bg");
                uiDrawRectOutline({listX, listY, listW, listH}, {0.5f, 0.6f, 0.8f, 1.0f}, "player-list-border");

                float y = listY + 8.0f;
                uiDrawText("PLAYERS", listX + 10.0f, y, 0.36f, {0.8f, 0.9f, 1.0f, 1.0f});
                y += headerH;
                uiDrawText("ID   NAME                         PING",
                           listX + 10.0f, y, 0.28f, {0.65f, 0.75f, 0.9f, 1.0f});
                y += lineH;

                // Local player
                if (mpContext.localPlayerId)
                {
                    const char* localName = player.username.empty() ? "you" : player.username.c_str();
                    char localLine[128];
                    snprintf(localLine, sizeof(localLine), "%u   %s   %dms (you)",
                             mpContext.localPlayerId, localName, mpContext.localPingMs);
                    uiDrawText(localLine, listX + 10.0f, y, 0.32f, {0.3f, 1.0f, 0.4f, 1.0f});
                    y += lineH;
                }

                // Remote players
                for (const auto& kv : mpContext.playerRegistry)
                {
                    if (kv.first == mpContext.localPlayerId)
                        continue;
                    const char* pname = kv.second.name.c_str();
                    char remoteLine[128];
                    snprintf(remoteLine, sizeof(remoteLine), "%u  %s  %dms",
                             kv.first, pname, kv.second.pingMs);
                    uiDrawText(remoteLine, listX + 10.0f, y, 0.32f, {0.9f, 0.95f, 1.0f, 1.0f});
                    y += lineH;
                }
            }

            // F3 Networking Debug Overlay
            if (mpContext.active && mpContext.showDebugOverlay)
            {
                float dbgX = uiScreenW() - 360.0f;
                float dbgY = 20.0f;
                float lineH = 18.0f;
                float dbgW = 340.0f;
                float dbgH = (13.0f + (float)mpContext.remotePlayers.size()) * lineH + 10.0f;

                uiDrawRect({dbgX, dbgY, dbgW, dbgH}, {0.0f, 0.0f, 0.0f, 0.8f}, "net-debug-bg");

                float y = dbgY + 6.0f;
                char buf[256];

                snprintf(buf, sizeof(buf), "STATUS: %s", mpContext.connectionStatus.c_str());
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                           mpContext.connected ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f)
                                               : glm::vec4(1.0f, 0.55f, 0.2f, 1.0f));
                y += lineH;

                snprintf(buf, sizeof(buf), "LOCAL PLAYER ID: %u", mpContext.localPlayerId);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.3f, 1.0f, 0.4f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "PING: %dms  FAKELAG: mode %d delay %dms queue %zu",
                         mpContext.localPingMs,
                         mpContext.fakeLagMode,
                         mpContext.fakeLagMode == 1
                            ? mpContext.fakeLagCurrentMs
                            : mpContext.fakeLagStaticMs,
                         mpContext.outgoingQueue.size());
                uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.75f, 0.85f, 1.0f, 1.0f});
                y += lineH;

                snprintf(buf, sizeof(buf), "ENTITIES: %zu (PLAYERS %zu / NPCS %zu)",
                         mpContext.remotePlayers.size() + mpContext.remoteNpcs.size() +
                             (mpContext.localPlayerId ? 1u : 0u),
                         mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1u : 0u),
                         mpContext.remoteNpcs.size());
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "TICK CLIENT %u / SERVER %llu",
                         mpContext.tick, (unsigned long long)mpContext.lastSnapshotTick);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

                const uint64_t snapshotAge = mpContext.lastSnapshotReceivedMs
                    ? MimitaNet::nowMs() - mpContext.lastSnapshotReceivedMs
                    : 0;
                snprintf(buf, sizeof(buf), "SNAPSHOT AGE: %llums  INTERP: 100ms",
                         (unsigned long long)snapshotAge);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

                const uint64_t snapshotTotal =
                    mpContext.snapshotsReceived + mpContext.snapshotsMissed;
                const float lossPercent = snapshotTotal
                    ? 100.0f * (float)mpContext.snapshotsMissed / (float)snapshotTotal
                    : 0.0f;
                snprintf(buf, sizeof(buf), "SNAPSHOT LOSS: %.1f%% (%llu missed)",
                         lossPercent, (unsigned long long)mpContext.snapshotsMissed);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "PACKETS TX/RX: %llu / %llu",
                         (unsigned long long)mpContext.packetsSent,
                         (unsigned long long)mpContext.packetsReceived);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "SERVER: %s", mpContext.serverAddress.c_str());
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.75f, 0.85f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "LOCAL POS: %.1f %.1f %.1f HP=%d",
                         player.pos.x, player.pos.y, player.pos.z,
                         mpContext.localServerHealth);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                           {0.35f, 1.0f, 0.45f, 1.0f});
                y += lineH;

                if (mpContext.hasLocalServerPosition)
                {
                    snprintf(buf, sizeof(buf), "SERVER POS ERROR: %.2fm",
                             glm::length(player.pos - mpContext.localServerPosition));
                    uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                               {1.0f, 0.35f, 0.25f, 1.0f});
                }
                y += lineH;

                for (const auto& kv : mpContext.remotePlayers)
                {
                    const Player& rp = kv.second;
                    auto nameIt = mpContext.playerRegistry.find(kv.first);
                    const char* rname = (nameIt != mpContext.playerRegistry.end()) ? nameIt->second.name.c_str() : "?";
                    snprintf(buf, sizeof(buf), "  %s id=%u pos=(%.1f,%.1f,%.1f)",
                             rname, kv.first, rp.pos.x, rp.pos.y, rp.pos.z);
                    uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.6f, 0.85f, 1.0f, 1.0f});
                    y += lineH;
                }
            }

            // Replay Browser overlay (rendered on top of everything)
            gReplayBrowser.draw();

            // Replay Timeline during playback
            if (replayPlaybackActive) {
                if (const ReplaySceneFrame* rFrame = gReplayPlayer.currentSceneFrame()) {
                    gReplayTimeline.draw(gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
                }
            }

            uiRenderFrameDebugOverlay(engine.window(), "PLAYING", worldPassRan);
            uiEndFrame();

            // Dev overlay notifications (temporary)
            DevOverlay::instance().render();
        }

        // Advance GUI media animations (GIF frames, future video)
        uiUpdateMedia(dt);

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
                bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
                glfwSetInputMode(engine.window(), GLFW_CURSOR,
                    gameState == GAME_PLAYING && !duelMatchOver ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            } else {
                // Disconnect from multiplayer if active
                if (mpContext.active) {
                    MimitaNet::mpShutdown(mpContext);
                }
                gameState = GAME_MENU;
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        escapePrev = escapeDown;

        // Terminal rendering (on top of everything)
        Terminal::instance().render();

        engine.endFrame();

    }

    printf("[MAIN] loop ended\n");
    MimitaNet::mpShutdown(mpContext);
    HotReloadSystem::instance().unloadGameDLL();
    engine.shutdown();
    
    return 0;
}
