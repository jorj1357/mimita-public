#pragma once
#include <glm/glm.hpp>

class Player;

enum class DashQuality {
    Perfect = 0,
    Excellent = 1,
    Good = 2,
    Okay = 3,
    Poor = 4
};

// Returns the impulse multiplier for a given dash quality.
inline float dashQualityMultiplier(DashQuality q) {
    switch (q) {
        case DashQuality::Perfect:   return 1.00f;
        case DashQuality::Excellent: return 0.85f;
        case DashQuality::Good:      return 0.70f;
        case DashQuality::Okay:      return 0.55f;
        case DashQuality::Poor:      return 0.40f;
    }
    return 0.40f;
}

// Returns quality from airborne movement tick count.
inline DashQuality dashQualityFromTicks(int ticks) {
    if (ticks <= 1) return DashQuality::Perfect;
    if (ticks == 2) return DashQuality::Excellent;
    if (ticks == 3) return DashQuality::Good;
    if (ticks == 4) return DashQuality::Okay;
    return DashQuality::Poor;
}

// Returns quality from movement hold duration (seconds).
// Hold duration < 1 tick = perfect.
inline DashQuality dashQualityFromHoldDuration(float holdDuration, float tickDt) {
    return holdDuration < tickDt ? DashQuality::Perfect : DashQuality::Excellent;
}

void doAirDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpTriggered,
    bool movementPressed,
    bool airborne,
    int movementTicks,
    float movementHeldDuration,
    float dt,
    const glm::vec3& camForward = glm::vec3(1.0f, 0.0f, 0.0f)
);
