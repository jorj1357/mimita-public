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

#include <cstdio>
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-loader.h"
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
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"

#include <cstdio>

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("[MAIN] start\n");

    Engine engine;
    printf("[MAIN] before engine.init\n");
    engine.init(800, 600, "mimita.exe");
    printf("[MAIN] after engine.init\n");

    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    printf("[MAIN] after glfwSetInputMode\n");

    fontInit();   // load .fnt + png
    printf("[MAIN] after fontInit()\n");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    printf("[MAIN] after glEnable and glBlendFunc()\n");


    // dont include these, fontInit() alread does tem? idk mar 13 2026
    // loadFontAtlas("assets/font/mingliu-mimita-v3_0.png");
    // printf("[MAIN] after loadFontAtlas()\n");

    // loadFontGlyphs("assets/font/mingliu-mimita-v3.fnt");
    // printf("[MAIN] after loadFontGlyphs()\n");

    World world;
    printf("[MAIN] before loadWorldFromJSON\n");
    loadWorldFromJSON(
        world,
        "assets/maps/json-converts/mimita-aabb-only-interior-small-v2-converted-v2.json"
    );
    printf("[MAIN] after loadWorldFromJSON\n");

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

        while (engine.running())
    {
        float dt = engine.beginFrame();

        audioUpdate(dt);

        InputState input = pollInput(engine.window(), camera);

        if (gameState == GAME_PLAYING)
        {
            physicsMainUpdate(player, world, input, dt);

            applyDebugMovement(player, engine.window(), camera, dt);

            camera.updateVectors();
            camera.follow(player.pos);

            player.updateAudio(dt);

            renderWorld(world, camera);
            renderPlayer(player, camera);
        }

        if (gameState == GAME_MENU)
        {
            guiMain(engine.window(), gameState);
        }

        if (glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            gameState = GAME_MENU;
        }

        engine.endFrame();
    }

    printf("[MAIN] loop ended\n");
    return 0;
}