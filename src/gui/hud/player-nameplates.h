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
    const char* debugPrefix,
    const char* sourceTag = "live_world");

const char* healthbarCullReasonName(HealthbarCullReason reason);
void resetHealthbarCounters();
int getHealthbarTotal();
int getHealthbarLiveWorld();
int getHealthbarInvalid();

// Aim-mode healthbar simplification
struct HealthbarAimState {
    float transitionAlpha = 0.0f;  // 0 = normal healthbar, 1 = triangle only
    int inAimCount = 0;
};

bool isActorInAimCone(const glm::vec3& camPos, const glm::vec3& camFront,
                      const glm::vec3& targetPos, float coneDegrees);
HealthbarAimState& getOrCreateAimState(const Player& player);
glm::vec4 healthColorForHp(int currentHp, int maxHp);

// Debug
bool isHealthbarDebugEnabled();
void setHealthbarDebugEnabled(bool enabled);
void drawHealthbarDebugOverlay(const Camera& camera);
