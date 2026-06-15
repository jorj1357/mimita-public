#pragma once

#include <string>
#include <glm/glm.hpp>

struct ShadowConfigData {
    bool enabled = true;
    int shadowMapSize = 1024;
    float shadowDistance = 60.0f;
    float shadowBias = 0.002f;
    float shadowDarkness = 0.55f;
    float shadowSoftness = 1.5f;
    glm::vec3 shadowTint{0.0f, 0.0f, 0.0f};
    bool stabilize = true;
    bool debugDrawShadowFrustum = false;
    bool debugShadowMap = false;

    bool playersCastShadows = true;
    bool npcsCastShadows = true;
    bool weaponsCastShadows = true;
    bool effectsCastShadows = true;
    bool particlesCastShadows = true;

    bool playersReceiveShadows = true;
    bool npcsReceiveShadows = true;
    bool worldReceivesShadows = true;
    bool effectsReceiveShadows = false;

    float effectShadowCutoffAlpha = 0.05f;
};

class ShadowConfig {
public:
    static ShadowConfig& instance();

    bool load(const std::string& path);
    void reset();

    bool checkFileChanged() const;
    bool pollReload();

    const ShadowConfigData& data() const { return mData; }
    ShadowConfigData& data() { return mData; }
    const std::string& path() const { return mPath; }

    bool enabled() const { return mData.enabled; }
    int shadowMapSize() const { return mData.shadowMapSize; }
    float shadowDistance() const { return mData.shadowDistance; }
    float shadowBias() const { return mData.shadowBias; }
    float shadowDarkness() const { return mData.shadowDarkness; }
    float shadowSoftness() const { return mData.shadowSoftness; }
    glm::vec3 shadowTint() const { return mData.shadowTint; }
    bool stabilize() const { return mData.stabilize; }
    bool debugDrawShadowFrustum() const { return mData.debugDrawShadowFrustum; }
    bool debugShadowMap() const { return mData.debugShadowMap; }

    bool playersCastShadows() const { return mData.playersCastShadows; }
    bool npcsCastShadows() const { return mData.npcsCastShadows; }
    bool weaponsCastShadows() const { return mData.weaponsCastShadows; }
    bool effectsCastShadows() const { return mData.effectsCastShadows; }
    bool particlesCastShadows() const { return mData.particlesCastShadows; }
    bool playersReceiveShadows() const { return mData.playersReceiveShadows; }
    bool npcsReceiveShadows() const { return mData.npcsReceiveShadows; }
    bool worldReceivesShadows() const { return mData.worldReceivesShadows; }
    bool effectsReceiveShadows() const { return mData.effectsReceiveShadows; }
    float effectShadowCutoffAlpha() const { return mData.effectShadowCutoffAlpha; }

private:
    ShadowConfig() = default;

    ShadowConfigData mData;
    std::string mPath;
    int64_t mLastModified = 0;
};
