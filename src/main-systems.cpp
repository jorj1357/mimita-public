// 08 16 2026, 01 35
/* purpose
* Initializes MiMITA runtime subsystems, command registration, and default binds.
* Connects engine-owned services to the shared gameplay and UI orchestration loop.
* Establishes replay clip export shortcuts while preserving editor commands.
* Does NOT implement subsystem feature logic or encoder backends.
* Does NOT own production deployment, launcher updates, or website behavior.
* Does NOT replace feature-owned configuration or terminal command handlers.
*/
#include "main-systems.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
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
#include "gui/hud/chat-history.h"
#include "gui/hud/chat-window.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "game/game-cli.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
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
#include "ragdoll/ragdoll-commands.h"
#include "config/spawn-velocity-config.h"
#include "void-death/void-death.h"
#include "crosshair/crosshair-commands.h"
#include "crosshair/crosshair-config.h"
#include "crosshair/crosshair-render.h"
#include "gui/hud/healthbar-config.h"
#include "replay/replay-camera.h"
#include "config/gameplay-config.h"
#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "config/movement-config.h"
#include "npc/npc-difficulty-config.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "npc/npc-behavior.h"
#include "gamemode/gamemode-map-pool.h"
#include "avatar/avatar.h"
#include "avatar/avatar-commands.h"
#include "terminal/map-entity-commands.h"
#include "entities/aim-commands.h"
#include "entities/aimbody-config.h"
void registerCompetitiveCommands();
void registerLeaderboardCommands();
void registerCursorCommands();

// The terminal-state convenience macros are included later with the command
// declarations; these globals are needed by the early deterministic-command
// registration helpers below.
extern Player* gpPlayer;
extern Camera* gpCamera;
extern MimitaNet::MultiplayerContext* gpMpContext;

