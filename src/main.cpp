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
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "sim/sim-context.h"
#include "combat/weapon-hit.h"

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

    // Gameplay terminal commands
    Terminal::instance().registerCommand({
        "jump", "Execute a jump action", "jump",
        [](const std::vector<std::string>&) {
            auto& cmd = InputCommandSystem::instance();
            InputCommandState& s = const_cast<InputCommandState&>(cmd.getState("jump"));
            s.pressed = true;
            s.held = true;
            Terminal::instance().addLog("[GAMEPLAY] jump");
        }
    });

    Terminal::instance().registerCommand({
        "dash", "Execute a dash action", "dash",
        [](const std::vector<std::string>&) {
            auto& cmd = InputCommandSystem::instance();
            InputCommandState& s = const_cast<InputCommandState&>(cmd.getState("dash"));
            s.pressed = true;
            Terminal::instance().addLog("[GAMEPLAY] dash");
        }
    });

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
        [&player](const std::vector<std::string>&) {
            weaponHit(player);
            Terminal::instance().addLog("[GAMEPLAY] shoot");
        }
    });

    Terminal::instance().registerCommand({
        "reload", "Reload weapon", "reload",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[GAMEPLAY] reload");
        }
    });

    // Replay terminal commands
    Terminal::instance().registerCommand({
        "replay.record", "Start replay recording", "replay.record",
        [](const std::vector<std::string>&) {
            if (gReplayRecorder.isRecording()) {
                Terminal::instance().addLog("[REPLAY] Already recording");
                return;
            }
            gReplayRecorder.beginRecording(0.0f, "mimita");
            Terminal::instance().addLog("[REPLAY] Recording started");
        }
    });

    Terminal::instance().registerCommand({
        "replay.stop", "Stop replay recording or playback", "replay.stop",
        [](const std::vector<std::string>&) {
            if (gReplayRecorder.isRecording()) {
                gReplayRecorder.stopRecording();
                Terminal::instance().addLog("[REPLAY] Recording stopped");
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
            gReplayRecorder.exportToJSON(path);
            Terminal::instance().addLog("[REPLAY] Exported to " + path);
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

    // SimContext setup: bundle sim state for replay/deterministic ticks
    SimContext simContext;
    simContext.player = &player;
    simContext.world = &world;
    simContext.npcSystem = &npcSystem;
    simContext.randomSeed = 0.0f;

    constexpr double SIM_DT = 1.0 / 60.0;
    double simAccumulator = 0.0;

    // start in main menu
    GameState gameState = GAME_MENU;
    GameState prevState = GAME_MENU;

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
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
                    tickFrame = buildInputFrame(engine.window(), camera);
                }

                // Record to replay
                if (gReplayRecorder.isRecording()) {
                    gReplayRecorder.recordFrame(tickFrame);
                }

                // Run simulation for this tick
                simulateTick(simContext, tickFrame);

                simAccumulator -= SIM_DT;
            }

            // Process NPC spawn commands (from console or F2) — after sim tick
            ProcessNpcSpawnCommands(npcSystem, camera, world, player);
            HandleF2SpawnNpc(npcSystem, camera, world, player, engine.window());

            applyDebugMovement(player, engine.window(), camera, dt);

            camera.updateVectors();
            camera.follow(player.pos);

            player.updateAudio(dt);

            // Update effect parts
            EffectPartSystem::instance().update(dt);

            renderWorld(world, camera);
            renderPlayer(player, camera);
            npcSystem.render(camera);
            
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
            uiDrawRect({14, 78, 260, 92}, {0.0f, 0.0f, 0.0f, 0.56f}, "hud-bg");
            uiDrawText("admin", 24, 88, 0.42f, {0.95f, 0.98f, 1.0f, 1.0f});
            uiDrawText("HP: 100/100", 24, 116, 0.38f, {0.35f, 1.0f, 0.45f, 1.0f});
            {
                glm::vec3 totalVel = player.vel + glm::vec3(player.dashVel.x, player.dashVel.y, 0.0f);
                float speed = glm::length(totalVel);
                char speedText[64];
                snprintf(speedText, sizeof(speedText), "Speed: %.2f m/s", speed);
                uiDrawText(speedText, 24, 144, 0.38f, {0.75f, 0.9f, 1.0f, 1.0f});
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
            uiRenderFrameDebugOverlay(engine.window(), "PLAYING", worldPassRan);
            uiEndFrame();

            // Dev overlay notifications (temporary)
            DevOverlay::instance().render();
        }

        if (gameState == GAME_MENU)
        {
            guiMain(engine.window(), gameState);
        }

        if (glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            if (Terminal::instance().isOpen()) {
                Terminal::instance().toggle();
            } else {
                gameState = GAME_MENU;
            }
        }

        // Terminal rendering (on top of everything)
        Terminal::instance().render();

        engine.endFrame();

    }

    printf("[MAIN] loop ended\n");
    engine.shutdown();
    
    return 0;
}
