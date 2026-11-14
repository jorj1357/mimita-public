#include "skybox.h"
#include "camera.h"
#include "renderer/renderer.h"
#include "game/gl-resource-bridge-module.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cmath>

#include "render/render-command.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include <nlohmann/json.hpp>

Skybox gSkybox;
extern Renderer* gRenderer;

namespace r = mimita::game::render;

// ── Printf-based debug (always visible in console) ──────────
#define SKYDBG(...) do { printf("[SKYBOX] " __VA_ARGS__); fflush(stdout); } while(0)

// ── Unit cube vertices (12 triangles, 36 verts) ─────────────
static const float gCubeVerts[] = {
    // Back face
    -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,
    // Front face
    -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
    // Left face
    -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
    // Right face
     1.0f,  1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,
    // Bottom face
    -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
    // Top face
    -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
};

// ── Helpers ─────────────────────────────────────────────────

static bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

// ── Skybox implementation ───────────────────────────────────

Skybox::Skybox() = default;
Skybox::~Skybox() { destroyCubemap(); destroyMesh(); }

void Skybox::init() {
    if (mInitialized) { SKYDBG("init: already initialized\n"); return; }
    mInitialized = true;
    SKYDBG("init: starting (cwd check: config/skybox.json exists=%d)\n",
           (int)std::filesystem::exists("config/skybox.json"));
    compileShader();
    createMesh();
    loadConfig();
    SKYDBG("init: complete (tex=%u shader=%u vao=%u enabled=%d)\n",
           mCubemapTex, mProgram.program(), mVAO, (int)mEnabled);
}

void Skybox::destroyCubemap() {
    if (mCubemapTex) { mimita::game::deleteTexture(mCubemapTex); mCubemapTex = 0; }
}

void Skybox::destroyMesh() {
    if (mVAO) { mimita::game::deleteVertexArray(mVAO); mVAO = 0; }
    if (mVBO) { mimita::game::deleteBuffer(mVBO); mVBO = 0; }
}

void Skybox::createMesh() {
    mVAO = mimita::game::genVertexArray();
    mVBO = mimita::game::genBuffer();
    r::bindVertexArray(mVAO);
    r::bindBuffer(r::BufferTarget::Array, mVBO);
    r::bufferData(r::BufferTarget::Array, mVBO, gCubeVerts, sizeof(gCubeVerts),
                  r::BufferUsage::Static);
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false,
                           3 * sizeof(float), 0);
    r::bindVertexArray(0);
}

void Skybox::compileShader() {
    if (!mProgram.load("shaders/skybox.vert", "shaders/skybox.frag")) {
        SKYDBG("compileShader: FAILED to load shaders/skybox.vert|frag\n");
        Debug::log(Debug::Category::Render,
                   "[SKYBOX] Cannot read or compile shader files\n");
        return;
    }
    SKYDBG("compileShader: program=%u\n", mProgram.program());
}

