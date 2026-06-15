#pragma once

#include <cstdint>
#include <string>

struct PostFXData
{
    // Core
    float brightness = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float gamma = 2.2f;
    float hueShift = 0.0f;
    float colorTemperature = 0.0f;
    float vignette = 0.0f;
    float filmGrain = 0.0f;
    float chromaticAberration = 0.0f;
    float lensDistortion = 0.0f;
    float scanlines = 0.0f;
    float pixelation = 0.0f;
    float posterize = 0.0f;

    // GUI
    float guiBrightness = 1.0f;
    float guiContrast = 1.0f;
    float guiSaturation = 1.0f;
    float guiHueShift = 0.0f;

    // Artistic modes
    float dreamStrength = 0.0f;
    float voidStrength = 0.0f;
    float psychedelicStrength = 0.0f;
    float retroStrength = 0.0f;
    float glitchStrength = 0.0f;

    // Procedural modifiers
    float worldWave = 0.0f;
    float screenWave = 0.0f;
    float screenShakeFx = 0.0f;
    float edgeGlow = 0.0f;
    float outlineBoost = 0.0f;
    float shadowBoost = 0.0f;
};

class PostFX
{
public:
    static PostFX& instance();

    // Load/save config
    void loadConfig(const std::string& path);
    void applyConfig(const PostFXData& data);
    void pollReload();

    // Pipeline
    bool initFBO(int width, int height);
    void bindFBO();
    void unbindFBO();
    void render();
    void setBypass(bool bypass) { mBypass = bypass; }
    bool bypass() const { return mBypass; }
    void requestMagentaTest() { mMagentaTestPending = true; }
    bool consumeMagentaTest();

    // Debug
    bool debugEnabled = false;
    const char* debugText() const { return mDebugText; }

    // Presets
    void applyPreset(const std::string& name);
    const PostFXData& data() const { return mData; }

    // Animated time (for psychedelic, dream modes)
    float time() const { return mTime; }
    void advanceTime(float dt) { mTime += dt; }

    // Diagnostic accessors
    GLuint fboId() const { return mFbo; }
    GLuint colorTexId() const { return mColorTex; }
    GLuint quadVaoId() const { return mQuadVao; }
    GLuint postShaderId() const { return mPostShader; }
    int fboWidth() const { return mFboW; }
    int fboHeight() const { return mFboH; }
    bool hasFbo() const { return mFbo != 0; }
    bool hasColorTex() const { return mColorTex != 0; }
    bool hasQuadVao() const { return mQuadVao != 0; }
    bool hasPostShader() const { return mPostShader != 0; }

public:
    PostFX() = default;

private:

    PostFXData mData;
    PostFXData mTarget; // for presets with animation

    std::string mConfigPath;
    uint64_t mLastModified = 0;

    // FBO
    GLuint mFbo = 0;
    GLuint mColorTex = 0;
    int mFboW = 0;
    int mFboH = 0;

    // Fullscreen quad
    GLuint mQuadVao = 0;
    GLuint mQuadVbo = 0;
    GLuint mGuiQuadVao = 0;
    GLuint mGuiQuadVbo = 0;

    // Shader
    GLuint mPostShader = 0;
    GLuint mGuiShader = 0;

    float mTime = 0.0f;
    char mDebugText[512] = {};
    bool mBypass = false;
    bool mMagentaTestPending = false;

    bool initQuad(GLuint& vao, GLuint& vbo);
    bool loadShaders();
    GLuint createShader(const char* vertPath, const char* fragPath);
    void setUniforms(GLuint shader, const PostFXData& data, const char* prefix = "");
    void renderQuad(GLuint vao);
};

extern PostFX gPostFX;
