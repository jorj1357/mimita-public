// C:\important\quiet\n\mimita-public\mimita-public\src\main.cpp
// dec 17 2025 new update but not made dec 17 2025
// dec 18 2025 cleaned minimal comments

/**
 * purpose
 * only calls other files functions
 * does no math here
 * todo cleanup bc like 50% is just comments 
 * also can we just include a file that has all the includes?
 * so includes arent giant 
 */

#include "world/world-loader.h"
#include "world/world-mesh.h"

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE

#include "renderer/renderer.h"
extern Renderer* gRenderer;

#include "map/map_common.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "entities/player.h"

#include <vector>
#include <algorithm>
#include <cstdlib>  
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

#include "utils/mesh_utils.h"

// dec 16 2025 use config everwhere 
#include "physics/config.h"
// this is for the hot loader that we dont need right now dec 18 2025 
// #include "physics/config-loader.h"
#include "debug/debug-status.h"

#include "world/world.h"

World world;


TextureManager TEX; 
GLuint groundTex;

bool editMode = false;



Enemy enemy;
Weapon weapon;
std::vector<Projectile> projectiles;
GLuint createMapVAO(const Mesh&);

void drawMap(const Mesh& mesh);

Camera* activeCamera = nullptr;

Renderer* gRenderer = nullptr;
GLuint gMainShaderProgram = 0;

int main() {
    srand((unsigned)time(NULL));

    if (!loadWorldFromJSON(
            world,
            // "assets/maps/json-converts/mimita-simple-collisions-apartment-v3-converted.json"
            "assets/maps/json-converts/mimita-block-sphere-limit-v1-converted.json"
        )) { 
        return -1;
    }

    world.rebuildChunks();

    std::vector<WorldVertex> worldVerts;
    buildWorldMesh(world, worldVerts);

    GLuint worldVAO, worldVBO;
    glGenVertexArrays(1, &worldVAO);
    glGenBuffers(1, &worldVBO);

    glBindVertexArray(worldVAO);
    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        worldVerts.size() * sizeof(WorldVertex),
        worldVerts.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WorldVertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE,
        sizeof(WorldVertex),
        (void*)offsetof(WorldVertex, uv)
    );
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    Renderer renderer(800, 600, "mimita.exe");
    gRenderer = &renderer;

    TEX.init(); 
    // Draw map with my texture yay
    GLuint shaderProgram = renderer.getShaderProgram();  
    printf("Window pointer: %p\n", renderer.window);
    if (!renderer.window) return -1;

    // dec 18 2025 make sure the origin is at the feet 
    Mesh playerMesh = loadOBJ("assets/entity/player/default/mimita-char-concise-v3.obj");
    glm::vec3 meshMin, meshMax;
    computeMeshBounds(playerMesh, meshMin, meshMax);

    GLuint playerVAO = createMapVAO(playerMesh);
    GLuint playerTex = loadTexture("assets/textures/greenwirev1.png");

    printf("Loading map...\n");

    // ---------------------
    // map picker section begin  
    // ---------------------

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
    // no maps plz 
    // Mesh map = loadOBJ("assets/maps/mimita-apartments-final-v2.obj");

    // ---------------------
    // map picker section end  
    // ---------------------

    // do this so that the map actually has data for us to walk on dec 16 2025
    // ---- BAKE MAP TRANSFORM FOR PHYSICS ----
    glm::mat4 mapTransform = glm::mat4(1.0f);

    // example values – adjust if you move/scale the map
    mapTransform = glm::scale(mapTransform, glm::vec3(1.0f));
    mapTransform = glm::translate(mapTransform, glm::vec3(0.0f));
    mapTransform = glm::rotate(mapTransform, 0.0f, glm::vec3(0,1,0));

    // for (auto& v : map.verts) {
    //     glm::vec4 p = mapTransform * glm::vec4(v.pos, 1.0f);
    //     v.pos = glm::vec3(p);
    // }

    //     glm::vec3 min(1e9f), max(-1e9f);
    // for (auto& v : map.verts) {
    //     min = glm::min(min, v.pos);
    //     max = glm::max(max, v.pos);
    // }
    // fprintf(stderr, "MAP AABB min(%f %f %f) max(%f %f %f)\n",
    //         min.x, min.y, min.z, max.x, max.y, max.z);

    // if (map.verts.empty()) {
    //     fprintf(stderr, "Map failed to load or has 0 verts.\n");
    //     return -1;
    // }
    // GLuint mapVAO = createMapVAO(map);
    
    // NOW we do world chunking ...? dec 17 2025
    // world.buildFromMesh(map);

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    Player player;
    Camera camera;
    activeCamera = &camera;
    glfwSetCursorPosCallback(renderer.window, [](GLFWwindow*, double x, double y) {
        if (activeCamera) activeCamera->updateMouse(x, y);
    });

    while (!renderer.shouldClose()) {
        float dt = renderer.beginFrame();

        camera.follow(player.pos);
        player.yaw = camera.yaw;       
        glm::mat4 view = camera.getView();
        // can we make it not 800 x 600 plz i hate this tiny little chud window 
        // dec 16 2025 
        glm::mat4 proj = camera.getProj(800.0f, 600.0f);

        // simple floor collision BEFORE rendering stuff
        updatePhysics(player, world, renderer.window, dt, camera);

        player.render(shaderProgram, playerVAO, playerMesh.verts.size(), view, proj, camera, playerTex);

        // old value 
        // char hud[128];
        char hud[32];
        snprintf(hud, sizeof(hud), "Speed, hp, gui goes here. debug: T = up, G = forward, B = down, R = reset");
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
        // drawMap(mapVAO, map.verts.size());

        glUniform1i(glGetUniformLocation(shaderProgram,"useTex"), false);
        glBindVertexArray(0);
        glUseProgram(0);
        glBindVertexArray(0);
        glUseProgram(0);

        enemy.draw(renderer, view, proj);
        for (auto& p : projectiles) p.draw(renderer, view, proj);

        // hot reload
        // dec 18 2025 dont add yet
        // hotReloadPhysicsConfig();

        renderer.endFrame();
    }

    renderer.shutdown();
    return 0;
}
