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

// C:\important\quiet\n\mimita-public\mimita-public\src\main.cpp
// cleaned dec 18 2025
// purpose: high-level orchestration only

// dec 19 2025 why does this little tinsmall chud game use 100% of gpu need to optimize
// and see ifits even in this file 

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE

// System/external headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

// Local headers alphabetical order 
#include "camera.h"
#include "debug/debug-visuals.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "map/map_common.h"
#include "map/map_loader.h"
#include "map/map_render.h"
#include "map/texture.h"
#include "map/texture_manager.h"
#include "physics/config.h"
#include "physics/physics.h"
#include "renderer/renderer.h"
#include "utils/mesh_utils.h"
#include "weapons/projectile.h"
#include "weapons/weapon.h"


extern Renderer* gRenderer;


// -------------------- Globals --------------------
Renderer* gRenderer = nullptr;
Camera*   gActiveCamera = nullptr;

World world;
TextureManager TEX;

Enemy enemy;
Weapon weapon;
std::vector<Projectile> projectiles;

GLuint groundTex = 0;

// -------------------- Forward Decls --------------------
GLuint createMapVAO(const Mesh&);

// -------------------- Input --------------------
static void mouseCallback(GLFWwindow*, double x, double y)
{
    if (gActiveCamera)
        gActiveCamera->updateMouse(x, y);
}

// -------------------- Main --------------------
int main()
{
    printf("MAIN START\n");
    fflush(stdout);
    srand((unsigned)time(nullptr));

    // -------- Renderer FIRST --------
    static Renderer renderer(1200, 800, "mimita.exe");
    gRenderer = &renderer;

    if (!renderer.window)
        return -1;
    
    DebugVis::init(renderer.window);

    TEX.init();

    GLuint shader = renderer.getShaderProgram();

    glUseProgram(shader);

    int samplers[16];
    for (int i = 0; i < 16; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, TEX.get(i));
        samplers[i] = i;
    }

    glUniform1iv(
        glGetUniformLocation(shader, "uTextures"),
        16,
        samplers
    );

    glUseProgram(0);
    
    printf("loaded renderr\n");

    // -------- THEN Load world --------
    if (!loadWorldFromJSON(
            world,
            // forward slash not back, /, not \, 
            // mimita-rotations-v3-converted.json
            "assets/maps/json-converts/mimita-rotations-v6-converted.json"))
            // "assets/maps/json-converts/mimita-rotations-v5-converted.json"))
            // "assets/maps/json-converts/mimita-rotations-v4-converted.json"))
            // "assets/maps/json-converts/mimita-rotations-v3-converted.json"))
            // "assets/maps/json-converts/mimita-rotations-test-v1-converted.json"))
            // "assets/maps/json-converts/mimita-block-sphere-limit-v1-converted.json"))
    {
        return -1;
    }

    world.rebuildChunks();
    printf("loaded world\n");

    // -------- Build world mesh --------
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

    // position
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(WorldVertex),
        (void*)offsetof(WorldVertex, pos)
    );
    glEnableVertexAttribArray(0);

    // uv
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        sizeof(WorldVertex),
        (void*)offsetof(WorldVertex, uv)
    );
    glEnableVertexAttribArray(1);

    // texture index
    glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE,
        sizeof(WorldVertex),
        (void*)offsetof(WorldVertex, texIndex)
    );
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    printf("loaded world json\n");

    // -------- Player --------
    Mesh playerMesh = loadOBJ(
        "assets/entity/player/default/mimita-char-concise-v3.obj"
    );

    GLuint playerVAO = createMapVAO(playerMesh);
    GLuint playerTex = loadTexture("assets/textures/greenwirev1.png");

    Player player;
    Camera camera;
    gActiveCamera = &camera;

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(renderer.window, mouseCallback);

    printf("loaded plr\n");

    // ==================== LOOP ====================
    while (!renderer.shouldClose())
    {
        float dt = renderer.beginFrame();
        DebugVis::update();

        // ---- Camera ----
        camera.follow(player.pos);
        player.yaw = camera.yaw;

        glm::mat4 view = camera.getView();
        // dec 18 2025 these dont match the render window on purpose, 
        // testing stretched res
        glm::mat4 proj = camera.getProj(800.0f, 600.0f);

        // ---- Render World ----
        glUseProgram(shader);

        glm::mat4 worldModel(1.0f);

        glUniformMatrix4fv(
            glGetUniformLocation(shader, "model"),
            1, GL_FALSE, &worldModel[0][0]
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shader, "view"),
            1, GL_FALSE, &view[0][0]
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shader, "projection"),
            1, GL_FALSE, &proj[0][0]
        );

        glUniform1i(glGetUniformLocation(shader, "useTex"), false);
        glUniform3f(glGetUniformLocation(shader, "color"), 0.7f, 0.7f, 0.75f);

        glBindVertexArray(worldVAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)worldVerts.size());
        glBindVertexArray(0);

        glUseProgram(0);

        // ---- Physics ----
        updatePhysics(player, world, renderer.window, dt, camera);

        // ---- Render Player ----
        player.render(
            shader,
            playerVAO,
            playerMesh.verts.size(),
            view,
            proj,
            camera,
            playerTex
        );

        // ---- HUD / Debug ----
        // if (gDebugStatusTimer > 0.0f)
        // {
        //     drawText2D(
        //         gDebugStatusText.c_str(),
        //         10,
        //         550,
        //         1.0f
        //     );
        //     gDebugStatusTimer -= dt;
        // }

        // drawText2D(
        //     "Speed, hp, gui | T up G fwd B down R reset",
        //     10,
        //     580,
        //     1.0f
        // );

        // ---- Entities ----
        weapon.update(dt);
        enemy.update(dt);

        for (auto& p : projectiles)
            p.update(dt);

        projectiles.erase(
            std::remove_if(
                projectiles.begin(),
                projectiles.end(),
                [](const Projectile& p) { return p.life <= 0; }
            ),
            projectiles.end()
        );

        enemy.draw(renderer, view, proj);
        for (auto& p : projectiles)
            p.draw(renderer, view, proj);

        renderer.endFrame();
    }

    // renderer.shutdown();
    return 0;
}
