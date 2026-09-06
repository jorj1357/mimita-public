// 09 06 2026, 00 00
/* purpose
* Client-side Bomb Tag manager that renders HUD, bomb visual, and pass effects
* from server-authoritative replicated state received via BombTagStatePacket.
* Does NOT run its own simulation, decide bomb ownership, manage timers,
* or determine pass validity — the server owns all gameplay decisions.
* Does NOT restrict weapons or manage player death — those are handled by
* the server-side match system and existing death/respawn infrastructure.
* Does NOT produce per-frame log spam — uses throttled debug logging.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct GLFWwindow;
class Player;
class Camera;

// ── Bomb Tag client state ─────────────────────────────────────────────
// Consumed from CommunityMatchClient replicated state. Renders HUD and
// bomb visual from authoritative server data.
class BombTagManager {
public:
    void start();   // Called when bomb tag match begins (replicated state arrives)
    void stop();    // Called when bomb tag match ends
    void update(float dt, Player& player);
    void renderHud();
    void renderBombVisual(Camera& camera, Player& player);
    void renderPassEffect(Camera& camera);

    bool enabled() const { return mEnabled; }

    // ── Replicated state accessors ───────────────────────────────────
    // These read from CommunityMatchClient and return the server's truth.
    bool isActive() const;
    bool isCountdownActive() const;
    bool isMatchEnd() const;
    bool playerIsBombHolder(uint32_t localPlayerId) const;
    const char* bombHolderName(uint32_t localPlayerId, const Player& player) const;
    float bombSecondsRemaining() const;
    bool bombIsActive() const;  // true when inactive ticks == 0
    glm::vec3 bombWorldPosition() const;

    void setCamera(class Camera& cam) { mCamera = &cam; }

private:
    bool mEnabled = false;
    Camera* mCamera = nullptr;

    // Pass beam visualization
    float mPassBeamTimer = 0.0f;
    glm::vec3 mPassBeamStart{0.0f};
    glm::vec3 mPassBeamEnd{0.0f};

    // Bomb blink state (client-side tick counting for visual only)
    uint32_t mClientBombTick = 0;

    void renderBombSphere(const glm::vec3& pos, float timerTicks, bool isActive);
    void renderWorldTimer(const glm::vec3& pos, float seconds);
};

void setArmToWeaponPose(Player& p, bool hasBomb);
