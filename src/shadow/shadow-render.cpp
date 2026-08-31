#include "shadow-render.h"
#include "shadow-config.h"

#include <cstdio>
#include <cmath>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/render-world.h"
#include "render/lighting-config.h"
#include "renderer/renderer.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "effects/hit-effects.h"
#include "effects/effect-part.h"
#include "perf/perf.h"

extern Renderer* gRenderer;
extern Player* gpPlayer;
extern NpcSystem* gpNpcSystem;

namespace {

GLuint gShadowFbo = 0;
GLuint gShadowDepthTex = 0;
int gShadowMapSize = 0;

GLuint gShadowShader = 0;
glm::mat4 gShadowMatrix(1.0f);
glm::mat4 gLightMVP(1.0f);

bool gShowShadowMap = false;

// Shared sphere VAO for effect/particle shadow rendering
GLuint gSphereVao = 0;
GLuint gSphereVbo = 0;
int gSphereVertCount = 0;

static void ensureSphereVao()
{
    if (gSphereVao) return;

    int sectors = 12;
    int stacks = 8;
    std::vector<glm::vec3> verts;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = (float)(3.14159265 * (double)i / (double)stacks);
        float xy = sinf(stackAngle);
        float z = cosf(stackAngle);
        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = (float)(2.0 * 3.14159265 * (double)j / (double)sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            verts.push_back(glm::vec3(x, y, z));
        }
    }

    std::vector<glm::vec3> triangles;
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;
        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                triangles.push_back(verts[k1]);
                triangles.push_back(verts[k2]);
                triangles.push_back(verts[k1 + 1]);
            }
            if (i != stacks - 1) {
                triangles.push_back(verts[k1 + 1]);
                triangles.push_back(verts[k2]);
                triangles.push_back(verts[k2 + 1]);
            }
        }
    }

    glGenVertexArrays(1, &gSphereVao);
    glGenBuffers(1, &gSphereVbo);
    glBindVertexArray(gSphereVao);
    glBindBuffer(GL_ARRAY_BUFFER, gSphereVbo);
    glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(glm::vec3), triangles.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);

    gSphereVertCount = (int)triangles.size();
}

static GLuint createShader(const char* vertSrc, const char* fragSrc)
{
    auto compile = [](GLuint shader, const char* src) -> bool {
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            printf("[SHADOW SHADER] compile error: %s\n", log);
            return false;
        }
        return true;
    };

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!compile(vs, vertSrc) || !compile(fs, fragSrc)) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        printf("[SHADOW SHADER] link error: %s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static bool loadShadowShaders()
{
    auto readFile = [](const char* path) -> std::string {
        FILE* f = fopen(path, "rb");
        if (!f) return {};
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string s((size_t)len, '\0');
        fread(s.data(), 1, (size_t)len, f);
        fclose(f);
        return s;
    };

    const char* vertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uLightMVP;
void main() {
    gl_Position = uLightMVP * vec4(aPos, 1.0);
}
)";

    const char* fragSrc = R"(
#version 330 core
void main() {
}
)";

    std::string vertFile = readFile("shaders/shadow.vert");
    std::string fragFile = readFile("shaders/shadow.frag");

    const char* vert = vertFile.empty() ? vertSrc : vertFile.c_str();
    const char* frag = fragFile.empty() ? fragSrc : fragFile.c_str();

    gShadowShader = createShader(vert, frag);
    if (!gShadowShader) {
        printf("[SHADOWS] Failed to load shadow shader, trying embedded fallback\n");
        gShadowShader = createShader(vertSrc, fragSrc);
    }
    return gShadowShader != 0;
}

} // namespace

