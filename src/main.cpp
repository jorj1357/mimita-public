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
#include "camera.h"
#include "input/input-state.h"
#include "input/input-poll.h"
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

#include <cstdio>

int main()
{
    printf("[MAIN] start\n");

    Engine engine;
    printf("[MAIN] before engine.init\n");
    engine.init(800, 600, "mimita.exe");
    printf("[MAIN] after engine.init\n");

    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    printf("[MAIN] after glfwSetInputMode\n");

    fontInit();   // load .fnt + png
    printf("[MAIN] after fontInit()\n");
    uiInit(engine.window());
    printf("[MAIN] after uiInit()\n");
    DebugVis::init(engine.window());
    Debug::startupReport();
    printf("[MAIN] after DebugVis::init()\n");

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

    Camera camera;
    printf("[MAIN] camera made\n");

    engine.bindCamera(&camera);
    // onl do this 1 time, not per frame
    // not in the while X loop
    glfwSetWindowUserPointer(engine.window(), &camera);
    printf("[MAIN] camera bound\n");

    // mar 13 2026 unused so commented?
    // int frame = 0;

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
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else
            {
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            prevState = gameState;
        }

        InputState input = pollInput(engine.window(), camera);

        if (gameState == GAME_PLAYING)
        {
            DebugVis::beginCollisionFrame();
            printf("lallaa 1 \n");
            physicsMainUpdate(player, world, input, dt);
                        printf("lallaa 2 \n");


            applyDebugMovement(player, engine.window(), camera, dt);

            camera.updateVectors();
            camera.follow(player.pos);

            player.updateAudio(dt);

            renderWorld(world, camera);
            renderPlayer(player, camera);
            worldPassRan = true;

            drawDebugStuff(player, camera, world);
                        printf("lallaa 3 \n");

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
        }

                                printf("lallaa 4 \n");


        if (gameState == GAME_MENU)
        {
            guiMain(engine.window(), gameState);
        }

        if (glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            gameState = GAME_MENU;
        }

        engine.endFrame();
                                printf("lallaa 4 \n");

    }

    printf("[MAIN] loop ended\n");
    
    return 0;
}
