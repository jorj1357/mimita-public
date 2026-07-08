#include "skybox.h"
#include "camera.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cmath>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include <nlohmann/json.hpp>

Skybox gSkybox;

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

static std::string shaderPath(const std::string& name) {
    return "shaders/" + name;
}

static std::string readTextFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string s((size_t)len, '\0');
    fread(&s[0], 1, (size_t)len, f);
    fclose(f);
    return s;
}

static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
    const char* srcPtr = src.c_str();
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Debug::log(Debug::Category::Render, "[SKYBOX] Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createProgram(const std::string& vertSrc, const std::string& fragSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) {
        glDeleteShader(vs); glDeleteShader(fs);
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
        Debug::log(Debug::Category::Render, "[SKYBOX] Link error: %s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
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
           mCubemapTex, mShader, mVAO, (int)mEnabled);
}

void Skybox::destroyCubemap() {
    if (mCubemapTex) { glDeleteTextures(1, &mCubemapTex); mCubemapTex = 0; }
}

void Skybox::destroyMesh() {
    if (mVAO) { glDeleteVertexArrays(1, &mVAO); mVAO = 0; }
    if (mVBO) { glDeleteBuffers(1, &mVBO); mVBO = 0; }
}

void Skybox::createMesh() {
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gCubeVerts), gCubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void Skybox::compileShader() {
    if (mShader) { glDeleteProgram(mShader); mShader = 0; }
    std::string vertSrc = readTextFile(shaderPath("skybox.vert"));
    std::string fragSrc = readTextFile(shaderPath("skybox.frag"));
    SKYDBG("compileShader: vert=%zu bytes frag=%zu bytes\n", vertSrc.size(), fragSrc.size());
    if (vertSrc.empty() || fragSrc.empty()) {
        SKYDBG("compileShader: FAILED to read shader files from 'shaders/skybox.vert/frag'\n");
        Debug::log(Debug::Category::Render, "[SKYBOX] Cannot read shader files\n");
        return;
    }
    mShader = createProgram(vertSrc, fragSrc);
    SKYDBG("compileShader: program=%u\n", mShader);
    if (!mShader) {
        Debug::log(Debug::Category::Render, "[SKYBOX] Shader compilation failed\n");
    }
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

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    bool anyLoaded = false;
    for (int i = 0; i < 6; i++) {
        const std::string& path = mFaces[i].path;
        if (path.empty() || !fileExists(path)) {
            Debug::log(Debug::Category::Render, "[SKYBOX] Face %s missing: %s\n", SKYBOX_FACE_NAMES[i], path.c_str());
            // Create a 1x1 magenta pixel as placeholder
            unsigned char magenta[] = {255, 0, 255, 255};
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta);
            continue;
        }
        int w, h, comp;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!data) {
            Debug::log(Debug::Category::Render, "[SKYBOX] Failed to load %s\n", path.c_str());
            unsigned char magenta[] = {255, 0, 255, 255};
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta);
            continue;
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        anyLoaded = true;
    }

    if (!anyLoaded) {
        glDeleteTextures(1, &tex);
        mCubemapTex = 0;
        Debug::log(Debug::Category::Render, "[SKYBOX] No face images loaded\n");
        return;
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    mCubemapTex = tex;
    Debug::log(Debug::Category::Render, "[SKYBOX] Cubemap loaded (tex=%u)\n", tex);
}

void Skybox::pollReload() {
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

void Skybox::setUniforms() const {
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

    glUniform3fv(glGetUniformLocation(mShader, "uFaceColor"), 6, glm::value_ptr(faceColors[0]));
    glUniform1fv(glGetUniformLocation(mShader, "uFaceAlpha"), 6, faceAlphas);
    glUniform1fv(glGetUniformLocation(mShader, "uFaceHue"), 6, faceHues);
    glUniform2fv(glGetUniformLocation(mShader, "uFaceStretch"), 6, glm::value_ptr(faceStretches[0]));
    glUniform2fv(glGetUniformLocation(mShader, "uFaceUVOffset"), 6, glm::value_ptr(faceUVOffsets[0]));

    glUniform1f(glGetUniformLocation(mShader, "uGlobalRotation"), mGlobalRotation);
    glUniform1f(glGetUniformLocation(mShader, "uGlobalHue"), mGlobalHue);
    glUniform2f(glGetUniformLocation(mShader, "uGlobalScale"), mGlobalScaleX, mGlobalScaleY);
}

void Skybox::render(const Camera& camera) {
    if (!mInitialized) init();
    if (!mEnabled || mCubemapTex == 0 || mShader == 0 || mVAO == 0) {
        SKYDBG("render: SKIPPED (enabled=%d tex=%u shader=%u vao=%u)\n",
               (int)mEnabled, mCubemapTex, mShader, mVAO);
        return;
    }

    SKYDBG("render: CALLED tex=%u shader=%u\n", mCubemapTex, mShader);

    // Save render state
    GLint prevCullFaceMode, prevDepthFunc;
    GLboolean prevCullEnabled, prevDepthMask;
    glGetIntegerv(GL_CULL_FACE_MODE, &prevCullFaceMode);
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glGetBooleanv(GL_CULL_FACE, &prevCullEnabled);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);

    // Skybox render state:
    // 1. Cull front faces (we're inside the cube, so front faces are the inner ones)
    //    (Alternatively just disable culling entirely)
    glDisable(GL_CULL_FACE);
    // 2. Use LEQUAL depth so skybox passes at the far plane
    glDepthFunc(GL_LEQUAL);
    // 3. Allow depth writes so skybox fills the depth buffer
    glDepthMask(GL_TRUE);

    glUseProgram(mShader);
    glBindVertexArray(mVAO);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mCubemapTex);

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj(
        (float)glm::max(1, 1280), (float)glm::max(1, 720));
    glUniformMatrix4fv(glGetUniformLocation(mShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(mShader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

    setUniforms();

    glDrawArrays(GL_TRIANGLES, 0, 36);
    Debug::log(Debug::Category::Render, "[SKYBOX] draw call submitted\n");

    glBindVertexArray(0);
    glUseProgram(0);

    // Restore render state
    glDepthFunc(prevDepthFunc);
    glDepthMask(prevDepthMask);
    if (prevCullEnabled) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    glCullFace(prevCullFaceMode);
}
