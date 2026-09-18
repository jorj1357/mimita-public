// 09 17 2026
/* purpose
* Physical exact-pose animation contract. Persistent per-actor state, collision
* recipes, and the contact-result / event vocabulary shared by the fixed-tick
* exact-pose runtime, the render pose publisher, and (read-only) the tool owner.
*
* The state is a kernel-owned dynamic component so it survives a hot-reload
* generation swap with the world/session/EntityIds intact. The fixed-tick system
* is the only writer. Animation computes pose, sweep, contact, and reaction;
* the tool owner computes damage/ammo/cooldowns.
* Hot-only header: not a GameAPI context field, no kernel enum.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation-blender.h"

namespace HotPhys {

using HotAnim::AnimationMode;
using HotAnim::PoseAuthority;

// Actor-relative pose for the six MiMITA body parts. `plrOrigin` (the actor
// world transform) is owned by the gameplay/runtime system, not stored here.
// Order matches HotAnim::PartId: torso, head, leftArm, rightArm, leftLeg, rightLeg.
struct ActorPoseV1 {
    float trans[HotAnim::PartCount][3];
    float rot[HotAnim::PartCount][3];   // degrees (library unit)
    std::uint32_t mask;
    std::uint32_t reserved;

    void clear()
    {
        mask = 0;
        for (std::uint32_t i = 0; i < HotAnim::PartCount; ++i)
            for (int k = 0; k < 3; ++k) {
                trans[i][k] = 0.0f;
                rot[i][k] = 0.0f;
            }
    }
};

// Additive reaction layer. Never replaces the authored exact target.
struct PoseReactionStateV1 {
    float swayOffset[3];
    float swayVelocity[3];
    float recoilOffset[3];
    float impactOffset[3];
};

static constexpr std::uint32_t PHYSICAL_ANIMATION_STATE_VERSION = 1;

static constexpr std::uint32_t PHYS_FLAG_ACTIVE = 1u << 0;
static constexpr std::uint32_t PHYS_FLAG_BLOCKED = 1u << 1;   // collision clamped
static constexpr std::uint32_t PHYS_FLAG_COMPLETED = 1u << 2;

struct PhysicalAnimationStateV1 {
    std::uint32_t version;
    std::uint32_t byteSize;
    std::uint64_t animationId;
    std::uint64_t actionSequence;
    std::uint32_t currentTick;
    std::uint32_t durationTicks;
    std::uint32_t mode;        // AnimationMode
    std::uint32_t authority;   // PoseAuthority
    std::uint32_t flags;       // PHYS_FLAG_*
    std::uint32_t reserved;
    ActorPoseV1 target;        // exact authored pose for currentTick
    ActorPoseV1 resolved;      // post-collision pose actually used
    PoseReactionStateV1 reactions;
};

// Contact response vocabulary (equal slide/bounce behavior for all contacts).
enum class PhysicalContactResponse : std::uint32_t {
    Slide = 0,
    Bounce = 1,
    PushDynamicActor = 2,
    RedirectToAnimatingActor = 3,
};

struct PhysicalContactResultV1 {
    std::uint32_t hit;
    std::uint32_t staticWorld;
    std::uint32_t dynamicActor;
    std::uint32_t bodyPart;     // HotAnim::PartId, 0xFFFFFFFF = weapon marker
    std::uint64_t otherEntity;  // 0 for static world
    float contactPoint[3];
    float normal[3];
    float slideVelocity[3];
    float bounceVelocity[3];
    float targetImpulse[3];
    float animatingActorImpulse[3];
};

// How a body part / weapon's motion becomes gameplay force.
struct PhysicalAnimationRecipeV1 {
    std::uint32_t enabled;
    float velocityToForce;
    float maximumImpulse;
    std::uint32_t collideHead;
    std::uint32_t collideTorso;
    std::uint32_t collideArms;
    std::uint32_t collideLegs;
    std::uint32_t collideWeapon;
    std::uint32_t affectDynamicActors;
    std::uint32_t reactToStaticWorld;
    std::uint32_t reserved;
};

// Sway/recoil/impact are additive and recipe-gated; the exact pose stays target.
struct PoseReactionRecipeV1 {
    std::uint32_t allowSway;
    std::uint32_t allowRecoil;
    std::uint32_t allowImpactReaction;
    float swayStrength;
    float damping;
};

// Per-tool animation set, resolved from the tool/action by the animation owner.
struct ToolAnimationSetV1 {
    std::uint64_t idleAnimation;
    std::uint64_t equipAnimation;
    std::uint64_t primaryAnimation;
    std::uint64_t secondaryAnimation;
    std::uint64_t reloadAnimation;
    std::uint64_t hitAnimation;
    std::uint32_t mode;        // AnimationMode
    std::uint32_t authority;   // PoseAuthority
    PhysicalAnimationRecipeV1 physics;
    PoseReactionRecipeV1 reactions;
};

// ── Dynamic component ids ───────────────────────────────────────────
static constexpr std::uint64_t HOT_PHYSICAL_ANIMATION_COMPONENT =
    gameHash("PhysicalAnimationState");
static constexpr std::uint64_t HOT_ANIMATION_MODE_COMPONENT =
    gameHash("AnimationModeState");

struct AnimationModeStateV1 {
    std::uint32_t mode;         // AnimationMode (global default per actor)
    std::uint32_t authority;    // PoseAuthority
    std::uint32_t globalDefault;
    std::uint32_t reserved;
};

// ── Event ids (generic facts; animation never applies damage) ───────
static constexpr std::uint64_t ANIM_EVENT_BODY_CONTACT =
    gameHash("animation.body-contact");
static constexpr std::uint64_t ANIM_EVENT_WEAPON_CONTACT =
    gameHash("animation.weapon-contact");
static constexpr std::uint64_t ANIM_EVENT_STATIC_REACTION =
    gameHash("animation.static-reaction");
static constexpr std::uint64_t ANIM_EVENT_ACTOR_PUSH =
    gameHash("animation.actor-push");
static constexpr std::uint64_t ANIM_EVENT_POSE_BLOCKED =
    gameHash("animation.pose-blocked");
static constexpr std::uint64_t ANIM_EVENT_POSE_COMPLETED =
    gameHash("animation.pose-completed");

static constexpr std::uint64_t ANIM_EVENT_SCHEMA = gameHash("animation.physical-event.v1");

// ── Recipe selection ────────────────────────────────────────────────
inline PhysicalAnimationRecipeV1 defaultPhysicalRecipe()
{
    PhysicalAnimationRecipeV1 r{};
    r.enabled = 1;
    r.velocityToForce = 4.0f;
    r.maximumImpulse = 250.0f;
    r.collideHead = 1;
    r.collideTorso = 1;
    r.collideArms = 1;
    r.collideLegs = 0;
    r.collideWeapon = 1;
    r.affectDynamicActors = 1;
    r.reactToStaticWorld = 1;
    return r;
}

inline PoseReactionRecipeV1 defaultReactionRecipe()
{
    PoseReactionRecipeV1 r{};
    r.allowSway = 1;
    r.allowRecoil = 1;
    r.allowImpactReaction = 1;
    r.swayStrength = 1.0f;
    r.damping = 12.0f;
    return r;
}

inline PhysicalAnimationRecipeV1 physicalRecipeForAction(std::uint64_t actionId)
{
    PhysicalAnimationRecipeV1 r = defaultPhysicalRecipe();
    // Only weapon-swing actions sweep the world in the first slice. Locomotion
    // and presentation actions keep the authored pose without force transfer.
    if (actionId != HOT_ACTION_SLASH && actionId != HOT_ACTION_LUNGE) {
        r.enabled = 0;
        r.affectDynamicActors = 0;
        r.reactToStaticWorld = 0;
    }
    return r;
}

} // namespace HotPhys
