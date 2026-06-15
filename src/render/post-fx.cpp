#include "post-fx.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <glad/glad.h>
#include <nlohmann/json.hpp>

#include "renderer/renderer.h"

using json = nlohmann::json;

extern Renderer* gRenderer;

PostFX gPostFX;

PostFX& PostFX::instance() { return gPostFX; }

static uint64_t fileModifiedTime(const std::string& path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        ft.time_since_epoch()).count();
}

void PostFX::loadConfig(const std::string& path)
{
    mConfigPath = path;
    mLastModified = fileModifiedTime(path);

    std::ifstream file(path);
    if (!file.is_open())
    {
        printf("[POSTFX] No config at %s, using defaults\n", path.c_str());
        return;
    }

    try
    {
        json j;
        file >> j;

        auto read = [&](const std::string& key, float& val, float def) {
            val = j.contains(key) ? j[key].get<float>() : def;
        };

        read("brightness", mData.brightness, 1.0f);
        read("contrast", mData.contrast, 1.0f);
        read("saturation", mData.saturation, 1.0f);
        read("gamma", mData.gamma, 2.2f);
        read("hueShift", mData.hueShift, 0.0f);
        read("colorTemperature", mData.colorTemperature, 0.0f);
        read("vignette", mData.vignette, 0.0f);
        read("filmGrain", mData.filmGrain, 0.0f);
        read("chromaticAberration", mData.chromaticAberration, 0.0f);
        read("lensDistortion", mData.lensDistortion, 0.0f);
        read("scanlines", mData.scanlines, 0.0f);
        read("pixelation", mData.pixelation, 0.0f);
        read("posterize", mData.posterize, 0.0f);

        read("guiBrightness", mData.guiBrightness, 1.0f);
        read("guiContrast", mData.guiContrast, 1.0f);
        read("guiSaturation", mData.guiSaturation, 1.0f);
        read("guiHueShift", mData.guiHueShift, 0.0f);

        read("dreamStrength", mData.dreamStrength, 0.0f);
        read("voidStrength", mData.voidStrength, 0.0f);
        read("psychedelicStrength", mData.psychedelicStrength, 0.0f);
        read("retroStrength", mData.retroStrength, 0.0f);
        read("glitchStrength", mData.glitchStrength, 0.0f);

        read("worldWave", mData.worldWave, 0.0f);
        read("screenWave", mData.screenWave, 0.0f);
        read("screenShakeFx", mData.screenShakeFx, 0.0f);
        read("edgeGlow", mData.edgeGlow, 0.0f);
        read("outlineBoost", mData.outlineBoost, 0.0f);
        read("shadowBoost", mData.shadowBoost, 0.0f);

        snprintf(mDebugText, sizeof(mDebugText),
            "POSTFX\nbri=%.2f con=%.2f sat=%.2f gam=%.2f hue=%.2f\n"
            "vig=%.2f grain=%.2f chroma=%.2f distort=%.2f scan=%.2f\n"
            "pix=%.2f post=%.2f\n"
            "dream=%.2f void=%.2f psy=%.2f retro=%.2f glitch=%.2f\n"
            "wWave=%.2f sWave=%.2f sShake=%.2f edge=%.2f out=%.2f shadow=%.2f",
            mData.brightness, mData.contrast, mData.saturation, mData.gamma, mData.hueShift,
            mData.vignette, mData.filmGrain, mData.chromaticAberration, mData.lensDistortion, mData.scanlines,
            mData.pixelation, mData.posterize,
            mData.dreamStrength, mData.voidStrength, mData.psychedelicStrength, mData.retroStrength, mData.glitchStrength,
            mData.worldWave, mData.screenWave, mData.screenShakeFx, mData.edgeGlow, mData.outlineBoost, mData.shadowBoost);

        printf("[POSTFX] Loaded config from %s\n", path.c_str());
    }
    catch (const std::exception& e)
    {
        printf("[POSTFX] Error loading %s: %s\n", path.c_str(), e.what());
    }
}