namespace {

bool parseAgentFloat(const std::vector<std::string>& args, size_t index, float& out)
{
    if (index >= args.size()) return false;
    try {
        size_t consumed = 0;
        out = std::stof(args[index], &consumed);
        return consumed == args[index].size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

void writeAgentCommandEvent(const std::string& eventName,
                            const std::string& reason,
                            nlohmann::json fields)
{
    const std::string requestId = Terminal::instance().currentRequestId();
    fields["request_id"] = requestId;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        eventName, requestId.empty() ? "agent-command" : requestId,
        reason, 0, fields,
        __FILE__, __LINE__, __FUNCTION__);
}

void registerAgentCommands()
{
    Terminal::instance().registerCommand({
        "agent.move", "Hold camera-relative movement for fixed ticks",
        "agent.move <forward> <side> <duration_ticks>",
        [](const std::vector<std::string>& args) {
            float forward = 0.0f, side = 0.0f, ticksFloat = 0.0f;
            if (args.size() != 3 || !parseAgentFloat(args, 0, forward) ||
                !parseAgentFloat(args, 1, side) || !parseAgentFloat(args, 2, ticksFloat)) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.move <forward> <side> <duration_ticks>");
                return;
            }
            const int ticks = std::clamp((int)std::lround(ticksFloat), 1, 3600);
            glm::vec3 forwardVec = gpCamera->front;
            forwardVec.z = 0.0f;
            if (glm::length(forwardVec) < 0.001f)
                forwardVec = glm::vec3(0.0f, 1.0f, 0.0f);
            else
                forwardVec = glm::normalize(forwardVec);
            const glm::vec3 rightVec = glm::normalize(
                glm::cross(forwardVec, glm::vec3(0.0f, 0.0f, 1.0f)));
            const glm::vec3 worldMove = forward * forwardVec + side * rightVec;
            // The modern multiplayer owner consumes raw camera-relative axes;
            // it applies camera orientation exactly like keyboard input.
            scheduleTerminalMove(side, forward, ticks);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.move accepted",
                {{"command", "agent.move"}, {"forward", forward}, {"side", side},
                 {"duration_ticks", ticks}, {"move_axes", {side, forward}},
                 {"world_move", {worldMove.x, worldMove.y}},
                 {"player", gpPlayer->username}});
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.aim", "Aim the normal camera and player input owner at a world point",
        "agent.aim <x> <y> <z>",
        [](const std::vector<std::string>& args) {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (args.size() != 3 || !parseAgentFloat(args, 0, x) ||
                !parseAgentFloat(args, 1, y) || !parseAgentFloat(args, 2, z)) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.aim <x> <y> <z>");
                return;
            }
            glm::vec3 direction = glm::vec3(x, y, z) - gpCamera->pos;
            if (glm::length(direction) < 0.001f) {
                Terminal::instance().addLog("[AGENT] aim target is at camera position");
                return;
            }
            direction = glm::normalize(direction);
            gpCamera->yaw = glm::degrees(std::atan2(direction.y, direction.x));
            gpCamera->pitch = glm::degrees(std::asin(std::clamp(direction.z, -1.0f, 1.0f)));
            gpCamera->updateVectors();
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.aim accepted",
                {{"command", "agent.aim"}, {"target", {x, y, z}},
                 {"camera_position", {gpCamera->pos.x, gpCamera->pos.y, gpCamera->pos.z}},
                 {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                 {"yaw", gpCamera->yaw}, {"pitch", gpCamera->pitch},
                 {"player", gpPlayer->username}});
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.shoot", "Fire once through the normal weapon command owner",
        "agent.shoot",
        [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                Terminal::instance().addLog("[AGENT] agent.shoot currently accepts no duration; use repeated commands");
                return;
            }
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.shoot accepted",
                {{"command", "agent.shoot"}, {"player", gpPlayer->username},
                 {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}}});
            Terminal::instance().execute("shoot");
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.action", "Issue one normal action edge",
        "agent.action <jump|dash>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 1 || (args[0] != "jump" && args[0] != "dash")) {
                Terminal::instance().addLog("[AGENT] Usage: agent.action <jump|dash>");
                return;
            }
            if (args[0] == "jump") gTerminalInputOverride.jumpPressed = true;
            if (args[0] == "dash") gTerminalInputOverride.dashPressed = true;
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.action accepted",
                {{"command", "agent.action"}, {"action", args[0]},
                 {"player", gpPlayer->username}});
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.snapshot", "Record player, camera, avatar, and posed body transforms",
        "agent.snapshot <label>",
        [](const std::vector<std::string>& args) {
            const std::string label = args.empty() ? "unnamed" : args[0];
            nlohmann::json parts = nlohmann::json::array();
            for (const auto& part : gpPlayer->physicalBody.parts) {
                nlohmann::json item = {
                    {"name", part.name}, {"node_index", part.nodeIndex},
                    {"world_matrix", nlohmann::json::array()}};
                for (int col = 0; col < 4; ++col)
                    for (int row = 0; row < 4; ++row)
                        item["world_matrix"].push_back(part.worldTransform[col][row]);
                parts.push_back(std::move(item));
            }
            nlohmann::json fields = {
                {"command", "agent.snapshot"}, {"label", label},
                {"player", gpPlayer->username},
                {"fixed_tick", gpMpContext ? gpMpContext->tick : 0},
                {"player_position", {gpPlayer->pos.x, gpPlayer->pos.y, gpPlayer->pos.z}},
                {"player_velocity", {gpPlayer->vel.x, gpPlayer->vel.y, gpPlayer->vel.z}},
                {"camera_position", {gpCamera->pos.x, gpCamera->pos.y, gpCamera->pos.z}},
                {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                {"camera_forward", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                {"camera_yaw", gpCamera->yaw}, {"camera_pitch", gpCamera->pitch},
                {"current_animation", gpPlayer->currentAnimName},
                {"on_ground", gpPlayer->ground.onGround},
                {"stable_on_ground", gpPlayer->ground.stableOnGround},
                {"has_world_contact", gpPlayer->ground.hasWorldContact},
                {"size_scale", gpPlayer->sizeScale},
                {"model_loaded", gpPlayer->modelLoaded},
                {"freeze_active", gpPlayer->freeze.freezeActive},
                {"procedural_frozen", gpPlayer->proceduralFrozen},
                {"ragdoll_active", gpPlayer->ragdollModeActive},
                {"movement_mode", gpPlayer->rig.movement.mode},
                {"anim_clip_count",
                    gpPlayer->animationSet ? gpPlayer->animationSet->clips.size() : 0},
                {"server_pos_error",
                    (gpMpContext && gpMpContext->hasLocalServerPosition)
                        ? glm::length(gpPlayer->pos - gpMpContext->localServerPosition)
                        : -1.0f},
                {"snapshot_schema_version", 3}, {"parts", std::move(parts)}};
            fields["active_animation"] = gpPlayer->currentAnimName;
            fields["physicalBody"] = {{"parts", fields["parts"]}};
            if (gpPlayer->avatarInstance) {
                fields["avatar_name"] = gpPlayer->avatarInstance->definition.name;
                fields["model_path"] = gpPlayer->avatarInstance->definition.playerModel;
                fields["body_transform_source"] = gpPlayer->avatarInstance->definition.bodyTransformSource;
                fields["body_frame_policy"] = gpPlayer->avatarInstance->definition.bodyFramePolicy;
            }
            writeAgentCommandEvent("avatar.pose.sample", "agent.snapshot captured pose", fields);
            writeAgentCommandEvent("avatar.command.assert", "agent.snapshot captured", fields);
            Terminal::instance().addLog("[AGENT] snapshot captured: " + label);
        }, "2026-10-13", CommandCategory::Debug
    });
}

} // namespace

