#pragma once

#include <string>
#include <glm/glm.hpp>

struct LightingConfigData {
    glm::vec3 ambientColor{0.35f, 0.35f, 0.40f};
    float ambientIntensity = 1.0f;

    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.35f, -0.1f, -1.0f));
    float diffuseStrength = 0.45f;

    float edgeDarkness = 0.10f;
    float edgeWidth = 1.2f;

    float aoDarkness = 0.05f;
    float aoContrast = 0.8f;

    float textureContrast = 1.02f;
    float textureBrightness = 1.35f;

    bool fogEnabled = false;
    float fogDensity = 0.002f;
    glm::vec3 fogColor{0.7f, 0.8f, 1.0f};

    float brightness = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float gamma = 2.2f;
    float hueShift = 0.0f;

    bool aoEnabled = false;
    float aoStrength = 1.0f;
};

class LightingConfig {
public:
    static LightingConfig& instance();

    bool load(const std::string& path);
    void reset();

    bool checkFileChanged() const;
    bool pollReload();

    const LightingConfigData& data() const { return mData; }
    const std::string& path() const { return mPath; }

    // Ambient
    glm::vec3 ambientColor() const { return mData.ambientColor * mData.ambientIntensity; }
    float ambientStrength() const { return 0.72f * mData.ambientIntensity; }
    glm::vec3 ambientRaw() const { return mData.ambientColor; }
    float ambientIntensity() const { return mData.ambientIntensity; }

    // Directional light
    glm::vec3 lightDir() const { return mData.lightDir; }
    float diffuseStrength() const { return mData.diffuseStrength; }

    // Edge
    float edgeDarkness() const { return mData.edgeDarkness; }
    float edgeWidth() const { return mData.edgeWidth; }

    // AO
    float aoDarkness() const { return mData.aoDarkness; }
    float aoContrast() const { return mData.aoContrast; }

    // Texture
    float textureContrast() const { return mData.textureContrast; }
    float textureBrightness() const { return mData.textureBrightness; }

    // Fog
    bool fogEnabled() const { return mData.fogEnabled; }
    float fogDensity() const { return mData.fogDensity; }
    glm::vec3 fogColor() const { return mData.fogColor; }

    // Post-processing
    float brightness() const { return mData.brightness; }
    float contrast() const { return mData.contrast; }
    float saturation() const { return mData.saturation; }
    float gamma() const { return mData.gamma; }
    float hueShift() const { return mData.hueShift; }

private:
    LightingConfig() = default;

    LightingConfigData mData;
    std::string mPath;
    int64_t mLastModified = 0;
};
