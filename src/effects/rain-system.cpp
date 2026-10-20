#include "effects/rain-system.h"

#include <algorithm>
#include <random>

#include "camera.h"
#include "debug/debug-visuals.h"

namespace {
constexpr int kDropCount = 420;
constexpr float kRadius = 22.0f;
constexpr float kHeight = 26.0f;
}

RainSystem& RainSystem::instance()
{
    static RainSystem rain;
    return rain;
}

void RainSystem::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

void RainSystem::updateAndRender(const Camera& camera, float dt)
{
    if (!mEnabled) return;
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> rx(-kRadius, kRadius);
    std::uniform_real_distribution<float> ry(-kRadius, kRadius);
    std::uniform_real_distribution<float> rz(0.0f, kHeight);
    std::uniform_real_distribution<float> rs(18.0f, 34.0f);

    if (!mSeeded || (int)mDrops.size() != kDropCount) {
        mDrops.resize(kDropCount);
        for (auto& drop : mDrops) {
            drop.position = camera.pos + glm::vec3(rx(rng), ry(rng), rz(rng));
            drop.speed = rs(rng);
        }
        mSeeded = true;
    }

    const float step = std::max(0.0f, dt);
    for (auto& drop : mDrops) {
        drop.position.z -= drop.speed * step;
        if (drop.position.z < camera.pos.z - 6.0f) {
            drop.position = camera.pos + glm::vec3(rx(rng), ry(rng), kHeight);
            drop.speed = rs(rng);
        }
        const glm::vec3 tip = drop.position + glm::vec3(0.0f, 0.0f, -0.9f);
        DebugVis::drawLine(camera, drop.position, tip, glm::vec4(0.55f, 0.62f, 0.72f, 0.5f));
    }
}

