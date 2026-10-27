// 2026-10-17
/* purpose
* Owns the data-driven Melee move-set component parsed from config/weapons.json.
* One WeaponDefinition id (e.g. captain_falcon_moveset) owns an ordered action list;
* each action owns input conditions, windup/active/recovery ticks, per-phase pose
* clips, limb-attached hitboxes, percent/knockback/shield damage, phase impulses,
* and tick-addressed audio with pitch/volume/spatial offset.
* Does NOT implement action state, hit sweeps, damage, movement, or networking.
* Does NOT add a C++ definition function per move; JSON selects and configures this.
* Does NOT expose arbitrary executable behavior from JSON.
*/

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace WeaponData {

enum class MeleeButton {
    Primary,   // left click / A
    Secondary, // right click / B
    Grab,      // E
    Shield     // Q (held)
};

enum class MeleeActorState {
    Any,
    Grounded,
    Airborne
};

// A modifier is an additional held/edge button that selects a chorded variant.
enum class MeleeModifier {
    None,
    Jump,   // Space
    Crouch  // Ctrl (down)
};

enum class MeleeHitboxAttachmentMode : uint8_t {
    CameraBasis = 0, // legacy compatibility
    AnimatedLimb = 1
};

struct MeleePhase {
    int ticks = 1;          // 1..999
    std::string clipId;     // phase clip/pose reference (may be empty for fallback)
};

struct MeleeHitboxTick {
    int tick = 0;                       // active-phase-local tick
    std::string attachedPart;
    std::string shape = "sphere";
    glm::vec3 size{0.5f};
    glm::vec3 localOffset{0.0f};
};

struct MeleeHitbox {
    std::string id;
    bool valid = true;
    std::string attachedPart;          // head|torso|leftArm|rightArm|leftLeg|rightLeg
    std::string shape = "sphere";      // sphere|capsule|box
    glm::vec3 size{0.5f};              // sphere: radius in x; capsule/box: extents
    glm::vec3 localOffset{0.0f};       // in the attached part's local frame (meters)
    MeleeHitboxAttachmentMode attachmentMode = MeleeHitboxAttachmentMode::AnimatedLimb;
    // Active window is phase-local and validated to the active phase; both are
    // clamped so the window stays inside the action's active phase.
    int activeStartTick = 0;
    int activeEndTick = 0;
    std::vector<MeleeHitboxTick> perTick;
};

inline MeleeHitbox resolveMeleeHitboxForTick(const MeleeHitbox& hitbox, int tick)
{
    MeleeHitbox resolved = hitbox;
    resolved.perTick.clear();
    for (const MeleeHitboxTick& override : hitbox.perTick) {
        if (override.tick != tick)
            continue;
        if (!override.attachedPart.empty()) resolved.attachedPart = override.attachedPart;
        resolved.shape = override.shape;
        resolved.size = override.size;
        resolved.localOffset = override.localOffset;
        break;
    }
    return resolved;
}

struct MeleeImpulse {
    std::string phase = "active";      // windup|active|recovery
    int tick = 0;                      // phase-local tick
    float forward = 0.0f;              // captured camera-forward
    float right = 0.0f;                // captured camera-right
    float up = 0.0f;                   // world +Z
};

struct MeleeAudioEvent {
    std::string phase = "active";      // windup|active|recovery
    int tick = 0;                      // phase-local tick
    std::string soundId;
    float pitch = 1.0f;
    float volume = 1.0f;
    glm::vec3 offset{0.0f};            // meters
};

// A small pose recipe used when an action has no authored clip yet. The
// rightArm/torso rotations are in the character frame; the weapon follows the
// arm transform, so the visible blade and its contact shape stay aligned.
struct MeleeProceduralPose {
    glm::vec3 rightArmRotation{60.0f, 5.0f, 40.0f};
    glm::vec3 rightArmTranslation{0.0f};
    glm::vec3 torsoRotation{0.0f};
    bool valid = false;
};

struct MeleeAction {
    std::string id;
    MeleeButton button = MeleeButton::Primary;
    MeleeActorState actorState = MeleeActorState::Grounded;
    MeleeModifier modifier = MeleeModifier::None;
    // Same-tick modifier tie-break: higher priority wins.
    int priority = 0;
    // A second press of the same button within this window selects
    // `doubleClickAction` (when non-empty). 30 is the spec default.
    int doubleClickWindowTicks = 30;
    std::string doubleClickAction;
    int tapCount = 1;
    bool holdWhileButton = false;

    MeleePhase windup;
    MeleePhase active;
    MeleePhase recovery;
    // New-format actions use one clip whose markers define these phase lengths.
    // Empty means this action still uses the explicit legacy phases during migration.
    std::string animationClipId;
    // Blender-authored boundaries used to report the phase retiming mapping.
    std::array<int, 4> animationSourceTicks{{0, 0, 0, 0}};

    std::vector<MeleeHitbox> hitboxes;

    float basePercent = 0.0f;
    bool hasShieldDamage = false;      // when false, move percent is the shield damage
    float shieldDamage = 0.0f;
    float baseKnockback = 0.0f;
    float knockbackVerticalFraction = 0.2f;
    float knockbackForceScale = 1.0f;

    bool isGrab = false;
    float grabRadius = 1.0f;           // meters
    int grabPullTicks = 30;

    // Continuous-contact health damage overrides. Sentinel -1 means inherit
    // the equipped weapon's level value.
    float contactMinDamage = -1.0f;
    float contactMaxDamage = -1.0f;
    float contactMinKnockback = -1.0f;
    float contactMaxKnockback = -1.0f;
    float contactMaxForceSpeed = -1.0f;
    float contactDamageMultiplier = 1.0f;
    bool blocksMelee = false;
    float blockArcDegrees = 180.0f;
    bool npcSelectable = true;

    MeleeProceduralPose proceduralBase;
    MeleeProceduralPose proceduralWindup;
    MeleeProceduralPose proceduralActive;
    MeleeProceduralPose proceduralRecovery;
    bool hasProceduralPose = false;

    std::vector<MeleeImpulse> impulses;

    std::string onStartSound;
    float onStartPitch = 1.0f;
    float onStartVolume = 1.0f;
    glm::vec3 onStartOffset{0.0f};
    std::vector<MeleeAudioEvent> audioEvents;

    int totalTicks() const { return windup.ticks + active.ticks + recovery.ticks; }
};

// Interpolates the placeholder pose recipe for the given action phase.
// Returns false when the action has no procedural pose data.
bool resolveMeleeProceduralPose(const MeleeAction& action, const char* phase,
                                int phaseTick, int phaseTicks,
                                MeleeProceduralPose& out);

struct MeleeMoveSet {
    std::string definitionId;
    bool animationOnly = false; // continuous-contact weapons reuse actions for presentation
    uint32_t revision = 0;
    uint32_t hash = 0;
    std::vector<MeleeAction> actions;

    const MeleeAction* find(const std::string& actionId) const
    {
        for (const MeleeAction& a : actions)
            if (a.id == actionId)
                return &a;
        return nullptr;
    }
};

// Process-wide registry of parsed move sets keyed by WeaponDefinition id.
// The owning loader publishes a new revision atomically; in-progress actions
// keep the revision pointer they started with (consumers hold the move set).
class MeleeMoveSetRegistry {
public:
    static MeleeMoveSetRegistry& instance();

    void set(MeleeMoveSet set);
    const MeleeMoveSet* get(const std::string& definitionId) const;
    void clear();

private:
    MeleeMoveSetRegistry() = default;
    std::unordered_map<std::string, MeleeMoveSet> mSets;
};

// Parses root["components"]["melee"] for `definitionId`. Returns false when the
// component is absent or has no usable actions; `out` is left untouched.
bool parseMeleeMoveSet(const std::string& definitionId,
                       const nlohmann::json& root,
                       MeleeMoveSet& out);

} // namespace WeaponData
