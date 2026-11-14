#include "shadow-render.h"
#include "shadow/shadow-shader.h"
#include "game/resident-state-module.h"
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
#include "render/render-command.h"
#include "render/world-shader.h"
#include "game/gl-resource-bridge-module.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "effects/hit-effects.h"
#include "effects/effect-part.h"
#include "perf/perf.h"

extern Renderer* gRenderer;

namespace r = mimita::game::render;

namespace {

std::uint32_t gShadowFbo = 0;
std::uint32_t gShadowDepthTex = 0;
int gShadowMapSize = 0;

glm::mat4 gShadowMatrix(1.0f);
glm::mat4 gLightMVP(1.0f);

bool gShowShadowMap = false;

// Shared sphere VAO for effect/particle shadow rendering
std::uint32_t gSphereVao = 0;
std::uint32_t gSphereVbo = 0;
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

    gSphereVao = mimita::game::genVertexArray();
    gSphereVbo = mimita::game::genBuffer();
    r::bindVertexArray(gSphereVao);
    r::bindBuffer(r::BufferTarget::Array, gSphereVbo);
    r::bufferData(r::BufferTarget::Array, gSphereVbo, triangles.data(),
                  static_cast<std::uint32_t>(triangles.size() * sizeof(glm::vec3)),
                  r::BufferUsage::Static);
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(glm::vec3), 0);
    r::bindVertexArray(0);

    gSphereVertCount = (int)triangles.size();
}

} // namespace

void initShadowMap(int mapSize)
{
    if (mapSize < 16) mapSize = 16;
    if (mapSize == gShadowMapSize && gShadowFbo) return;

    if (gShadowFbo) {
        mimita::game::deleteFramebuffer(gShadowFbo);
        mimita::game::deleteTexture(gShadowDepthTex);
    }

    gShadowMapSize = mapSize;

    gShadowDepthTex = mimita::game::genTexture();
    r::bindTexture(r::TextureTarget::Texture2D, gShadowDepthTex);
    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::Depth24,
                  mapSize, mapSize, 0, r::PixelFormat::Depth,
                  r::PixelType::Float, nullptr);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                     r::TextureValue::Linear);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                     r::TextureValue::Linear);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                     r::TextureValue::ClampToBorder);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                     r::TextureValue::ClampToBorder);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    r::texParameterfv(r::TextureTarget::Texture2D,
                      r::TextureParameter::BorderColor, borderColor);

    gShadowFbo = mimita::game::genFramebuffer();
    r::bindFramebuffer(r::FramebufferTarget::Framebuffer, gShadowFbo);
    r::framebufferTexture2D(r::TextureTarget::Texture2D,
                            r::FramebufferAttachment::Depth, gShadowDepthTex);
    r::drawBuffer(r::FramebufferBuffer::None);
    r::readBuffer(r::FramebufferBuffer::None);

    if (!r::checkFramebufferStatus()) {
        printf("[SHADOWS] FBO incomplete\n");
        mimita::game::deleteFramebuffer(gShadowFbo);
        mimita::game::deleteTexture(gShadowDepthTex);
        gShadowFbo = 0;
        gShadowDepthTex = 0;
        return;
    }
    r::bindFramebuffer(r::FramebufferTarget::Framebuffer, 0);
    printf("[SHADOWS] Shadow map FBO created: %dx%d\n", mapSize, mapSize);

    r::shadowShader().ensureLoaded();
}

void renderShadowMap(const World& world, const glm::vec3& focusPoint)
{
    const auto& cfg = ShadowConfig::instance();
    if (!cfg.enabled()) return;

    int mapSize = cfg.shadowMapSize();
    if (mapSize < 16) return;

    if (!r::shadowShader().ensureLoaded()) return;

    initShadowMap(mapSize);
    if (!gShadowFbo) return;

    const auto& lcfg = LightingConfig::instance();
    glm::vec3 lightDir = lcfg.lightDir();
    float distance = cfg.shadowDistance();

    updateShadowMatrix(lightDir, distance, focusPoint, mapSize, cfg.stabilize());

    r::bindFramebuffer(r::FramebufferTarget::Framebuffer, gShadowFbo);
    r::viewport(0, 0, mapSize, mapSize);
    r::clear(false, true, false);
    r::enable(r::Capability::DepthTest);
    r::depthFunc(r::DepthFunc::Less);
    r::depthMask(true);
    r::disable(r::Capability::Blend);
    r::enable(r::Capability::CullFace);
    r::cullFace(r::CullFaceMode::Back);

    // World depth
    if (cfg.worldReceivesShadows() || true) // world always casts into the map
        renderWorldDepth(world, r::shadowShader().programObject(), gLightMVP);

    // Player depth
    if (cfg.playersCastShadows() && mimita::game::playerPtr())
        renderPlayerDepth(*mimita::game::playerPtr(), r::shadowShader().programObject(), gLightMVP);

    // NPC depth
    if (cfg.npcsCastShadows() && mimita::game::npcSystemPtr())
        renderNpcDepths(r::shadowShader().programObject(), gLightMVP);

    // Effect depth
    if (cfg.effectsCastShadows())
        renderEffectDepths(r::shadowShader().programObject(), gLightMVP);

    // Particle depth
    if (cfg.particlesCastShadows())
        renderParticleDepths(r::shadowShader().programObject(), gLightMVP);

    r::bindFramebuffer(r::FramebufferTarget::Framebuffer, 0);
}

