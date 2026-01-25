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
#include "world/world-loader.h"
#include "world/world-mesh.h"


extern Renderer* gRenderer;


// -------------------- Globals --------------------
Renderer* gRenderer = nullptr;
Camera*   gActiveCamera = nullptr;

// jan 5 2026 debug test
// World world;
// TextureManager TEX;

// Enemy enemy;
// Weapon weapon;

// jan 5 2026 moving them into the render 1200 800 loop 
// World world;
// TextureManager TEX;
// Enemy enemy;
// Weapon weapon;

std::vector<Projectile> projectiles;

GLuint groundTex = 0;
GLuint worldShader = 0;

// -------------------- Forward Decls --------------------
GLuint createMapVAO(const Mesh&);

// -------------------- Input --------------------
static void mouseCallback(GLFWwindow*, double x, double y)
{
    if (gActiveCamera)
        gActiveCamera->updateMouse(x, y);
}

/**
 * jan 25 2026
 * i know this make the file messies 
 * but need to just fix this then clean it later ?
 * i think idk 
 */

 #include <fstream>
#include <sstream>
#include <string>

static std::string readFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("FAILED TO OPEN %s\n", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        printf("SHADER COMPILE ERROR:\n%s\n", log);
        return 0;
    }
    return s;
}

static GLuint loadShader(const char* vertPath, const char* fragPath)
{
    std::string vsSrc = readFile(vertPath);
    std::string fsSrc = readFile(fragPath);

    if (vsSrc.empty() || fsSrc.empty())
        return 0;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc.c_str());
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc.c_str());
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        printf("SHADER LINK ERROR:\n%s\n", log);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// -------------------- Main --------------------
int main()
{
    printf("MAIN START\n");
    fflush(stdout);
    printf("fflush \n");
    srand((unsigned)time(nullptr));
    printf("null ptr\n");

    // -------- Renderer FIRST --------
    printf("abt to load renderer \n");
    // static Renderer renderer(1200, 800, "mimita.exe");
    // testing jan 5 2026 to m atch the real thing 
    static Renderer renderer(800, 600, "mimita.exe");
    gRenderer = &renderer;

    // tri this jan 5 2026 
    // move before world tex enem weapon etc fix 2 
    glfwMakeContextCurrent(renderer.window);   // ← REQUIRED

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("FAILED TO INIT GLAD\n");
        return -1;
    }
    printf("GLAD OK\n");

    worldShader = loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag"
    );

    if (!worldShader) {
        printf("FAILED TO LOAD WORLD SHADER\n");
        return -1;
    }

    printf("World shader loaded\n");

    // jan 5 2026 moved them here 
    // fix 3 printfs 
    printf("before World\n");
    World world;
    printf("after World\n");

    printf("before TextureManager\n");
    TextureManager TEX;
    printf("after TextureManager\n");

    printf("before Enemy\n");
    Enemy enemy;
    printf("after Enemy\n");

    printf("before Weapon\n");
    Weapon weapon;
    printf("after Weapon\n");

    printf("render static load \n");
    printf("renderer.window = %p\n", (void*)renderer.window);
    fflush(stdout);
    printf("fflush stoud\n");

    printf("render window attempt\n");
    if (!renderer.window)
        return -1;
    printf("render returned -1 \n");
    printf("tetx init atempt \n");

    // add this before tex init ? jan 25 2026 
    GLuint texVAO;
    glGenVertexArrays(1, &texVAO);
    glBindVertexArray(texVAO);

    TEX.init();
    printf("tex init works \n");

    glUseProgram(worldShader);

    int samplers[16];
    for (int i = 0; i < 16; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, TEX.get(i));
        samplers[i] = i;
    }

    glUniform1iv(
        glGetUniformLocation(worldShader, "uTextures"),
        16,
        samplers
    );

    glUseProgram(0);
    
    printf("loaded renderr v2 with worldshader not just shader\n");

    // -------- THEN Load world --------
    if (!loadWorldFromJSON(
            // whi its a asterisk before that fix it idk jan 5 2026 
            world,
            TEX,
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

    // are we alreadu doing this? commet jan 5 2026 
    // we do it in world-loader.cpp 
    // world.rebuildChunks();
   
    printf("loaded world\n");

    // -------- Build world mesh --------
    std::vector<WorldVertex> worldVerts;
    buildWorldMesh(world, worldVerts);
            printf("loaded buildworldmesh\n");


    GLuint worldVAO, worldVBO;
    glGenVertexArrays(1, &worldVAO);
    glGenBuffers(1, &worldVBO);

    glBindVertexArray(worldVAO);
    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);

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

            printf("loaded plr textrue\n");


    Player player;
    Camera camera;
    gActiveCamera = &camera;

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(renderer.window, mouseCallback);

    printf("loaded plr\n");

    // idk wher  put these jan 5 2026 
    // Put this after context creation, not inside VAO setup:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // temporarily, for sanity

    // ==================== LOOP ====================
    while (!renderer.shouldClose())
    {
        float dt = renderer.beginFrame();
        // DEBUG is it even showing antthing 
        // glViewport(0, 0, 1200, 800);
        // jan 5 2026 testing to match it
        glViewport(0, 0, 800, 600);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        DebugVis::update();

        // ---- Camera ----
        camera.follow(player.pos);
        player.yaw = camera.yaw;

        glm::mat4 view = camera.getView();
        // dec 18 2025 these dont match the render window on purpose, 
        // testing stretched res
        printf("attempt 800 x 600 camera \n");

        glm::mat4 proj = camera.getProj(800.0f, 600.0f);
        printf(" 800 x 600 camera good \n");


        // ---- Render World ----
        glUseProgram(worldShader);

        glm::mat4 worldModel(1.0f);

        glUniformMatrix4fv(
            glGetUniformLocation(worldShader, "model"),
            1, GL_FALSE, &worldModel[0][0]
        );

        glUniformMatrix4fv(
            glGetUniformLocation(worldShader, "view"),
            1, GL_FALSE, &view[0][0]
        );

        glUniformMatrix4fv(
            glGetUniformLocation(worldShader, "projection"),
            1, GL_FALSE, &proj[0][0]
        );

        // debug comment 
        // glUniform1i(glGetUniformLocation(shader, "useTex"), false);
        // glUniform3f(glGetUniformLocation(shader, "color"), 0.7f, 0.7f, 0.75f);

        glBindVertexArray(worldVAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)worldVerts.size());
        glBindVertexArray(0);

        glUseProgram(0);
                printf(" gluseprogram stuff good \n");


        // ---- Physics ----
        updatePhysics(player, world, renderer.window, dt, camera);
                        printf(" update phsics good  \n");


        // ---- Render Player ----
        player.render(
            worldShader,
            playerVAO,
            playerMesh.verts.size(),
            view,
            proj,
            camera,
            playerTex
        );
        printf(" render plr good  \n");

                printf(" attempt to debug vis enable  \n");

        if (DebugVis::enabled()) {
            const auto& C = DebugVis::colors();

            glm::vec3 start = camera.pos;
            glm::vec3 end   = camera.pos + camera.front * 2.0f;

            gRenderer->drawLine(
                start,
                end,
                C.lookVector,
                view,
                proj
            );
        }
                printf(" debug vis good  \n");

                        printf(" attempt to debug vis enable 2  \n");

        if (DebugVis::enabled()) {
            const auto& C = DebugVis::colors();

            for (auto& [coord, chunk] : world.chunks) {
                glm::vec3 min(
                    coord.x * world.chunkSize,
                    coord.y * world.chunkSize,
                    0.0f
                );

                glm::vec3 max = min + glm::vec3(
                    world.chunkSize,
                    world.chunkSize,
                    CHUNK_SIZE
                );

                // draw a few edges only (keep it simple)
                gRenderer->drawLine(min, {max.x, min.y, min.z}, C.worldChunks, view, proj);
                gRenderer->drawLine(min, {min.x, max.y, min.z}, C.worldChunks, view, proj);
            }
        }
        printf(" attempt to debug vis enable GOOD 2  \n");


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
                printf(" weapon and enemi update good \n");


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

                        printf(" projectile update good \n");

        enemy.draw(renderer, view, proj);
        for (auto& p : projectiles)
            p.draw(renderer, view, proj);
                        printf(" render end frame attempt \n");

        renderer.endFrame();
        printf(" render end frame GOOD  \n");

    }

    // renderer.shutdown();
            printf(" return 0 attempt  \n");

    return 0;
                printf(" return 0 GOOD   \n");

}
