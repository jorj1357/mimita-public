// C:\important\quiet\n\mimita-public\mimita-public\src\main.cpp
// dec 17 2025 new update but not made dec 17 2025

/**
 * purpose
 * only calls other files functions
 * does no math here
 * todo cleanup bc like 50% is just comments 
 * also can we just include a file that has all the includes?
 * so includes arent giant 
 */

// main.cpp
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE

/*
todo nov 6 2025
sort the includes by directory and stuff
*/

#include "renderer/renderer.h"
extern Renderer* gRenderer;

#include "map/map_common.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// later when we need our own sure
// #include "map/map_editor.h"
#include "entities/player.h"

#include <vector>
#include <algorithm>
#include <cstdlib>   // for rand()
#include <ctime>

#include "renderer/renderer.h"

#include "map/texture.h"

#include "entities/enemy.h"
#include "weapons/weapon.h"
#include "weapons/projectile.h"

#include "map/map_loader.h"
#include "map/map_render.h"

#include "physics/physics.h"
#include "physics/config.h"

#include "map/texture_manager.h"
#include <random>
#include <ctime>

#include "camera.h"

// dec 3 2025 todo sort includes maybe 
#include "utils/mesh_utils.h"

// dec 16 2025 use config everwhere 
#include "physics/config.h"
#include "physics/config-loader.h"
#include "debug/debug-status.h"

#include "world/world.h"

// where to put this idk dec 17 2025 
World world;

TextureManager TEX; // global instance
GLuint groundTex;

bool editMode = false;

/*
todo
migrate to camera.cpp
main.cpp should ONLY call functions from other files
*/

// global calls for things
// map editor for later not rn just use blender rn
// MapEditor editor; // global
Enemy enemy;
Weapon weapon;
std::vector<Projectile> projectiles;
GLuint createMapVAO(const Mesh&);

void drawMap(const Mesh& mesh);

// add this above main
Camera* activeCamera = nullptr; // declare globally

// and add this
Renderer* gRenderer = nullptr;
GLuint gMainShaderProgram = 0;