void PostFX::pollReload()
{
    if (mConfigPath.empty()) return;
    uint64_t mod = fileModifiedTime(mConfigPath);
    if (mod != 0 && mod != mLastModified)
    {
        mLastModified = mod;
        printf("[POSTFX] File changed\n");
        printf("[POSTFX] Reloaded %s\n", mConfigPath.c_str());
        loadConfig(mConfigPath);
    }
}

bool PostFX::initQuad(GLuint& vao, GLuint& vbo)
{
    float verts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

GLuint PostFX::createShader(const char* vertPath, const char* fragPath)
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

    auto compile = [](GLuint shader, const char* src) -> bool {
        const char* srcs[] = { src };
        glShaderSource(shader, 1, srcs, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            printf("[SHADER] compile error: %s\n", log);
            return false;
        }
        return true;
    };

    std::string vsrc = readFile(vertPath);
    std::string fsrc = readFile(fragPath);
    if (vsrc.empty() || fsrc.empty())
    {
        printf("[SHADER] Failed to read %s or %s\n", vertPath, fragPath);
        return 0;
    }

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!compile(vs, vsrc.c_str()) || !compile(fs, fsrc.c_str()))
    {
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
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        printf("[SHADER] link error: %s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

bool PostFX::loadShaders()
{
    mPostShader = createShader("shaders/post.vert", "shaders/post.frag");
    if (!mPostShader)
    {
        printf("[POSTFX] Failed to load post shader\n");
        return false;
    }

    // GUI shader is the same — the effect values differ (uGui prefix vs no prefix)
    mGuiShader = mPostShader; // same shader handles both via uniform prefixes

    // Must init quad VAO so PostFX::render() can blit the FBO to screen.
    // Without this, render() returns early and 3D content is invisible.
    if (!initQuad(mQuadVao, mQuadVbo))
    {
        printf("[POSTFX] Failed to init quad VAO\n");
        return false;
    }

    return true;
}

bool PostFX::initFBO(int width, int height)
{
    if (mFbo && width == mFboW && height == mFboH)
        return true;

    // Clean up old
    if (mFbo)
    {
        glDeleteFramebuffers(1, &mFbo);
        glDeleteTextures(1, &mColorTex);
    }

    mFboW = width;
    mFboH = height;

    glGenTextures(1, &mColorTex);
    glBindTexture(GL_TEXTURE_2D, mColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &mFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTex, 0);

    GLuint depthRb = 0;
    glGenRenderbuffers(1, &depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("[POSTFX] FBO incomplete: 0x%x\n", status);
        glDeleteFramebuffers(1, &mFbo);
        glDeleteTextures(1, &mColorTex);
        glDeleteRenderbuffers(1, &depthRb);
        mFbo = 0;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    printf("[POSTFX] FBO created: %dx%d\n", width, height);
    return true;
}

void PostFX::bindFBO()
{
    if (!mFbo) return;
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    // Clear the FBO's color and depth buffers.
    // Renderer::beginFrame() only clears the default framebuffer, not the FBO.
    // Without this, the FBO retains the previous frame's depth buffer,
    // causing new geometry to fail depth testing.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostFX::unbindFBO()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostFX::setUniforms(GLuint shader, const PostFXData& data, const char* prefix)
{
    auto loc = [&](const char* name) -> GLint {
        std::string full = std::string(prefix) + name;
        return glGetUniformLocation(shader, full.c_str());
    };

    auto set1f = [&](const char* name, float v) {
        GLint l = loc(name);
        if (l >= 0) glUniform1f(l, v);
    };
    auto set1i = [&](const char* name, int v) {
        GLint l = loc(name);
        if (l >= 0) glUniform1i(l, v);
    };

    set1i("uScene", 0);
    set1f("uBrightness", data.brightness);
    set1f("uContrast", data.contrast);
    set1f("uSaturation", data.saturation);
    set1f("uGamma", data.gamma);
    set1f("uHueShift", data.hueShift);
    set1f("uColorTemperature", data.colorTemperature);
    set1f("uVignette", data.vignette);
    set1f("uFilmGrain", data.filmGrain);
    set1f("uChromaticAberration", data.chromaticAberration);
    set1f("uLensDistortion", data.lensDistortion);
    set1f("uScanlines", data.scanlines);
    set1f("uPixelation", data.pixelation);
    set1f("uPosterize", data.posterize);
    set1f("uDreamStrength", data.dreamStrength);
    set1f("uVoidStrength", data.voidStrength);
    set1f("uPsychedelicStrength", data.psychedelicStrength);
    set1f("uRetroStrength", data.retroStrength);
    set1f("uGlitchStrength", data.glitchStrength);
    set1f("uWorldWave", data.worldWave);
    set1f("uScreenWave", data.screenWave);
    set1f("uScreenShakeFx", data.screenShakeFx);
    set1f("uEdgeGlow", data.edgeGlow);
    set1f("uOutlineBoost", data.outlineBoost);
    set1f("uShadowBoost", data.shadowBoost);

    // Time for animated effects
    set1f("uTime", mTime);
    set1f("uScreenW", (float)mFboW);
    set1f("uScreenH", (float)mFboH);
}

void PostFX::renderQuad(GLuint vao)
{
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void PostFX::render()
{
    if (!mPostShader || !mFbo || !mQuadVao) return;

    glDisable(GL_DEPTH_TEST);

    // --- Pass 1: full scene post-process ---
    glUseProgram(mPostShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mColorTex);
    setUniforms(mPostShader, mData, "");

    renderQuad(mQuadVao);

    // GUI is rendered after this by the caller (PostFX::unbindFBO returns to default FB)
}

void PostFX::applyPreset(const std::string& name)
{
    PostFXData p;
    if (name == "normal" || name == "default")
    {
        // Keep defaults from struct initializer
    }
    else if (name == "dream")
    {
        p.dreamStrength = 0.6f; p.saturation = 1.3f; p.brightness = 1.1f;
        p.gamma = 2.0f; p.vignette = 0.3f; p.hueShift = 0.05f;
    }
    else if (name == "void")
    {
        p.voidStrength = 0.8f; p.saturation = 0.3f; p.brightness = 0.6f;
        p.contrast = 1.4f; p.vignette = 0.7f; p.shadowBoost = 0.5f;
    }
    else if (name == "psychedelic")
    {
        p.psychedelicStrength = 0.7f; p.saturation = 2.0f; p.hueShift = 0.0f;
        p.chromaticAberration = 0.3f; p.worldWave = 0.4f;
    }
    else if (name == "retro" || name == "ps2")
    {
        p.retroStrength = 0.7f; p.pixelation = 0.5f; p.scanlines = 0.4f;
        p.saturation = 0.8f; p.contrast = 1.2f; p.chromaticAberration = 0.15f;
        p.lensDistortion = 0.05f; p.filmGrain = 0.2f;
    }
    else if (name == "glitch")
    {
        p.glitchStrength = 0.5f; p.chromaticAberration = 0.6f;
        p.lensDistortion = 0.1f; p.filmGrain = 0.3f;
    }
    else if (name == "competitive" || name == "melee")
    {
        p.contrast = 1.15f; p.brightness = 0.95f; p.saturation = 1.1f;
        p.gamma = 2.0f; p.vignette = 0.15f; p.shadowBoost = 0.2f;
    }
    else
    {
        printf("[POSTFX] Unknown preset '%s'\n", name.c_str());
        return;
    }
    mData = p;
    printf("[POSTFX] Applied preset '%s'\n", name.c_str());
}

void PostFX::applyConfig(const PostFXData& data)
{
    mData = data;
}
