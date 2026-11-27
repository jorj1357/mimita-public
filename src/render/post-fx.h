#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "render/shader-program.h"

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
    bool chromaticAberrationRadial = false;
    // false = full-world chromatic aberration; true = only background/dark
    // pixels receive the shifted red/blue image.
    bool chromaticAberrationBackgroundOnly = false;
    float lensDistortion = 0.0f;
    float lensDistortionCurve = 1.0f;
    float lensDistortionZoom = 1.0f;
    int lensDistortionEdgeMode = 4;
    float lensDistortionEdgeRadius = 0.72f;
    float lensDistortionEdgeSoftness = 0.30f;
    float lensDistortionEdgeDarkness = 1.0f;
    float lensDistortionPeripheralBlur = 1.0f;
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
    enum class Mode { Full = 0, Minimal = 1, Off = 2 };

    // Chromatic-aberration presentation modes. These are deliberately
    // exclusive: exactly one compositing policy is active at a time.
    enum class LensMode : int {
        Mixed = 1,       // CA only through dark/background pixels
        WorldOnly = 2,   // CA on the complete world, UI composited normally
        Normal = 3,      // no CA anywhere; black stays black
        WorldAndUi = 4,  // CA on the complete world and UI/3D popups
        BodycamSpecial = 5 // whole-frame CA + fisheye/vignette, UI in scene
    };

    static PostFX& instance();

    // Load/save config
    void loadConfig(const std::string& path);
    void applyConfig(const PostFXData& data);
    struct ModeBodycamLensOverride {
        float chromaticAberration = 0.0f;
        bool chromaticAberrationBackgroundOnly = false;
        int lensMode = 1;
        float vignette = 0.0f;
        float distortion = 0.0f;
        float curve = 2.0f;
        float zoom = 1.0f;
        int edgeMode = 1;
        float edgeRadius = 0.72f;
        float edgeSoftness = 0.30f;
        float edgeDarkness = 1.0f;
        float peripheralBlur = 1.0f;
        bool radialChromaticAberration = true;
    };
    void setBodycamLensOverride(const ModeBodycamLensOverride& value) {
        mBodycamLensOverride = value;
    }
    void clearBodycamLensOverride() { mBodycamLensOverride.reset(); }
    int effectiveLensMode() const;
    float effectiveChromaticAberration() const;
    void setLensModeOverride(int mode);
    void clearLensModeOverride() { mLensModeOverride.reset(); }
    bool hasLensModeOverride() const { return mLensModeOverride.has_value(); }
    // Live CA on/off. An empty override uses the resolved bodycam/config value.
    void setChromaticAberrationOverride(float strength) {
        mChromaticAberrationOverride = std::max(0.0f, strength);
    }
    void clearChromaticAberrationOverride() {
        mChromaticAberrationOverride.reset();
    }
    bool hasChromaticAberrationOverride() const {
        return mChromaticAberrationOverride.has_value();
    }
    bool chromaticAberrationEnabled() const {
        return effectiveLensMode() != 3 && effectiveChromaticAberration() > 0.0001f;
    }
    const ModeBodycamLensOverride* bodycamLensOverride() const {
        return mBodycamLensOverride ? &*mBodycamLensOverride : nullptr;
    }
    void setPresentationPatternFrames(int frames) {
        mPresentationPatternFrames = std::max(0, frames);
    }
    int presentationPatternFrames() const { return mPresentationPatternFrames; }
    void consumePresentationPatternFrame() {
        if (mPresentationPatternFrames > 0) --mPresentationPatternFrames;
    }
    static const char* lensModeName(int mode);
    // Camera-owned lens distortion supports values well above 100.
    void setCameraLensDistortion(float amount, float curve, float zoom,
                                 int edgeMode, float edgeRadius,
                                 float edgeSoftness, float edgeDarkness,
                                 float peripheralBlur);
    void pollReload();

    // Pipeline
    bool initFBO(int width, int height);
    void bindFBO();
    void unbindFBO();
    void render();
    void probeScene(const char* phase);
    void setBypass(bool bypass) { mBypass = bypass; }
    bool bypass() const { return mBypass; }
    Mode mode() const { return mMode; }
    void setMode(Mode mode) { mMode = mode; }
    bool enabled() const { return mMode != Mode::Off; }

    // Returns true when any PostFX parameter differs from identity.
    // When false, the FBO + fullscreen pass can be skipped entirely.
    bool isActive() const;
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
    void advanceTime(float dt) {
        if (mMode == Mode::Off) return;
        mTime += dt;
        updateRandomAnim(dt);
    }

    // Random animation mode (postfxrand)
    void enableRandomMode();
    void disableRandomMode();
    bool randomModeEnabled() const { return mRandomMode; }
    bool setAnimExcluded(const std::string& name, bool exclude);
    bool animExcluded(int idx) const { return idx >= 0 && idx < mAnimPropCount && mAnimExcluded[idx]; }
    int animPropCount() const { return mAnimPropCount; }
    const char* animPropName(int idx) const;

    // Diagnostic accessors
    std::uint32_t fboId() const { return mFbo; }
    std::uint32_t colorTexId() const { return mColorTex; }
    std::uint32_t quadVaoId() const { return mQuadVao; }
    std::uint32_t postShaderId() const { return mPostShader.program(); }
    int fboWidth() const { return mFboW; }
    int fboHeight() const { return mFboH; }
    bool hasFbo() const { return mFbo != 0; }
    bool hasColorTex() const { return mColorTex != 0; }
    bool hasQuadVao() const { return mQuadVao != 0; }
    bool hasPostShader() const { return mPostShader.valid(); }