int main() {
    srand((unsigned)time(NULL));
    Renderer renderer(800, 600, "mimita.exe");
    gRenderer = &renderer;

    // now OpenGL exists, so load textures
    TEX.init();  // add this right after Renderer renderer(...)
        // Draw map with my texture yay
    GLuint shaderProgram = renderer.getShaderProgram();  // <--- move this up, right after TEX.init();
    printf("Window pointer: %p\n", renderer.window);
    if (!renderer.window) return -1;

    // mesh loading
    // nov 6 2025 todo do we rly need to load the mesh every loop? performance hit maybe
    // dec 2 2025 PLZ todo no more hard coding this high ke make me mad
    // Mesh playerMesh = loadOBJ("assets/entity/player/default/mimita-dev-player-v1.obj");
    // dec 3 2025 new cool
    // dec 18 2025 make sure the origin is at the feet 
    Mesh playerMesh = loadOBJ("assets/entity/player/default/mimita-char-concise-v3.obj");
    // dec 3 2025 todo where put this
    glm::vec3 meshMin, meshMax;
    computeMeshBounds(playerMesh, meshMin, meshMax);

    glm::vec3 meshDims = meshMax - meshMin;
    // dec 3 2025 todo WTF IS M HITBOX 
    printf("Mesh dims: %f %f %f\n", meshDims.x, meshDims.y, meshDims.z);

    GLuint playerVAO = createMapVAO(playerMesh);
    GLuint playerTex = loadTexture("assets/textures/greenwirev1.png");

    printf("Loading map...\n");


    // 4 squares trailer map
    // Mesh map = loadOBJ("assets/maps/mimita-4-squares-map-v1.obj");

    // collisions test map dec 3 2025
    // Mesh map = loadOBJ("assets/maps/mimita-collisions-test-map.obj");

    // big slopes map dec 3 2025 test
    // Mesh map = loadOBJ("assets/maps/mimita-big-map-v2.obj");

    // sizing test dec 4 2025 test
    // Mesh map = loadOBJ("assets/maps/mimita-sizing-v1.obj");

    // sizing test better dec 16 2025
    // Mesh map = loadOBJ("assets/maps/mimita-sizing-better-v2.obj");

    // apartments first real map i think
    // first REAL map dec 18 2025 ugh
    Mesh map = loadOBJ("assets/maps/mimita-apartments-final-v2.obj");

    // do this so that the map actually has data for us to walk on 
    // ---- BAKE MAP TRANSFORM FOR PHYSICS ----
    glm::mat4 mapTransform = glm::mat4(1.0f);

    // example values – adjust if you move/scale the map
    mapTransform = glm::scale(mapTransform, glm::vec3(1.0f));
    mapTransform = glm::translate(mapTransform, glm::vec3(0.0f));
    mapTransform = glm::rotate(mapTransform, 0.0f, glm::vec3(0,1,0));

    for (auto& v : map.verts) {
        glm::vec4 p = mapTransform * glm::vec4(v.pos, 1.0f);
        v.pos = glm::vec3(p);
    }

    // debug to see if it actually wokrs dec 12 2025
        glm::vec3 min(1e9f), max(-1e9f);
    for (auto& v : map.verts) {
        min = glm::min(min, v.pos);
        max = glm::max(max, v.pos);
    }
    fprintf(stderr, "MAP AABB min(%f %f %f) max(%f %f %f)\n",
            min.x, min.y, min.z, max.x, max.y, max.z);

    if (map.verts.empty()) {
        fprintf(stderr, "Map failed to load or has 0 verts.\n");
        return -1;
    }
    GLuint mapVAO = createMapVAO(map);
    
    // NOW we do world chunking ...? dec 17 2025
    world.buildFromMesh(map);

    // i dotn want enemy for now but later yes we need ai 
    // place enemy at map center and on top of ground
    // glm::vec3 avg = glm::vec3(0);
    // for (auto& v : map.verts) avg += v.pos;
    // avg /= (float)map.verts.size();
    // enemy.pos = avg; // center on map
    // enemy.pos.y += 1.0f; // lift slightly above ground

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    Player player;
    Camera camera;
    activeCamera = &camera;
    glfwSetCursorPosCallback(renderer.window, [](GLFWwindow*, double x, double y) {
        if (activeCamera) activeCamera->updateMouse(x, y);
    });

    while (!renderer.shouldClose()) {
        float dt = renderer.beginFrame();

        // Camera
        // calling only, not writing code in main.cpp
        camera.follow(player.pos);
        player.yaw = camera.yaw;        // <-- absolutely required
        glm::mat4 view = camera.getView();
        // can we make it not 800 x 600 plz i hate this tiny little chud window 
        glm::mat4 proj = camera.getProj(800.0f, 600.0f);

        // simple floor collision BEFORE rendering stuff
        // do u put this here nov 6 2025 todo
        // world WAS map now its world for chunking dec 17 2025 
        updatePhysics(player, world, renderer.window, dt, camera);

        // DEBUG: draw collision spheres
        // const glm::vec3 SPHERE_OFFSETS[3] = {
        //     glm::vec3(0.0f, PLAYER_RADIUS, 0.0f),
        //     glm::vec3(0.0f, 0.9f, 0.0f),
        //     glm::vec3(0.0f, 1.6f, 0.0f)
        // };

        // for (int s = 0; s < 3; ++s)
        // {
        //     glm::vec3 center = player.pos + SPHERE_OFFSETS[s];
        //     renderer.drawDebugSphere(
        //         center,
        //         PLAYER_RADIUS,
        //         glm::vec3(1.0f, 0.0f, 0.0f),
        //         view,
        //         proj);
        // }

        // draw m hitbox for n ow dec 16 2025  nvm dont 
        // Main should never calculate collision shapes.
        // glm::vec3 feetSphereCenter = player.pos + glm::vec3(0.0f, PLAYER_RADIUS, 0.0f);
        // glm::vec3 headSphereCenter = player.pos + glm::vec3(
        //     0.0f,
        //     PLAYER_HEIGHT - PLAYER_RADIUS,
        //     0.0f
        // );

        // renderer.drawDebugSphere(player.debugFeetSphere, PLAYER_RADIUS, {1,0,0}, view, proj);
        // renderer.drawDebugSphere(player.debugMidSphere,  PLAYER_RADIUS, {0,1,0}, view, proj);
        // renderer.drawDebugSphere(player.debugHeadSphere, PLAYER_RADIUS, {0,0,1}, view, proj);

        // player drawing logic in player.cpp
        player.render(shaderProgram, playerVAO, playerMesh.verts.size(), view, proj, camera, playerTex);

        char hud[128];
        snprintf(hud, sizeof(hud), "Speed: --- km/h   Mode: PLAY");
        if (gDebugStatusTimer > 0.0f)
        {
            drawText2D(
                gDebugStatusText.c_str(),
                10,
                550,
                1.0f
            );

            gDebugStatusTimer -= dt;
        }
        // use mingliu font eventually
        drawText2D(hud, 10, 580, 1.0f);

        // Player shoot
        // if (glfwGetMouseButton(renderer.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        //     glm::vec3 shootDir = glm::normalize(camera.front);
        //     weapon.shoot(projectiles, player.pos + glm::vec3(0,1,0), shootDir);
        // }

        // Update
        weapon.update(dt);
        enemy.update(dt);
        for (auto& p : projectiles) p.update(dt);
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p){ return p.life <= 0; }), projectiles.end());

        glUseProgram(shaderProgram);

        glm::mat4 model(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,&model[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,&proj[0][0]);

        // Right before calling drawMap(...) in main.cpp, add:
        glUniform1i(glGetUniformLocation(shaderProgram, "useTex"), true);
        glUniform1i(glGetUniformLocation(shaderProgram, "tex"), 0);
        drawMap(mapVAO, map.verts.size());

        glUniform1i(glGetUniformLocation(shaderProgram,"useTex"), false);
        glBindVertexArray(0);
        glUseProgram(0);
        glBindVertexArray(0);
        glUseProgram(0);

        // im not a cube... im a mesh
        // player.cpp has drawcube i think idk
        // renderer.drawCube(player.pos, view, proj);
        enemy.draw(renderer, view, proj);
        for (auto& p : projectiles) p.draw(renderer, view, proj);

        // hot reload
        // hotReloadPhysicsConfig();

        renderer.endFrame();
    }

    renderer.shutdown();
    return 0;
}
