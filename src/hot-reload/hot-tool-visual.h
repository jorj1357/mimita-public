// 09 16 2026
/* purpose
* ONE generic tool/presentation recipe vocabulary for hot C++. A weapon is a
* recipe (held model + transform, muzzle flash, dynamic light, projectile
* appearance, trail, impact, explosion, sounds), never a renderer branch. The
* recipe is pure hot data selected by a stable tool key hash (gameHash(weaponId)),
* so a future tool is a new recipe with no new EXE enum/slot.
* Server-disagreement presentation is the same idea: a recipe owns pulse/beam/
* tracer/text/particle/sound appearance so config JSON is fallback only.
* Hot-only header: not a GameAPI context field. Structs never cross the hot
* boundary as owning types; they only carry fixed plain data.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation-clips.h"

// Bright flash geometry + optional dynamic light at the muzzle.
struct ToolMuzzleVisualV1 {
    std::uint64_t meshId;      // logical mesh (default HOT_MESH_SPHERE)
    std::uint64_t textureId;   // 0 = untextured (color is used directly)
    float color[4];
    float scale;
    float lifetime;            // seconds (one 60 Hz tick by default)
    float growth;              // scale growth per second (0 = constant)
    float fadeStart;           // age fraction where alpha starts fading
    // Optional dynamic light driven through the existing effect.spawn manager.
    std::uint32_t hasLight;
    std::uint32_t reserved;
    float lightColor[3];
    float lightIntensity;
    float lightRadius;
    float lightLifetime;
    float lightOffset[3];      // relative to the muzzle position
};

// Projectile presentation only. Gameplay authority stays in the hot projectile
// simulation; this must never decide damage, collision, or authority.
struct ToolProjectileVisualV1 {
    std::uint64_t meshId;
    std::uint64_t textureId;
    float scale;
    float color[4];
    float glow;
    float length;
    float radius;
    std::uint32_t orientFromVelocity;  // 1 = orient +Z along velocity
    std::uint32_t reserved;
};

struct ToolTrailVisualV1 {
    std::uint64_t meshId;
    float color[4];
    float rate;             // emissions per second
    float size;
    float endSize;
    float lifetime;
    std::uint32_t enabled;
    std::uint32_t reserved;
};

struct ToolImpactVisualV1 {
    float color[4];
    float scale;
    float lifetime;
    std::uint32_t particleCount;
    std::uint32_t reserved;
};

struct ToolExplosionVisualV1 {
    float flashColor[4];
    float smokeColor[4];
    float debrisColor[4];
    float scale;
    float lifetime;
    std::uint32_t debrisCount;
    std::uint32_t reserved;
};

struct ToolSoundSetV1 {
    const char* fire;
    const char* reload;
    const char* equip;
    const char* unequip;
};

// ── Per-tool animation phases ───────────────────────────────────────
// A tool owns the arm poses for each of its phases as procedural C++ keyframes
// (same units/engine as the body clip library). The body state machine selects
// the phase; pose generation resolves the equipped tool's phase clip. A missing
// phase falls back to the generic body clip, so partial tools stay safe.
static constexpr std::uint64_t TOOL_PHASE_IDLE = gameHash("tool.phase.idle");
static constexpr std::uint64_t TOOL_PHASE_SHOOT = gameHash("tool.phase.shoot");
static constexpr std::uint64_t TOOL_PHASE_JUST_SHOT = gameHash("tool.phase.just-shot");
static constexpr std::uint64_t TOOL_PHASE_RELOAD = gameHash("tool.phase.reload");
static constexpr std::uint64_t TOOL_PHASE_EQUIP = gameHash("tool.phase.equip");
static constexpr std::uint64_t TOOL_PHASE_UNEQUIP = gameHash("tool.phase.unequip");
static constexpr std::uint64_t TOOL_PHASE_SLASH = gameHash("tool.phase.slash");
static constexpr std::uint64_t TOOL_PHASE_LUNGE = gameHash("tool.phase.lunge");

struct ToolAnimPhaseV1 {
    std::uint64_t phaseId;
    float duration;       // seconds
    std::uint32_t loop;   // 1 = loop, 0 = one-shot
    std::uint32_t mask;   // HotAnim::MaskUpper / MaskArms / MaskFull ...
    std::uint32_t frameCount;
    const HotAnim::Keyframe* frames;
};

struct ToolParamV1 {
    const char* key;   // customParams key
    float value;
};

// The single held-tool definition: gameplay data + held presentation + per-phase
// animation. `presentMask` (GameToolFieldFlags) marks which gameplay groups the
// hot side makes authoritative for the cold resolver; unset groups keep the
// existing JSON/builtin value.
struct ToolDefinitionV1 {
    std::uint64_t presentMask;
    const char* id;             // stable weapon id (registry + network key)
    const char* displayName;
    // Gameplay.
    std::uint32_t behaviorType;  // WeaponBehaviorType numeric
    std::uint32_t fireMode;      // WeaponFireMode numeric
    std::uint32_t networkMode;   // WeaponNetworkMode numeric
    std::uint32_t hitscan;
    std::uint32_t slot;
    float damage;
    float headshotMultiplier;
    float fireDelay;
    float reloadTime;
    float spread;
    float recoil;
    float equipPoseTime;
    float unequipPoseTime;
    float projectileSpeed;
    float projectileRadius;
    float projectileLifetime;
    std::int32_t magazineSize;
    std::int32_t reserveAmmo;
    std::int32_t pelletCount;
    const char* soundHit;
    const char* soundDryFire;
    std::uint32_t paramCount;
    const ToolParamV1* params;
    // Animation phases.
    std::uint32_t phaseCount;
    const ToolAnimPhaseV1* phases;
};

// The single held-tool presentation + definition recipe.
struct ToolVisualRecipeV1 {
    std::uint64_t toolKey;     // gameHash(weaponId)
    // Held model + material.
    const char* modelPath;     // null/"" => recipe is incomplete (cold owns)
    std::uint64_t meshId;      // logical mesh id
    std::uint64_t textureId;   // logical texture id (0 = untextured)
    const char* texturePath;   // optional (null = none)
    // Hand attachment and local transforms.
    std::uint64_t socket;      // gameHash("rightArm")
    float viewPosition[3];
    float viewRotation[4];     // quaternion xyzw
    float viewScale;
    float worldPosition[3];
    float worldRotation[4];
    float worldScale;
    float muzzleOffset[3];     // local muzzle point in attachment space
    // Sub-visuals.
    ToolMuzzleVisualV1 muzzle;
    ToolProjectileVisualV1 projectile;
    ToolTrailVisualV1 trail;
    ToolImpactVisualV1 impact;
    ToolExplosionVisualV1 explosion;
    ToolSoundSetV1 sounds;
    // Gameplay + animation definition (authoritative for cold execution).
    ToolDefinitionV1 definition;
    std::uint32_t flags;       // bit0 = complete recipe
    std::uint32_t reserved;
};

// Server-disagreement presentation recipe (replaces the JSON appearance values
// once the hot path is proven equivalent; JSON stays the fallback until then).
struct ServerDisagreementVisualV1 {
    float pulseColor[4];
    float pulseStartScale;
    float pulseEndScale;
    float pulseAlpha;
    float pulseLifetime;
    float beamHeight;
    float beamThickness;
    float beamLifetime;
    float beamColor[4];
    float tracerThickness;
    float tracerLifetime;
    float tracerColor[4];
    const char* textPrefix;
    float textScale;
    float textZOffset;
    float textLifetimeExtra;
    std::uint32_t particleCount;
    float particleMinSpeed;
    float particleMaxSpeed;
    float particleGravity;
    float particleLifetime;
    float particleScale;
    float particleEndScale;
    float particleAlpha;
    float particleColor[4];
    const char* sound;
    float soundVolumeMin;
    float soundVolumeMax;
    float soundRange;
    // Severity scaling by correction magnitude.
    float smallCorrectionMag;
    float largeCorrectionMag;
    float smallScale;
    float largeScale;
    float minEffectInterval;   // seconds between composed effects
    float reasonColor[10][4];
};

// Local-only correction indicator recipe (arrow + label for the corrected peer).
struct LocalDisagreementVisualV1 {
    float arrowColor[4];
    float arrowLifetime;
    float arrowThickness;
    float arrowAlpha;
    float textColor[4];
    float textZOffset;
    float textLifetime;
    float textScale;
    const char* textLabel;
};

// Recipe registry. Stable tool-key selection; no weapon enum.
const ToolVisualRecipeV1* findToolVisual(std::uint64_t toolKey);
// Projectile type (network id) -> recipe, so projectile presentation can be
// recovered on a client even when the component did not replicate.
const ToolVisualRecipeV1* findProjectileVisual(std::uint64_t projectileTypeId);

// Compose the shared explosion presentation (flash/smoke/debris/sound) for a
// rocket/grenade effect type at a world position. Defined by the hot effect
// composition module; lets the hot projectile simulation produce the detonation
// visual without a cold call site.
void hotComposeExplosion(GameplayContextV1* ctx, std::uint64_t effectTypeId,
                         const float position[3], float scale);
// Create the client-only tick explosion timeline (data-driven layers). Defined
// by the hot hit-visuals module.
void hotSpawnExplosionTimeline(GameplayContextV1* ctx, const float position[3],
                               float scale, bool grenade);

// Compose the full hit feedback (blood, bullet holes, cracks, impact spheres,
// damage numbers, tick-burst timeline) for an `effect.hit.*` fact. Defined by
// the hot hit-visuals module; reuses the existing cold EffectPart/decals.
void hotComposeHit(GameplayContextV1* ctx, const EffectRequestV1& req);
ToolVisualRecipeV1 makeRevolverVisual();
ToolVisualRecipeV1 makeShotgunVisual();
ToolVisualRecipeV1 makeRocketLauncherVisual();
ToolVisualRecipeV1 makeGrenadeLauncherVisual();
ToolVisualRecipeV1 makeSpyknifeVisual();
ToolVisualRecipeV1 makeSwordswordVisual();
ServerDisagreementVisualV1 makeServerDisagreementVisual();
LocalDisagreementVisualV1 makeLocalDisagreementIndicatorVisual();

// Reason -> recipe color (index by DisagreementReason value, bounds-checked).
const float* serverDisagreementReasonColor(const ServerDisagreementVisualV1& recipe,
                                           std::uint32_t reason);