void renderPlayerDepth(const Player& player, mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj)
{
    Perf::state().renderPerf.shadowBatches++;
    player.renderDepth(shadowShader, lightViewProj);
}

void renderNpcDepths(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj)
{
    if (!mimita::game::npcSystemPtr()) return;
    for (const Npc& npc : mimita::game::npcSystemPtr()->all()) {
        Perf::state().renderPerf.shadowBatches++;
        npc.body.renderDepth(shadowShader, lightViewProj);
    }
}

void renderEffectDepths(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj)
{
    ensureSphereVao();

    r::useProgram(shadowShader.program());
    const auto loc = shadowShader.uniform("uLightMVP");
    if (loc < 0) { r::useProgram(0); return; }
    r::bindVertexArray(gSphereVao);

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
        r::uniformMatrix4fv(loc, 1, &mvp[0][0]);
        r::drawArrays(r::Primitive::Triangles, 0, gSphereVertCount);
    }

    r::bindVertexArray(0);
    r::useProgram(0);
}

void renderParticleDepths(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj)
{
    ensureSphereVao();

    r::useProgram(shadowShader.program());
    const auto loc = shadowShader.uniform("uLightMVP");
    if (loc < 0) { r::useProgram(0); return; }
    r::bindVertexArray(gSphereVao);

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
        r::uniformMatrix4fv(loc, 1, &mvp[0][0]);
        r::drawArrays(r::Primitive::Triangles, 0, gSphereVertCount);
    }

    r::bindVertexArray(0);
    r::useProgram(0);
}

void bindShadowMap(int textureUnit)
{
    if (gShadowDepthTex) {
        r::activeTexture(static_cast<std::uint32_t>(textureUnit));
        r::bindTexture(r::TextureTarget::Texture2D, gShadowDepthTex);
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

std::uint32_t shadowDepthTex()
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

    r::disable(r::Capability::DepthTest);
    r::enable(r::Capability::CullFace);

    if (!r::worldShader().ensureLoaded()) return;
    auto& shader = r::worldShader();

    float verts[] = {
        (float)x,          (float)y,           0.0f, 0.0f,
        (float)(x + size), (float)y,           1.0f, 0.0f,
        (float)(x + size), (float)(y + size),  1.0f, 1.0f,
        (float)x,          (float)y,           0.0f, 0.0f,
        (float)(x + size), (float)(y + size),  1.0f, 1.0f,
        (float)x,          (float)(y + size),  0.0f, 1.0f,
    };

    static std::uint32_t vao = 0, vbo = 0;
    if (!vao) {
        vao = mimita::game::genVertexArray();
        vbo = mimita::game::genBuffer();
    }

    r::bindVertexArray(vao);
    r::bindBuffer(r::BufferTarget::Array, vbo);
    r::bufferData(r::BufferTarget::Array, vbo, verts, sizeof(verts),
                  r::BufferUsage::Stream);
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 2, r::VertexType::Float, false, 4 * sizeof(float), 0);
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, 4 * sizeof(float),
                           2 * sizeof(float));

    r::useProgram(shader.program());
    r::uniform1i(shader.uniform("uUseColor"), 0);
    r::uniform1i(shader.uniform("uDebugView"), 3);
    glm::mat4 id(1.0f);
    r::uniformMatrix4fv(shader.uniform("model"), 1, &id[0][0]);
    r::uniformMatrix4fv(shader.uniform("view"), 1, &id[0][0]);
    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
    r::uniformMatrix4fv(shader.uniform("projection"), 1, &ortho[0][0]);

    r::activeTexture(0);
    r::bindTexture(r::TextureTarget::Texture2D, gShadowDepthTex);
    r::uniform1i(shader.uniform("uTex"), 0);

    r::drawArrays(r::Primitive::Triangles, 0, 6);
    r::bindVertexArray(0);
    r::useProgram(0);
}
