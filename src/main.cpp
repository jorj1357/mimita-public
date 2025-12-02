// C:\important\go away v5\s\mimita-v5\src\main.cpp

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
    Mesh playerMesh = loadOBJ("assets/entity/player/default/mimita-dev-player-v1.obj");
    GLuint playerVAO = createMapVAO(playerMesh);
    GLuint playerTex = loadTexture("assets/textures/greenwirev1.png");

    printf("Loading map...\n");
    Mesh map = loadOBJ("assets/maps/mimita-4-squares-map-v1.obj");
    if (map.verts.empty()) {
        fprintf(stderr, "Map failed to load or has 0 verts.\n");
        return -1;
    }
    GLuint mapVAO = createMapVAO(map);

    // place enemy at map center and on top of ground
    glm::vec3 avg = glm::vec3(0);
    for (auto& v : map.verts) avg += v.pos;
    avg /= (float)map.verts.size();
    enemy.pos = avg; // center on map
    enemy.pos.y += 1.0f; // lift slightly above ground

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
        glm::mat4 proj = camera.getProj(800.0f, 600.0f);

        // simple floor collision BEFORE rendering stuff
        // do u put this here nov 6 2025 todo
        updatePhysics(player, map, renderer.window, dt, camera);

        // player drawing logic in player.cpp
        player.render(shaderProgram, playerVAO, playerMesh.verts.size(), view, proj, camera, playerTex);

        char hud[128];
        snprintf(hud, sizeof(hud), "Speed: --- km/h   Mode: PLAY");
        // use mingliu font eventually
        drawText2D(hud, 10, 580, 1.0f);

        // Player shoot
        if (glfwGetMouseButton(renderer.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            glm::vec3 shootDir = glm::normalize(camera.front);
            weapon.shoot(projectiles, player.pos + glm::vec3(0,1,0), shootDir);
        }

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

        renderer.endFrame();
    }

    renderer.shutdown();
    return 0;
}
