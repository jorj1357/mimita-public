#include "main-systems.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <memory>
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "camera.h"
#include "config/camera-config.h"
#include "input/input-state.h"
#include "input/input-poll.h"
#include "input/input-frame.h"
#include "input/input-commands.h"
#include "render/render-world.h"
#include "render/post-fx.h"
#include "render/render-player.h"
#include "physics/physics-mini.h"
#include "physics/physics-debug-movement.h"
#include "physics/ray-utils.h"
#include "physics/movement/physics-collision.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "audio/music-manager.h"
#include "analytics/analytics-manager.h"
#include "gui/gui-main.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-editor.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "game/game-cli.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/transform-debug.h"
#include "debug/transform-debug-commands.h"
#include "debug/debug-diag.h"
#include "debug/log-manager.h"
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/dev-teleport.h"
#include "devtools/dev-commands.h"
#include "devtools/terminal.h"
#include "devtools/account-config.h"
#include "devtools/npc-spawn-commands.h"
#include "devtools/dev-log-commands.h"
#include "devtools/dev-overlay-commands.h"
#include "ui/hitmarker.h"
#include "gui/hud/chat-bubble.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "replay/replay-factory-worker.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "shadow/shadow-commands.h"
#include "video/video-settings.h"
#include "video/outro.h"
#include "video/frame-pacer.h"
#include "video/video-commands.h"
#include "sim/sim-context.h"
#include "engine/engine-tick.h"
#include "combat/weapon-hit.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "ragdoll/ragdoll.h"
#include "ragdoll/ragdoll-config.h"
#include "ragdoll/ragdoll-commands.h"
#include "void-death/void-death.h"
#include "crosshair/crosshair-commands.h"
#include "crosshair/crosshair-config.h"
#include "crosshair/crosshair-render.h"
#include "gui/hud/healthbar-config.h"
#include "replay/replay-camera.h"
#include "config/gameplay-config.h"
#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "avatar/avatar.h"
#include "avatar/avatar-commands.h"
void registerCompetitiveCommands();
void registerCursorCommands();

#include "avatar/avatar-menu.h"
#include "avatar/character-registry.h"
#include "render/lighting-config.h"
#include "render/lighting-commands.h"
#include "render/postfx-commands.h"
#include "hot-reload/hot-reload-system.h"
#include "auth/auth-system.h"
#include "profile/local-profile-system.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"
#include "game/duel.h"
#include "gui/menus/duel-config-menu.h"
#include "terminal/terminal-state.h"
#include "terminal/replay-commands.h"
#include "terminal/debug-commands.h"
#include "terminal/player-commands.h"
#include "terminal/weapon-commands.h"
#include "terminal/weapon-bench-commands.h"
#include "terminal/npc-commands.h"
#include "terminal/duel-commands.h"
#include "terminal/editor-commands.h"
#include "terminal/network-commands.h"
#include "perf/perf.h"
#include "replay/replay-export.h"
#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "config/weapon-hitfx-config.h"
#include "game/bomb-tag.h"
#include "gui/gui-editor-commands.h"
#include "camera/camera-commands.h"
#include "audio/music-commands.h"
#include <windows.h>
#include <glad/glad.h>

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern Player* gpPlayer;
extern Camera* gpCamera;
extern World* gpWorld;
extern NpcSystem* gpNpcSystem;
extern WeaponSystem* gpWeapons;
extern bool* gpFreecamEnabled;
extern glm::vec3* gpDeathPosition;
extern int* gpSelectedEditorObject;
extern bool* gpEditorMode;
extern std::string* gpActiveGameMode;
extern std::string* gpActiveMapPath;
extern bool* gpWorldLoaded;
extern ReplayRingBuffer* gpReplayRecorder;
extern ReplayPlayer* gpReplayPlayer;
extern ReplayClipSaver* gpReplayClipSaver;
extern ReplayFactory* gpReplayFactory;
extern ReplayBrowser* gpReplayBrowser;
extern ReplayTimeline* gpReplayTimeline;
extern ReplayCameraMgr* gpReplayCameraMgr;
extern std::unordered_map<std::string, ActorChatState>* gpReplayChatStates;
extern std::vector<std::string>* gpReplayClipsCache;
extern std::unordered_map<int, std::string>* gpCommandBinds;
extern std::unordered_map<int, bool>* gpBindPrev;
extern DuelConfig* gpDuelConfig;
extern MimitaNet::MultiplayerContext* gpMpContext;
extern GameState* gpGameState;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;
extern bool gMainmenuDebug;

extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;

extern Renderer* gRenderer;

void gameInitSubsystems(Engine& engine)
{
    DevConfig::instance().load("config/dev_controls.txt");
    DevOverlay::instance().init(engine.window());
    DevOverlay::instance().showNotification("Dev mode enabled. Press ` to open console.", 5.0f);
    CreateDefaultAccountConfig();
    LoadAccountConfig("default");
    LoadDuelStats("default");

    MusicManager::instance().init();
    MusicManager::instance().setVolume(GetPlayerSettings().musicVolume);
    MusicManager::instance().setMuted(GetPlayerSettings().musicMuted);

    VideoSettings::instance().load();
    VideoSettings::instance().apply();
    gFramePacer.setMaxFrames(VideoSettings::instance().maxFrames());
    gFramePacer.setVSync(VideoSettings::instance().vsync());

    {
        extern Renderer* gRenderer;
        PostFX::instance().loadConfig("config/postfx.json");
        PostFX::instance().initFBO(gRenderer->width, gRenderer->height);
    }

    InputCommandSystem::instance().init(engine.window());
    InputCommandSystem::instance().loadBinds("config/accounts/default.json");
    RegisterTeleportCommands();
    Terminal::instance().init(engine.window());
    AnalyticsManager::instance().registerCommands();

    LightingConfig::instance().load("config/lighting.json");
    ShadowConfig::instance().load("config/shadows.json");
    GameplayConfig::instance().load("config/gameplay.json");
    CrosshairConfig::instance().load();
    registerCrosshairCommands();
    HealthbarConfig::instance().load();

    EffectPartSystem::instance().init();
    HotReloadSystem::instance().loadGameDLL();
    printf("[MAIN] dev tools initialized\n");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    printf("[MAIN] after glEnable and glBlendFunc()\n");

    static World world;
    static bool worldLoaded = false;
    printf("[MAIN] world object made; world JSON loads when PLAY is pressed so the menu appears first\n");

    static Player player;
    player.username = AuthSystem::instance().displayName();
    Debug::warn(Debug::Category::Auth, "[BOOT] player.username=%s\n", player.username.c_str());
    player.equippedSlot = GetPlayerSettings().equippedSlot;
    CharacterRegistry::instance().scanAll();
    if (!GetPlayerSettings().characterName.empty())
        player.loadCharacter(GetPlayerSettings().characterName);


    if (!GetPlayerSettings().avatarName.empty())
        AvatarSystem::instance().loadAvatar(GetPlayerSettings().avatarName);
    if (AvatarSystem::instance().hasAvatar())
        AvatarSystem::instance().applyToPlayer(player);

    printf("[MAIN] player made\n");

    static NpcSystem npcSystem;
    printf("[MAIN] npc system made\n");

    static Camera camera;
    printf("[MAIN] camera made\n");
    printf("[CAMERA] Config: stiffness=%.2f  fov=%.0f\n", CamConfig::instance().data().positionStiffness, CamConfig::instance().data().fov);
    printf("[CAMERA] Raw 1:1 camera enabled\n");

    engine.bindCamera(&camera);
    glfwSetWindowUserPointer(engine.window(), &camera);
    printf("[MAIN] camera bound\n");

    static ReplayRingBuffer gReplayRecorder;
    static ReplayPlayer gReplayPlayer;
    static ReplayClipSaver gReplayClipSaver(gReplayRecorder);
    setActiveReplayClipSaver(&gReplayClipSaver);
    static ReplaySaveWorker gReplayWorker;
    static ReplayFactory gReplayFactory(gReplayRecorder);
    gReplayFactory.setWorker(&gReplayWorker);
    static ReplayBrowser gReplayBrowser;
    static ReplayTimeline gReplayTimeline;
    static ReplayCameraMgr gReplayCameraMgr;

    static GameState gameState = GAME_MENU;

    setReplayFactoryNotifyFn([](const std::string& killerId,
                                 const std::string& victimId,
                                 bool killerAirborne,
                                 bool victimAirborne,
                                 bool roundWinning) {
        gReplayFactory.notifyKill(killerId, victimId, killerAirborne, victimAirborne, roundWinning);
    });

    gReplayBrowser.setPlayCallback([](const std::string& path) {
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
    static std::mt19937 rng(std::random_device{}());

    static DuelConfig gDuelConfig;

    static MimitaNet::MultiplayerContext mpContext;

    static bool editorMode = false;
    static std::string activeGameMode = "sandbox";
    const std::string defaultMapPath =
        "assets/maps/mimita-aabb-only-interior-small-v4.glb";
    static std::string activeMapPath;
    static int selectedEditorObject = -1;
    static WeaponSystem weapons;
    static std::unordered_map<std::string, std::unique_ptr<Player>> replayActorModels;
    static std::unordered_map<std::string, WeaponViewModel> replayWeaponModels;
    gpReplayActorModels = &replayActorModels;
    gpReplayWeaponModels = &replayWeaponModels;
    static bool freecamEnabled = false;
    static glm::vec3 deathPosition{0.0f};
    struct ReplayTestState {
        bool active = false;
        uint32_t tick = 0;
        uint32_t npcId = 0;
    };
    static ReplayTestState replayTest;

    gpPlayer = &player;
    gpCamera = &camera;
    gpWeapons = &weapons;
    gpReplayRecorder = &gReplayRecorder;
    gpReplayPlayer = &gReplayPlayer;
    gpReplayClipSaver = &gReplayClipSaver;
    gpReplayFactory = &gReplayFactory;
    gpReplayBrowser = &gReplayBrowser;
    gpReplayTimeline = &gReplayTimeline;
    gpReplayCameraMgr = &gReplayCameraMgr;
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

    registerPlayerCommands();
    registerWeaponCommands();
    registerWeaponBenchCommands();
    registerWeaponDebugCommand();
    loadWorldCrosshairConfig();
    applyStartupDefaults();
    registerWorldXhReloadCommand();
    registerCameraCommands();
    registerGuiEditorCommands();
    registerPostFxCommands();
    registerLightingCommands();
    registerVideoCommands();
    registerMusicCommands();
    registerDevLogCommands();
    registerDevOverlayCommands();

    registerNpcCommands();
    registerTransformDebugCommands();

    registerEditorCommands();
    registerNetworkCommands();

    registerReplayCaptureCommands();
    registerReplayCommands();
    registerReplayCameraCommands();

    // Bind F3 to save instant replay
    {
        auto& binds = G_COMMAND_BINDS;
        binds[GLFW_KEY_F3] = "replay.save";
    }
    registerAvatarCommands(player);
    registerVoidDeathCommands();
    registerHitmarkerAudioCommands();
    registerOutroCommands();
    registerPerfCommands();
    registerHitFxCommands();
    registerDiagnosticCommands();
    registerShadowCommands();
    registerDebugCommands();
    registerWorldTextureCommands();
    SizeScalingConfig::instance().load("config/size_scaling.json");
    RagdollConfig::instance().load("config/ragdolldeath.json");
    registerRagdollCommands();
    HitEffects::loadConfig("config/hitfx.json");
    WeaponHitFxConfig::instance().load("config/weapon_hitfx.json");

    Terminal::instance().registerCommand({
        "net_debug_entities", "Toggle entity replication debug overlay", "net_debug_entities [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gNetDebugEntities = !gNetDebugEntities;
            } else {
                gNetDebugEntities = args[0] == "1";
            }
            printf("[NET ENTITIES DEBUG] %s\n", gNetDebugEntities ? "ON" : "OFF");
            Terminal::instance().addLog(gNetDebugEntities
                ? "[NET] Entity debug ON"
                : "[NET] Entity debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "replay_test",
        "Record a deterministic gameplay replay and validate it in Blender",
        "replay_test",
        [](const std::vector<std::string>&) {
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
            replayTest.npcId = npcSystem.nextNpcId();
            npcSystem.spawnNpc(replayTest.npcId, 1.0f, npcSpawnPoint);
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

    registerDuelCommands();
    registerCompetitiveCommands();
    registerCursorCommands();
}

