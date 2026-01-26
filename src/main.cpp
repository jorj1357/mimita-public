// C:\important\quiet\n\mimita-public-main\src\main.cpp
// jan 25 2026 3rd refacotr
// purpose is only to call other functions from other files thats it

#include <cstdio>
#include <vector>
#include <ctime>
#include <algorithm>

#include "renderer/renderer.h"
#include "camera.h"
#include "map/texture_manager.h"
#include "map/map_loader.h"
#include "map/map_render.h"
#include "world/world-mesh.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "weapons/weapon.h"
#include "weapons/projectile.h"
#include "debug/debug-visuals.h"

#include "world/world-loader.h"
#include "map/texture.h"
#include "physics/physics.h"

// ---------------- Globals ----------------
Renderer* gRenderer = nullptr;
Camera*   gActiveCamera = nullptr;
std::vector<Projectile> projectiles;
GLuint worldShader = 0;

// ---------------- Shader loader ----------------
#include <fstream>
#include <sstream>

static std::string readFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        printf("FAILED TO OPEN %s\n", path);
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        printf("SHADER ERROR:\n%s\n", log);
        return 0;
    }
    return s;
}

static GLuint loadShader(const char* vsPath, const char* fsPath) {
    auto vsSrc = readFile(vsPath);
    auto fsSrc = readFile(fsPath);
    if (vsSrc.empty() || fsSrc.empty()) return 0;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc.c_str());
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc.c_str());

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------- Input ----------------
static void mouseCallback(GLFWwindow*, double x, double y) {
    if (gActiveCamera)
        gActiveCamera->updateMouse(x, y);
}

// ---------------- Main ----------------
int main() {
    printf("MAIN START\n");
    srand((unsigned)time(nullptr));

    // ---- Renderer (owns GL) ----
    Renderer renderer(800, 600, "mimita.exe");
    gRenderer = &renderer;

    if (!renderer.window) return -1;

    // ---- Shaders ----
    worldShader = loadShader("shaders/basic.vert", "shaders/basic.frag");
    if (!worldShader) {
        printf("World shader failed\n");
        return -1;
    }

    printf("World shader loaded\n");

    // ---- Game systems ----
    World world;
    TextureManager TEX;
    Enemy enemy;
    Weapon weapon;

    printf("Initializing textures\n");
    TEX.init();

    // ---- Bind textures to shader ----
    glUseProgram(worldShader);
    int samplers[16];
    for (int i = 0; i < 16; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, TEX.get(i));
        samplers[i] = i;
    }
    glUniform1iv(glGetUniformLocation(worldShader,"uTextures"), 16, samplers);
    glUseProgram(0);

    // ---- Load world ----
    if (!loadWorldFromJSON(
        world,
        TEX,
        "assets/maps/json-converts/mimita-rotations-v6-converted.json"))
    {
        return -1;
    }

    std::vector<WorldVertex> worldVerts;
    buildWorldMesh(world, worldVerts);

    GLuint worldVAO, worldVBO;
    glGenVertexArrays(1, &worldVAO);
    glGenBuffers(1, &worldVBO);

    glBindVertexArray(worldVAO);
    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, worldVerts.size()*sizeof(WorldVertex), worldVerts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(WorldVertex),(void*)offsetof(WorldVertex,pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(WorldVertex),(void*)offsetof(WorldVertex,uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(WorldVertex),(void*)offsetof(WorldVertex,texIndex));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // ---- Player ----
    Mesh playerMesh = loadOBJ("assets/entity/player/default/mimita-char-concise-v3.obj");
    GLuint playerVAO = createMapVAO(playerMesh);
    GLuint playerTex = loadTexture("assets/textures/greenwirev1.png");

    Player player;
    Camera camera;
    gActiveCamera = &camera;

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(renderer.window, mouseCallback);

    // ---- Loop ----
    while (!renderer.shouldClose()) {
        float dt = renderer.beginFrame();

        DebugVis::update();
        camera.follow(player.pos);

        glm::mat4 view = camera.getView();
        glm::mat4 proj = camera.getProj(800,600);

        // World
        glUseProgram(worldShader);
        glUniformMatrix4fv(glGetUniformLocation(worldShader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(worldShader,"projection"),1,0,&proj[0][0]);
        glm::mat4 model(1);
        glUniformMatrix4fv(glGetUniformLocation(worldShader,"model"),1,0,&model[0][0]);

        glBindVertexArray(worldVAO);
        glDrawArrays(GL_TRIANGLES, 0, worldVerts.size());
        glBindVertexArray(0);
        glUseProgram(0);

        updatePhysics(player, world, renderer.window, dt, camera);

        player.render(worldShader, playerVAO, playerMesh.verts.size(), view, proj, camera, playerTex);

        weapon.update(dt);
        enemy.update(dt);

        renderer.endFrame();
    }

    return 0;
}