void initShadowMap(int mapSize)
{
    if (mapSize < 16) mapSize = 16;
    if (mapSize == gShadowMapSize && gShadowFbo) return;

    if (gShadowFbo) {
        glDeleteFramebuffers(1, &gShadowFbo);
        glDeleteTextures(1, &gShadowDepthTex);
    }

    gShadowMapSize = mapSize;

    glGenTextures(1, &gShadowDepthTex);
    glBindTexture(GL_TEXTURE_2D, gShadowDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mapSize, mapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &gShadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gShadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gShadowDepthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("[SHADOWS] FBO incomplete: 0x%x\n", status);
        glDeleteFramebuffers(1, &gShadowFbo);
        glDeleteTextures(1, &gShadowDepthTex);
        gShadowFbo = 0;
        gShadowDepthTex = 0;
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    printf("[SHADOWS] Shadow map FBO created: %dx%d\n", mapSize, mapSize);

    if (!gShadowShader) {
        loadShadowShaders();
    }
}

void renderShadowMap(const World& world, const glm::vec3& focusPoint)
{
    const auto& cfg = ShadowConfig::instance();
    if (!cfg.enabled()) return;

    int mapSize = cfg.shadowMapSize();
    if (mapSize < 16) return;

    if (!gShadowShader) {
        if (!loadShadowShaders()) return;
    }

    initShadowMap(mapSize);
    if (!gShadowFbo) return;

    const auto& lcfg = LightingConfig::instance();
    glm::vec3 lightDir = lcfg.lightDir();
    float distance = cfg.shadowDistance();

    updateShadowMatrix(lightDir, distance, focusPoint, mapSize, cfg.stabilize());

    glBindFramebuffer(GL_FRAMEBUFFER, gShadowFbo);
    glViewport(0, 0, mapSize, mapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // World depth
    if (cfg.worldReceivesShadows() || true) // world always casts into the map
        renderWorldDepth(world, gShadowShader, gLightMVP);

    // Player depth
    if (cfg.playersCastShadows() && gpPlayer)
        renderPlayerDepth(*gpPlayer, gShadowShader, gLightMVP);

    // NPC depth
    if (cfg.npcsCastShadows() && gpNpcSystem)
        renderNpcDepths(gShadowShader, gLightMVP);

    // Effect depth
    if (cfg.effectsCastShadows())
        renderEffectDepths(gShadowShader, gLightMVP);

    // Particle depth
    if (cfg.particlesCastShadows())
        renderParticleDepths(gShadowShader, gLightMVP);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderPlayerDepth(const Player& player, GLuint shadowShader, const glm::mat4& lightViewProj)
{
    Perf::state().renderPerf.shadowBatches++;
    player.renderDepth(shadowShader, lightViewProj);
}

void renderNpcDepths(GLuint shadowShader, const glm::mat4& lightViewProj)
{
    if (!gpNpcSystem) return;
    for (const Npc& npc : gpNpcSystem->all()) {
        Perf::state().renderPerf.shadowBatches++;
        npc.body.renderDepth(shadowShader, lightViewProj);
    }
}

void renderEffectDepths(GLuint shadowShader, const glm::mat4& lightViewProj)
{
    ensureSphereVao();

    glUseProgram(shadowShader);
    GLint loc = glGetUniformLocation(shadowShader, "uLightMVP");
    if (loc < 0) { glUseProgram(0); return; }
    glBindVertexArray(gSphereVao);

    float cutoffAlpha = ShadowConfig::instance().effectShadowCutoffAlpha();

    static constexpr int MAX_BURSTS = 64;
    static HitBurstSnapshot bursts[MAX_BURSTS];
    int count = HitEffects::collectBurstSnapshots(bursts, MAX_BURSTS);

    for (int i = 0; i < count; ++i) {
        const auto& b = bursts[i];
        if (!b.alive) continue;
        Perf::state().renderPerf.shadowBatches++;

        float progress = b.totalTicks > 0 ? (float)b.ageTicks / (float)b.totalTicks : 0.0f;
        float baseRadius = 0.3f + progress * 0.5f;
        glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), b.position), glm::vec3(baseRadius));
        glm::mat4 mvp = lightViewProj * model;
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, gSphereVertCount);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void renderParticleDepths(GLuint shadowShader, const glm::mat4& lightViewProj)
{
    ensureSphereVao();

    glUseProgram(shadowShader);
    GLint loc = glGetUniformLocation(shadowShader, "uLightMVP");
    if (loc < 0) { glUseProgram(0); return; }
    glBindVertexArray(gSphereVao);

    float cutoffAlpha = ShadowConfig::instance().effectShadowCutoffAlpha();

    static constexpr int MAX_PARTS = 200;
    static EffectPartSystem::PartSnapshot parts[MAX_PARTS];
    int count = EffectPartSystem::instance().collectAlive(parts, MAX_PARTS, cutoffAlpha);

    for (int i = 0; i < count; ++i) {
        const auto& p = parts[i];
        float radius = p.scale * 0.5f;
        if (radius < 0.01f) continue;
        Perf::state().renderPerf.shadowBatches++;

        glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), p.position), glm::vec3(radius));
        glm::mat4 mvp = lightViewProj * model;
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, gSphereVertCount);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void bindShadowMap(int textureUnit)
{
    if (gShadowDepthTex) {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, gShadowDepthTex);
    }
}

void updateShadowMatrix(const glm::vec3& lightDir, float distance, const glm::vec3& focusPoint, int mapSize, bool stabilize)
{
    glm::vec3 L = glm::normalize(-lightDir);
    glm::vec3 lightPos = focusPoint + L * distance;

    if (stabilize) {
        float texelWorld = (2.0f * distance) / mapSize;
        lightPos.x = roundf(lightPos.x / texelWorld) * texelWorld;
        lightPos.y = roundf(lightPos.y / texelWorld) * texelWorld;
        lightPos.z = roundf(lightPos.z / texelWorld) * texelWorld;
    }

    glm::mat4 lightView = glm::lookAt(lightPos, focusPoint, glm::vec3(0, 0, 1));
    glm::mat4 lightProj = glm::ortho(-distance, distance, -distance, distance, 0.0f, distance * 2.0f);

    glm::mat4 bias(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f
    );

    gLightMVP = lightProj * lightView;
    gShadowMatrix = bias * lightProj * lightView;
}

const glm::mat4& shadowMatrix()
{
    return gShadowMatrix;
}

GLuint shadowDepthTex()
{
    return gShadowDepthTex;
}

void setShowShadowMap(bool v)
{
    gShowShadowMap = v;
}

bool showShadowMap()
{
    return gShowShadowMap;
}

void renderShadowMapOverlay(int screenW, int screenH)
{
    if (!gShowShadowMap || !gShadowDepthTex) return;

    int size = std::min(screenW, screenH) / 3;
    int x = screenW - size - 10;
    int y = 70;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    GLuint shader = gRenderer ? gRenderer->shaderProgram : 0;
    if (!shader) return;

    float verts[] = {
        (float)x,          (float)y,           0.0f, 0.0f,
        (float)(x + size), (float)y,           1.0f, 0.0f,
        (float)(x + size), (float)(y + size),  1.0f, 1.0f,
        (float)x,          (float)y,           0.0f, 0.0f,
        (float)(x + size), (float)(y + size),  1.0f, 1.0f,
        (float)x,          (float)(y + size),  0.0f, 1.0f,
    };

    static GLuint vao = 0, vbo = 0;
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uDebugView"), 3);
    glm::mat4 id(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &id[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &id[0][0]);
    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &ortho[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gShadowDepthTex);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}