#include "avatar/character-registry.h"
#include "render/lighting-config.h"
#include "render/lighting-commands.h"
#include "render/dynamic-light-commands.h"
#include "render/dynamic-light-config.h"
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
#include "terminal/movement-commands.h"
#include "terminal/weapon-commands.h"
#include "terminal/weapon-bench-commands.h"
#include "terminal/npc-commands.h"
#include "terminal/crate-commands.h"
#include "terminal/object-commands.h"
#include "terminal/procedural-world-commands.h"
#include "terminal/duel-commands.h"
#include "terminal/actor-commands.h"
#include "terminal/editor-commands.h"
#include "terminal/network-commands.h"
#include "terminal/badconn-commands.h"
#include "terminal/vip-commands.h"
#include "network/badconn/badconn.h"
#include "terminal/auth-commands.h"
#include "perf/perf.h"
#include "replay/replay-export.h"
#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "config/weapon-hitfx-config.h"
#include "config/impact-decals-config.h"
#include "config/weapon-tracers-config.h"
#include "game/gamemode-manager.h"
#include "gui/gui-editor-commands.h"
#include "camera/camera-commands.h"
#include "audio/music-commands.h"
#include "notifications/notification-commands.h"
#include <windows.h>
#include <glad/glad.h>

extern DuelManager gDuelManager;
extern GamemodeManager gGamemodeManager;
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
extern ChatHistory* gpChatHistory;
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

    // MusicManager is lazy-initialized on first update() — not blocking startup

    VideoSettings::instance().load();
    VideoSettings::instance().apply();
    gFramePacer.setMaxFrames(VideoSettings::instance().maxFrames());
    gFramePacer.setVSync(false);

    printf("[VSYNC] FORCED OFF: software frame pacing only (maxFrames=%d)\n",
           gFramePacer.maxFrames());
    Debug::log(Debug::Category::Render,
        "[VSYNC] FORCED OFF: swap interval=0, software pacer active, "
        "VSync cannot be re-enabled at runtime\n");

    PostFX::instance().loadConfig("config/postfx.json");
    // PostFX FBO is lazily initialized on first bindFBO() call

    InputCommandSystem::instance().init(engine.window());
    InputCommandSystem::instance().loadBinds("config/accounts/default.json");
    initChatWindowState(gChatWindowState);
    RegisterTeleportCommands();
    Terminal::instance().init(engine.window());
    AnalyticsManager::instance().registerCommands();

    LightingConfig::instance().load("config/lighting.json");
    DynamicLightConfig::instance().load("config/lighting.json");
    ShadowConfig::instance().load("config/shadows.json");
    GameplayConfig::instance().load("config/gameplay.json");
    NpcDifficultyConfig::instance().load("config/npc-difficulty.json");
    GamemodeRegistry::instance().loadDirectory("config/gamemodes");
    MatchRoleRegistry::instance().load("config/roles.json");
    MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
    BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
    GamemodeMapPool::instance().load("config/gamemode-good-maps.json");
    MovementJsonConfig::instance().load("config/movement.json");
    CrosshairConfig::instance().load();
    AimBodyConfig::instance().load("config/aimbody.json");
    registerCrosshairCommands();
    HealthbarConfig::instance().load();

    EffectPartSystem::instance().init();
    // HotReload DLL is lazy-loaded on first reloadGameDLLIfChanged() call
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
    player.mCharacterName = GetPlayerSettings().characterName;
    if (player.mCharacterName.empty())
        player.mCharacterName = "DefaultGuy";

    // Avatar metadata is loaded at boot (fast ~1ms JSON parse).
    // Model + atlas are loaded lazily on first render via worker threads.
    if (!GetPlayerSettings().avatarName.empty())
        AvatarSystem::instance().loadAvatar(GetPlayerSettings().avatarName);

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
    static ReplaySaveWorker gReplayWorker;
    AvatarSystem::instance().setBackgroundWorker(&gReplayWorker);
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

    // Keep the owning object distinct from the gChatHistory convenience macro
    // in terminal-state.h so initialization targets the real storage object.
    static ChatHistory sChatHistory;

    static DuelConfig gDuelConfig;

    static MimitaNet::MultiplayerContext mpContext;

    static bool editorMode = false;
    static std::string activeGameMode = "sandbox";
    // const std::string defaultMapPath =
    //     "assets/maps/mimita-aabb-only-interior-small-v4.glb";
    // 9 5 2026 not good  needs to work without wifi
    // cuz i cant strart a server without wfii so just make it start a local one todo
    const std::string defaultMapPath =
       "assets/maps/dust2cyberiav4.glb";
        
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
    gpReplayFactory = &gReplayFactory;
    gpReplayBrowser = &gReplayBrowser;
    gpReplayTimeline = &gReplayTimeline;
    gpReplayCameraMgr = &gReplayCameraMgr;
    gpReplayChatStates = &gReplayChatStates;
    gpReplayClipsCache = &G_REPLAY_CLIPS_CACHE;
    gpCommandBinds = &G_COMMAND_BINDS;
    gpBindPrev = &G_BIND_PREV;
    gpGameState = &gameState;
    Debug::log(Debug::Category::Chat,
               "[CHAT TRACE 1 BEFORE INIT] gpChatHistory=%p\n",
               (void*)gpChatHistory);
    gpChatHistory = &sChatHistory;
    Debug::log(Debug::Category::Chat,
               "[CHAT TRACE 1 AFTER INIT] gpChatHistory=%p historyAddress=%p\n",
               (void*)gpChatHistory, (void*)&sChatHistory);
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
    registerMovementCommands();
    registerAimCommands();
    registerAuthCommands();
    registerVipCommands();
    registerWeaponCommands();
    registerWeaponBenchCommands();
    registerWeaponDebugCommand();
    loadWorldCrosshairConfig();
    loadCoolShotLineConfig();
    WeaponTracersConfig::instance().load();
    applyStartupDefaults();
    registerWorldXhReloadCommand();
    registerCoolShotLineCommands();
    registerCameraCommands();
    registerGuiEditorCommands();
    registerPostFxCommands();
    registerLightingCommands();
    registerDynamicLightCommands();
    registerVideoCommands();
    registerMusicCommands();
    registerNotificationCommands();
    registerDevLogCommands();
    registerDevOverlayCommands();

    registerNpcCommands();
    registerMapEntityCommands();
    registerCrateCommands();
    registerObjectCommands();
    registerProceduralWorldCommands();
    registerTransformDebugCommands();

    registerEditorCommands();
    registerNetworkCommands();
    badconn::loadConfig(badconn::configPath());
    registerBadConnCommands();

    registerReplayCaptureCommands();
    registerReplayCommands();
    registerReplayCameraCommands();

    // Bind F3 to save instant replay
    {
        auto& binds = G_COMMAND_BINDS;
        binds[GLFW_KEY_F3] = "replay.save";
        binds[GLFW_KEY_P] = "rplfx";
        binds[GLFW_KEY_J] = "replay_open_last_export";
    }
    registerAvatarCommands(player);
    registerAgentCommands();
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
    registerRagdollCommands();
    HitEffects::loadConfig("config/hitfx.json");
    WeaponHitFxConfig::instance().load("config/weapon_hitfx.json");
    ImpactDecalsConfig::instance().load("config/impact_decals.json");
    SpawnVelocityConfig::instance().load("config/spawnvelocity.json");

    Terminal::instance().registerCommand({
        "spawnvelocity_reload",
        "Reload spawnvelocity.json without restart",
        "spawnvelocity_reload",
        [](const std::vector<std::string>&) {
            if (SpawnVelocityConfig::instance().load()) {
                const auto& d = SpawnVelocityConfig::instance().data();
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "[SPAWN VELOCITY] reloaded: enabled=%d mode=%s speed=%.1f",
                    (int)d.enabled, d.mode.c_str(), d.speed);
                Terminal::instance().addLog(buf);
            } else {
                Terminal::instance().addLog(
                    "[SPAWN VELOCITY] config reload failed (see log)");
            }
        },
        "2026-09-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "spawnvelocity_status",
        "Print spawn velocity config status",
        "spawnvelocity_status",
        [](const std::vector<std::string>&) {
            const auto& d = SpawnVelocityConfig::instance().data();
            char buf[512];
            snprintf(buf, sizeof(buf),
                "[SPAWN VELOCITY] enabled=%d mode=%s speed=%.1f "
                "dir=(%.1f,%.1f,%.1f) vertical=%d range=(%.0f,%.0f)",
                (int)d.enabled, d.mode.c_str(), d.speed,
                d.direction.x, d.direction.y, d.direction.z,
                (int)d.applyVertical, d.randomRange.x, d.randomRange.y);
            Terminal::instance().addLog(buf);
        },
        "2026-09-08",
        CommandCategory::Debug
    });

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
    registerActorCommands();
    registerCompetitiveCommands();
    registerLeaderboardCommands();
    registerCursorCommands();
}

