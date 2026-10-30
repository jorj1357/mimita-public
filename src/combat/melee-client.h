// 2026-10-17
/* purpose
* Owns the local client Melee action driver for Captain Falcon move sets.
* Resolves LMB/RMB/E + Space/Ctrl held modifiers and grounded/airborne state to
* a data-driven action, captures camera-horizontal-forward at action start,
* sends the action to the server via the generic AttackRequest, and drives the
* local action animation through Player::playOneShotPhase. Self-impulses are
* predicted through the local external-impulse owner.
* Does NOT own hit confirmation, percent, or knockback authority (server owns
* those; Phase 7 adds prediction/referee for contact).
* Does NOT add per-move C++ behavior; all values come from config/weapons.json.
*/

#pragma once

#include <cstdint>

class Camera;
class Player;
class WeaponSystem;

namespace MimitaNet { struct MultiplayerContext; }

namespace MimitaMelee {

struct ClientInput {
    bool primaryPressed = false;   // LMB edge this frame
    bool primaryHeld = false;      // LMB held this frame (arm-ragdoll motor)
    bool secondaryPressed = false; // RMB edge this frame
    bool secondaryHeld = false;
    bool grabPressed = false;      // E edge this frame
    bool shieldHeld = false;       // Q held
    bool jumpHeld = false;         // Space held
    bool crouchHeld = false;       // Ctrl held
    bool movementPressed = false;  // movement input this fixed tick
};

// True when the player's currently equipped definition has a parsed Melee move
// set. The client uses this to route LMB/RMB/E to the Melee driver instead of
// the ordinary weapon fire.
bool hasEquippedMoveSet(const Player& player, WeaponSystem& weapons);

// Advance the local Melee action by one fixed tick (or start one on an input
// edge). `simTick` is the client fixed-tick counter. Safe to call every render
// frame; the phase machine only advances when `simTick` changes.
void clientTick(Player& player,
                const Camera& camera,
                WeaponSystem& weapons,
                MimitaNet::MultiplayerContext& ctx,
                const ClientInput& input,
                uint32_t simTick);

// Clear the local action (spawn/death/mode change).
void reset();

// Render the local attack's configured hitbox while its active window is open.
// This is presentation only; the server remains the contact authority.
void drawActiveHitbox(const Player& player, const Camera& camera);

// Current held-shield intent for the local player (Q). Read by the input/report
// path so the server can run the Melee shield. False when no move set is active.
bool shieldHeld();

// Test/dev hook: force-start a named action on the next fixed tick, as if the
// matching input chord were pressed. Used by agent.* live-test commands.
void debugRequestAction(const std::string& actionId);

// Test/dev hook: force the crouch input state (agent.crouch).
void debugSetCrouch(bool crouch);
bool debugCrouchHeld();

// Test/dev hook: hold/release the shield input state (agent.shield).
void debugSetShield(bool shield);
bool debugShieldHeld();
// Press/release through the same fixed-tick button resolver as mouse input.
void requestButton(bool secondary, bool held);

// Read-only scenario/diagnostic view of the local action instance.
bool currentActionActive();
const std::string& currentActionDefinitionId();
const std::string& currentActionId();
uint32_t currentActionStartTick();
int currentActionPhaseTick();
uint32_t currentActionRevision();

} // namespace MimitaMelee