public:
    PostFX() = default;

private:

    PostFXData mData;
    std::optional<ModeBodycamLensOverride> mBodycamLensOverride;
    std::optional<int> mLensModeOverride;
    std::optional<float> mChromaticAberrationOverride;
    int mPresentationPatternFrames = 0;
    PostFXData mTarget; // for presets with animation

    std::string mConfigPath;
    uint64_t mLastModified = 0;

    // FBO
    std::uint32_t mFbo = 0;
    std::uint32_t mColorTex = 0;
    int mFboW = 0;
    int mFboH = 0;

    // Fullscreen quad
    std::uint32_t mQuadVao = 0;
    std::uint32_t mQuadVbo = 0;
    std::uint32_t mGuiQuadVao = 0;
    std::uint32_t mGuiQuadVbo = 0;

    // Shader
    mimita::game::render::ShaderProgram mPostShader;

    float mTime = 0.0f;
    char mDebugText[512] = {};
    bool mBypass = false;
    Mode mMode = Mode::Full;
    bool mMagentaTestPending = false;

    // Random animation
    struct AnimProp {
        float* value = nullptr;
        float startValue = 0.0f;
        float target = 0.0f;
        float duration = 1.0f;
        float elapsed = 0.0f;
        float minVal = -1.0f;
        float maxVal = 1.0f;
    };
    AnimProp mAnimProps[28];
    bool mAnimExcluded[28] = {};
    int mAnimPropCount = 0;
    bool mRandomMode = false;
    PostFXData mOriginalValues;
    bool mRestoring = false;
    float mRestoreElapsed = 0.0f;
    static constexpr float RESTORE_DURATION = 1.0f;

    void buildAnimProps();
    void pickRandomTarget(AnimProp& p);
    void updateRandomAnim(float dt);

    bool initQuad(std::uint32_t& vao, std::uint32_t& vbo);
    bool loadShaders();
    void setUniforms(const PostFXData& data, const char* prefix = "");
    void renderQuad(std::uint32_t vao);
};

extern PostFX gPostFX;

// Deferred presentation probe. Applying a lens mode and reading the pixels
// back in the same tick would measure the previous mode. Scheduling the
// capture a couple of frames later keeps mode, test pattern, and readback in
// one atomic request, so a concurrent game-DLL generation swap cannot drop
// the mode between two separate commands.
void requestPresentationProbe(const std::string& tag, int mode);
void tickPresentationProbeFrame();