bool Skybox::loadConfig(const std::string& path) {
    mConfigPath = path;
    SKYDBG("loadConfig: path='%s'\n", path.c_str());
    if (!fileExists(path)) {
        SKYDBG("loadConfig: FILE NOT FOUND at '%s'\n", path.c_str());
        Debug::log(Debug::Category::Render, "[SKYBOX] No config at %s — disabled\n", path.c_str());
        mEnabled = false;
        return false;
    }
    SKYDBG("loadConfig: file exists\n");

    std::ifstream f(path);
    if (!f.is_open()) {
        SKYDBG("loadConfig: FAILED to open file\n");
        Debug::log(Debug::Category::Render, "[SKYBOX] Cannot open %s\n", path.c_str());
        return false;
    }
    nlohmann::json j;
    try { f >> j; } catch (const std::exception& e) {
        SKYDBG("loadConfig: JSON parse failed: %s\n", e.what());
        Debug::log(Debug::Category::Render, "[SKYBOX] JSON parse failed: %s\n", path.c_str());
        return false;
    }
    SKYDBG("loadConfig: JSON parsed OK, enabled=%d folder='%s'\n",
           j.value("enabled", false), j.value("folder", "?").c_str());

    mEnabled = j.value("enabled", true);
    mFolder = j.value("folder", std::string());
    mGlobalRotationSpeed = j.value("rotation_speed", 0.0f);
    mGlobalHueSpeed = j.value("global_hue_speed", 0.0f);
    mGlobalScaleX = j.value("global_scale_x", 1.0f);
    mGlobalScaleY = j.value("global_scale_y", 1.0f);

    // Reset per-face config to defaults
    for (int i = 0; i < 6; i++) {
        mFaces[i] = SkyboxFaceConfig{};
    }

    // Load per-face config from JSON
    if (j.contains("faces") && j["faces"].is_object()) {
        const auto& faces = j["faces"];
        for (int i = 0; i < 6; i++) {
            const std::string& name = SKYBOX_FACE_NAMES[i];
            if (faces.contains(name) && faces[name].is_object()) {
                const auto& fc = faces[name];
                auto& cfg = mFaces[i];
                cfg.path = fc.value("path", std::string());
                if (fc.contains("color") && fc["color"].is_array() && fc["color"].size() >= 3) {
                    cfg.color = {fc["color"][0].get<float>(), fc["color"][1].get<float>(), fc["color"][2].get<float>()};
                }
                cfg.alpha = fc.value("alpha", 1.0f);
                cfg.rotationSpeed = fc.value("rotation_speed", 0.0f);
                cfg.hueSpeed = fc.value("hue_speed", 0.0f);
                cfg.stretchXSpeed = fc.value("stretch_x_speed", 0.0f);
                cfg.stretchYSpeed = fc.value("stretch_y_speed", 0.0f);
                cfg.uvScrollX = fc.value("uv_scroll_x", 0.0f);
                cfg.uvScrollY = fc.value("uv_scroll_y", 0.0f);
            }
        }
    }

    // Auto-resolve paths from folder if not specified
    if (!mFolder.empty()) {
        for (int i = 0; i < 6; i++) {
            if (mFaces[i].path.empty()) {
                mFaces[i].path = "assets/skybox/" + mFolder + "/" + SKYBOX_FACE_NAMES[i] + ".png";
            }
        }
    }

    // Load cubemap
    loadCubemap();

    // Reset animation state
    mGlobalRotation = 0.0f;
    mGlobalHue = 0.0f;
    for (int i = 0; i < 6; i++) {
        mFaceAnim[i] = SkyboxFaceAnim{};
    }

    // Track file for hot-reload
    std::error_code ec;
    mConfigLastWrite = std::filesystem::last_write_time(path, ec);

    Debug::log(Debug::Category::Render, "[SKYBOX] loaded folder=%s enabled=%d faces:", mFolder.c_str(), (int)mEnabled);
    for (int i = 0; i < 6; i++) {
        Debug::log(Debug::Category::Render, "[SKYBOX]   %s=%s\n", SKYBOX_FACE_NAMES[i], mFaces[i].path.c_str());
    }
    return true;
}

