// aug 18 2026, 14 30
/* purpose
* Declares the DynamicLight struct and DynamicLightManager singleton.
* Owns a fixed-size pool of dynamic point lights for muzzle flashes, explosions, etc.
* Provides spawn, update, and submitToShader for the rendering pipeline.
* Does NOT own shader compilation or uniform upload (render-world-mesh.cpp handles that).
* Does NOT own config loading or hot-reload (DynamicLightConfig handles that).
*/
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <glm/glm.hpp>

struct DynamicLight {
    uint32_t id = 0;
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 5.0f;
    float lifetime = 0.1f;
    float age = 0.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    bool active = false;
};

class DynamicLightManager {
public:
    static DynamicLightManager& instance();

    static constexpr int MAX_LIGHTS = 32;
    static constexpr int MAX_SUBMIT = 8;

    DynamicLight* spawn(const glm::vec3& position, const glm::vec3& color,
                        float intensity, float radius, float lifetime,
                        float fadeIn = 0.0f, float fadeOut = 0.0f);

    void update(float dt);

    struct SubmittedLight {
        glm::vec3 position;
        glm::vec3 color;
        float radius;
        float intensity;
    };

    struct SubmitResult {
        int count = 0;
        SubmittedLight lights[MAX_SUBMIT];
    };

    SubmitResult submitToShader(const glm::vec3& cameraPos, int maxCount = MAX_SUBMIT) const;

    void clear();
    void resetStats();

    int activeCount() const;
    int submittedCount() const { return mSubmittedCount; }

    void enableDebugOverlay(bool on) { mDebugOverlay = on; }
    bool debugOverlay() const { return mDebugOverlay; }

private:
    DynamicLightManager() = default;

    std::array<DynamicLight, MAX_LIGHTS> mLights{};
    uint32_t mNextId = 1;
    mutable int mSubmittedCount = 0;
    bool mDebugOverlay = false;
};
