#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct Camera;
class Player;

enum class HealthbarCullReason : uint8_t
{
    None,
    Dead,
    TooFar,
    Offscreen
};

struct HealthbarRenderResult
{
    glm::vec3 anchor{0.0f};
    glm::vec2 screen{0.0f};
    float distance = 0.0f;
    bool usedHeadTransform = false;
    bool rendered = false;
    HealthbarCullReason cullReason = HealthbarCullReason::None;
};

glm::vec3 playerHealthbarAnchor(
    const Player& player,
    bool* usedHeadTransform = nullptr);

HealthbarRenderResult drawPlayerHealthbar(
    const Player& player,
    const Camera& camera,
    const char* debugPrefix);

const char* healthbarCullReasonName(HealthbarCullReason reason);