void Skybox::loadCubemap() {
    destroyCubemap();

    std::uint32_t tex = 0;
    tex = mimita::game::genTexture();
    r::bindTexture(r::TextureTarget::TextureCube, tex);

    bool anyLoaded = false;
    for (int i = 0; i < 6; i++) {
        const std::string& path = mFaces[i].path;
        SKYDBG("  face[%d] '%s' exists=%d\n", i, path.c_str(), (int)fileExists(path));
        if (path.empty() || !fileExists(path)) {
            unsigned char magenta[] = {255, 0, 255, 255};
            r::texImage2D(r::cubeFace(i), 0, r::TextureFormat::RGBA, 1, 1, 0,
                          r::PixelFormat::RGBA, r::PixelType::UnsignedByte, magenta);
            continue;
        }
        int w, h, comp;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!data) {
            SKYDBG("  stbi_load FAILED\n");
            unsigned char magenta[] = {255, 0, 255, 255};
            r::texImage2D(r::cubeFace(i), 0, r::TextureFormat::RGBA, 1, 1, 0,
                          r::PixelFormat::RGBA, r::PixelType::UnsignedByte, magenta);
            continue;
        }
        SKYDBG("  LOADED %dx%d\n", w, h);
        r::texImage2D(r::cubeFace(i), 0, r::TextureFormat::RGBA, w, h, 0,
                      r::PixelFormat::RGBA, r::PixelType::UnsignedByte, data);
        stbi_image_free(data);
        anyLoaded = true;
    }

    if (!anyLoaded) {
        mimita::game::deleteTexture(tex);
        mCubemapTex = 0;
        SKYDBG("ALL FACES FAILED\n");
        return;
    }
    SKYDBG("Cubemap loaded OK (tex=%u)\n", tex);

    r::texParameteri(r::TextureTarget::TextureCube, r::TextureParameter::MinFilter,
                     r::TextureValue::Linear);
    r::texParameteri(r::TextureTarget::TextureCube, r::TextureParameter::MagFilter,
                     r::TextureValue::Linear);
    r::texParameteri(r::TextureTarget::TextureCube, r::TextureParameter::WrapS,
                     r::TextureValue::ClampToEdge);
    r::texParameteri(r::TextureTarget::TextureCube, r::TextureParameter::WrapT,
                     r::TextureValue::ClampToEdge);
    r::texParameteri(r::TextureTarget::TextureCube, r::TextureParameter::WrapR,
                     r::TextureValue::ClampToEdge);

    mCubemapTex = tex;
    Debug::log(Debug::Category::Render, "[SKYBOX] Cubemap loaded (tex=%u)\n", tex);
}

void Skybox::pollReload() {
    // Shader sources are hot: recompile when either file changed. The previous
    // program stays active if the candidate fails.
    mProgram.reloadIfChanged();
    if (mConfigPath.empty()) return;
    std::error_code ec;
    auto wt = std::filesystem::last_write_time(mConfigPath, ec);
    if (ec || wt == std::filesystem::file_time_type::min()) return;
    if (wt != mConfigLastWrite) {
        mConfigLastWrite = wt;
        Debug::log(Debug::Category::Render, "[SKYBOX] hot reload triggered\n");
        loadConfig(mConfigPath);
    }
}

bool Skybox::isEnabled() {
    if (!mInitialized) init();
    return mEnabled && mCubemapTex != 0;
}

void Skybox::update(float dt) {
    if (!mEnabled || mCubemapTex == 0) return;

    // Global animations
    mGlobalRotation += mGlobalRotationSpeed * dt;
    if (mGlobalRotation > 360.0f) mGlobalRotation -= 360.0f;
    if (mGlobalRotation < 0.0f) mGlobalRotation += 360.0f;

    mGlobalHue += mGlobalHueSpeed * dt;
    if (mGlobalHue > 1.0f) mGlobalHue -= 1.0f;
    if (mGlobalHue < 0.0f) mGlobalHue += 1.0f;

    // Per-face animations
    for (int i = 0; i < 6; i++) {
        auto& a = mFaceAnim[i];
        const auto& c = mFaces[i];

        a.rotation += c.rotationSpeed * dt;
        if (a.rotation > 360.0f) a.rotation -= 360.0f;

        a.hue += c.hueSpeed * dt;
        if (a.hue > 1.0f) a.hue -= 1.0f;

        // Stretch: sin wave around 1.0 (breathe)
        if (c.stretchXSpeed > 0.0f)
            a.stretchX = 1.0f + std::sin(mGlobalRotation * 3.14159f / 180.0f * c.stretchXSpeed) * 0.1f;
        if (c.stretchYSpeed > 0.0f)
            a.stretchY = 1.0f + std::sin(mGlobalRotation * 3.14159f / 180.0f * c.stretchYSpeed) * 0.1f;

        a.uvOffX += c.uvScrollX * dt;
        a.uvOffY += c.uvScrollY * dt;
    }
}

