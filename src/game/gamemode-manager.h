// 09 06 2026, 00 00
/* purpose
* Generic gamemode manager that renders HUD, world elements, and mode-specific
* visuals based on feature declarations in the active gamemode's JSON config.
* Data-driven: any new gamemode that declares features in its JSON will
* automatically get the correct rendering without C++ code changes.
* Does NOT run gameplay simulation — the server owns all gameplay decisions.
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

// ── Generic Gamemode Manager ──────────────────────────────────────────
// Reads the current mode's features from CommunityMatchClient + GamemodeRegistry.
// Renders HUD text, world elements, and mode-specific visuals.
// Adding a new feature = adding a case in the renderer + a JSON feature flag.
class GamemodeManager {
public:
    void start();   // Called when a community match begins
    void stop();    // Called when a community match ends
    void update(float dt, Player& player);
    void renderHud();
    void renderWorldElements(Camera& camera, Player& player);

    bool enabled() const { return mEnabled; }

    // ── Generic state queries (read from CommunityMatchClient) ───────
    bool isActive() const;
    bool isCountdownActive() const;
    bool isMatchEnd() const;
    const char* currentModeId() const;

    // ── Feature-specific queries ─────────────────────────────────────
    // These check the current gamemode's features, not hardcoded mode names.
    bool hasFeature(const char* featureName) const;

    // ── Bomb Tag specific (driven by features, not mode name) ────────
    bool playerIsBombHolder(uint32_t localPlayerId) const;
    const char* bombHolderName(uint32_t localPlayerId, const Player& player) const;
    float bombSecondsRemaining() const;
    bool bombIsActive() const;
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

    // Sound state tracking (detect changes from server replication)
    uint32_t mPrevBombTimerTicks = 0;
    uint32_t mPrevBombHolderId = 0;
    bool mInactiveSoundPlaying = false;

    // ── Feature-based renderers ──────────────────────────────────────
    // Each renders only when its feature flag is true in the gamemode JSON.
    void renderBombHolderText();
    void renderBombVisual(Camera& camera, Player& player);
    void renderPassEffect(Camera& camera);
    void renderBombSphere(const glm::vec3& pos, float timerTicks, bool isActive);
    void renderWorldTimer(const glm::vec3& pos, float seconds);
};

void setArmToWeaponPose(Player& p, bool hasBomb);