void Skybox::setUniforms() {
    // Per-face arrays
    glm::vec3 faceColors[6];
    float faceAlphas[6];
    float faceHues[6];
    glm::vec2 faceStretches[6];
    glm::vec2 faceUVOffsets[6];

    for (int i = 0; i < 6; i++) {
        faceColors[i] = mFaces[i].color;
        faceAlphas[i] = mFaces[i].alpha;
        faceHues[i] = mFaceAnim[i].hue;
        faceStretches[i] = {mFaceAnim[i].stretchX, mFaceAnim[i].stretchY};
        faceUVOffsets[i] = {mFaceAnim[i].uvOffX, mFaceAnim[i].uvOffY};
    }

    r::uniform3fv(mProgram.uniform("uFaceColor"), 6, glm::value_ptr(faceColors[0]));
    r::uniform1fv(mProgram.uniform("uFaceAlpha"), 6, faceAlphas);
    r::uniform1fv(mProgram.uniform("uFaceHue"), 6, faceHues);
    r::uniform2fv(mProgram.uniform("uFaceStretch"), 6, glm::value_ptr(faceStretches[0]));
    r::uniform2fv(mProgram.uniform("uFaceUVOffset"), 6, glm::value_ptr(faceUVOffsets[0]));

    r::uniform1f(mProgram.uniform("uGlobalRotation"), mGlobalRotation);
    r::uniform1f(mProgram.uniform("uGlobalHue"), mGlobalHue);
    r::uniform2f(mProgram.uniform("uGlobalScale"), mGlobalScaleX, mGlobalScaleY);
}

void Skybox::render(const Camera& camera) {
    if (!mInitialized) init();
    if (!mEnabled || mCubemapTex == 0 || !mProgram.valid() || mVAO == 0) {
        SKYDBG("render: SKIPPED (enabled=%d tex=%u shader=%u vao=%u)\n",
               (int)mEnabled, mCubemapTex, mProgram.program(), mVAO);
        return;
    }

    if (StructuredLogger::instance().shouldLog(StructuredCategory::Skybox, StructuredLevel::Trace))
        SKYDBG("render: CALLED tex=%u shader=%u\n", mCubemapTex, mProgram.program());

    // Host-owned state guard: the host snapshots and restores the GL state it
    // owns, so the game does not query or restore raw GL state here.
    r::pushState();

    // Skybox render state:
    // 1. Do not cull: we are inside the cube.
    // 2. LEQUAL depth so the skybox passes at the far plane.
    // 3. Do not write depth: world geometry must render on top.
    r::disable(r::Capability::CullFace);
    r::depthFunc(r::DepthFunc::Lequal);
    r::depthMask(false);

    r::useProgram(mProgram.program());
    r::bindVertexArray(mVAO);
    r::bindTexture(r::TextureTarget::TextureCube, mCubemapTex);

    glm::mat4 view = camera.getView();
    const float projectionWidth = gRenderer
        ? (float)gRenderer->sceneWidth : 1280.0f;
    const float projectionHeight = gRenderer
        ? (float)gRenderer->sceneHeight : 720.0f;
    glm::mat4 proj = camera.getProj(projectionWidth, projectionHeight);
    r::uniformMatrix4fv(mProgram.uniform("view"), 1, glm::value_ptr(view));
    r::uniformMatrix4fv(mProgram.uniform("projection"), 1, glm::value_ptr(proj));

    setUniforms();

    r::drawArrays(r::Primitive::Triangles, 0, 36);
    Debug::log(Debug::Category::Render, "[SKYBOX] draw call submitted\n");

    r::bindVertexArray(0);
    r::useProgram(0);

    r::popState();
}
