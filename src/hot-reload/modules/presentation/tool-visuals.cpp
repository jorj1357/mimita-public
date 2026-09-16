// 09 16 2026
/* purpose
* The single hot recipe registry for tool/presentation visuals. Each weapon is a
* `ToolVisualRecipeV1` selected by a stable tool key (gameHash(weaponId)); there
* is no weapon enum, switch, or renderer branch. Server-disagreement appearance
* is a recipe on the same registry idea.
* Editing this file and saving changes the running client's presentation with no
* EXE rebuild (the recipes are re-read every frame).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-tool-visual.h"

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"

#include "combat/weapon-types.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

// Compact authoring macros for tool phase keyframes (part order: torso, head,
// leftArm, rightArm, leftLeg, rightLeg). Values are degrees. Locomotion phases
// (idle/carry) drive arms only; weapon actions drive the whole upper body
// (torso+head+arms) so the motion reads strongly.
#define WPN_ARMS(t, lrx, lry, lrz, rrx, rry, rrz)                              \
    {t, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,lrx,lry,lrz},                    \
         {0,0,0,rrx,rry,rrz}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}}
#define WPN_UPPER(t, torx, tory, torz, hdx, hdy, hdz, lrx, lry, lrz, rrx, rry, rrz) \
    {t, {{0,0,0,torx,tory,torz}, {0,0,0,hdx,hdy,hdz}, {0,0,0,lrx,lry,lrz},     \
         {0,0,0,rrx,rry,rrz}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}}
#define WPN_PH(phase, dur, loop, mask, frames)                                 \
    {phase, dur, loop, mask,                                                   \
     (std::uint32_t)(sizeof(frames) / sizeof(HotAnim::Keyframe)), frames}

ToolVisualRecipeV1 makeBaseVisual(std::uint64_t toolKey, const char* modelPath,
                                  std::uint64_t meshId)
{
    ToolVisualRecipeV1 r{};
    r.toolKey = toolKey;
    r.modelPath = modelPath;
    r.meshId = meshId;
    // 0 = use the model's own embedded GLB texture (correct per-weapon material).
    r.textureId = 0;
    r.texturePath = nullptr;
    r.socket = gameHash("rightArm");
    r.viewRotation[3] = 1.0f;
    r.viewScale = 1.0f;
    r.worldRotation[3] = 1.0f;
    r.worldScale = 1.0f;
    // Muzzle default: bright untextured sphere, one 60 Hz tick.
    r.muzzle.meshId = HOT_MESH_SPHERE;
    r.muzzle.textureId = 0;
    r.muzzle.color[0] = 1.0f;
    r.muzzle.color[1] = 0.92f;
    r.muzzle.color[2] = 0.62f;
    r.muzzle.color[3] = 1.0f;
    r.muzzle.scale = 0.16f;
    r.muzzle.lifetime = 1.0f / 60.0f;
    r.muzzle.growth = 0.0f;
    r.muzzle.fadeStart = 0.5f;
    r.impact.color[0] = 1.0f;
    r.impact.color[1] = 0.8f;
    r.impact.color[2] = 0.4f;
    r.impact.color[3] = 1.0f;
    r.impact.scale = 0.2f;
    r.impact.lifetime = 0.4f;
    r.explosion.flashColor[0] = 1.0f;
    r.explosion.flashColor[1] = 0.75f;
    r.explosion.flashColor[2] = 0.25f;
    r.explosion.flashColor[3] = 1.0f;
    r.explosion.smokeColor[0] = 0.35f;
    r.explosion.smokeColor[1] = 0.35f;
    r.explosion.smokeColor[2] = 0.35f;
    r.explosion.smokeColor[3] = 1.0f;
    r.explosion.scale = 1.0f;
    r.explosion.lifetime = 0.8f;
    r.flags = 1u;  // complete recipe
    return r;
}

void addFirearmMuzzle(ToolVisualRecipeV1& r, float sr, float sg, float sb,
                      float scale, float lightR, float lightG, float lightB,
                      float intensity, float radius)
{
    r.muzzle.color[0] = sr;
    r.muzzle.color[1] = sg;
    r.muzzle.color[2] = sb;
    r.muzzle.scale = scale;
    r.muzzle.hasLight = 1u;
    r.muzzle.lightColor[0] = lightR;
    r.muzzle.lightColor[1] = lightG;
    r.muzzle.lightColor[2] = lightB;
    r.muzzle.lightIntensity = intensity;
    r.muzzle.lightRadius = radius;
    r.muzzle.lightLifetime = 0.06f;
}

} // namespace

// ── Per-weapon animation phase tables (procedural C++ keyframes) ──────
namespace {

// Revolver.
static constexpr HotAnim::Keyframe kRevIdle[] = {
    WPN_ARMS(0.00f, -58, 0, 6, -70, 0, -4),
    WPN_ARMS(1.00f, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevShoot[] = {
    WPN_UPPER(0.00f, -8, 0, 0, -4, 0, 0, -70, 0, 10, -85, 0, -2),
    WPN_UPPER(0.05f, -17, 0, 0, -11, 0, 0, -74, 0, 12, -128, 0, -2),
    WPN_UPPER(0.14f, -6, 0, 0, -2, 0, 0, -70, 0, 10, -82, 0, -2),
};
static constexpr HotAnim::Keyframe kRevJustShot[] = {
    WPN_UPPER(0.00f, -4, 0, 0, -1, 0, 0, -70, 0, 10, -92, 0, -2),
    WPN_UPPER(0.12f, 0, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevReload[] = {
    WPN_UPPER(0.00f, 6, 0, 0, 12, 0, 0, 55, 0, 30, -60, 0, -2),
    WPN_UPPER(0.30f, 10, -14, 0, 8, -10, 0, -20, 0, 15, -58, 0, -2),
    WPN_UPPER(0.60f, 2, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevEquip[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 6, 0, 0, -20, 0, 4, -30, 0, -4),
    WPN_UPPER(0.18f, 0, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
    WPN_UPPER(0.18f, 8, 0, 0, 6, 0, 0, -20, 0, 4, -30, 0, -4),
};
static const ToolAnimPhaseV1 kRevPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kRevIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.14f, 0, HotAnim::MaskUpper, kRevShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.12f, 0, HotAnim::MaskUpper, kRevJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.60f, 0, HotAnim::MaskUpper, kRevReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.18f, 0, HotAnim::MaskUpper, kRevEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.18f, 0, HotAnim::MaskUpper, kRevUnequip),
};

// Shotgun.
static constexpr HotAnim::Keyframe kShotIdle[] = {
    WPN_ARMS(0.00f, -60, 0, 10, -72, 0, -6),
    WPN_ARMS(1.00f, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotShoot[] = {
    WPN_UPPER(0.00f, -9, 0, 0, -5, 0, 0, -72, 0, 12, -88, 0, -4),
    WPN_UPPER(0.06f, -20, 0, 0, -13, 0, 0, -76, 0, 14, -135, 0, -4),
    WPN_UPPER(0.16f, -6, 0, 0, -2, 0, 0, -72, 0, 12, -88, 0, -4),
};
static constexpr HotAnim::Keyframe kShotJustShot[] = {
    WPN_UPPER(0.00f, -5, 0, 0, -2, 0, 0, -72, 0, 12, -96, 0, -4),
    WPN_UPPER(0.14f, 0, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotReload[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 14, 0, 0, 60, 0, 34, -60, 0, -2),
    WPN_UPPER(0.30f, 12, -18, 0, 10, -12, 0, 10, 0, 26, -58, 0, -2),
    WPN_UPPER(0.55f, 8, 10, 0, 4, 8, 0, -30, 0, 14, -62, 0, 0),
    WPN_UPPER(0.80f, 2, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotEquip[] = {
    WPN_UPPER(0.00f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -6),
    WPN_UPPER(0.22f, 0, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
    WPN_UPPER(0.22f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -6),
};
static const ToolAnimPhaseV1 kShotPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kShotIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.16f, 0, HotAnim::MaskUpper, kShotShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.14f, 0, HotAnim::MaskUpper, kShotJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.80f, 0, HotAnim::MaskUpper, kShotReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.22f, 0, HotAnim::MaskUpper, kShotEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.22f, 0, HotAnim::MaskUpper, kShotUnequip),
};

// Rocket launcher.
static constexpr HotAnim::Keyframe kRocketIdle[] = {
    WPN_ARMS(0.00f, -62, 0, 8, -66, 0, -10),
    WPN_ARMS(1.00f, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketShoot[] = {
    WPN_UPPER(0.00f, -10, 0, 0, -6, 0, 0, -64, 0, 8, -80, 0, -8),
    WPN_UPPER(0.07f, -22, 0, 0, -14, 0, 0, -68, 0, 10, -120, 0, -8),
    WPN_UPPER(0.18f, -6, 0, 0, -2, 0, 0, -64, 0, 8, -80, 0, -8),
};
static constexpr HotAnim::Keyframe kRocketJustShot[] = {
    WPN_UPPER(0.00f, -5, 0, 0, -2, 0, 0, -62, 0, 8, -86, 0, -10),
    WPN_UPPER(0.14f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketReload[] = {
    WPN_UPPER(0.00f, -8, 0, 0, -4, 0, 0, 50, 0, 28, -55, 0, -6),
    WPN_UPPER(0.45f, 6, 12, 0, 4, 10, 0, -25, 0, 14, -55, 0, -6),
    WPN_UPPER(0.90f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketEquip[] = {
    WPN_UPPER(0.00f, 6, 0, 0, 5, 0, 0, -24, 0, 6, -32, 0, -8),
    WPN_UPPER(0.24f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
    WPN_UPPER(0.24f, 6, 0, 0, 5, 0, 0, -24, 0, 6, -32, 0, -8),
};
static const ToolAnimPhaseV1 kRocketPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kRocketIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.18f, 0, HotAnim::MaskUpper, kRocketShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.14f, 0, HotAnim::MaskUpper, kRocketJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.90f, 0, HotAnim::MaskUpper, kRocketReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.24f, 0, HotAnim::MaskUpper, kRocketEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.24f, 0, HotAnim::MaskUpper, kRocketUnequip),
};

// Grenade launcher.
static constexpr HotAnim::Keyframe kGrenadeIdle[] = {
    WPN_ARMS(0.00f, -60, 0, 12, -72, 0, -2),
    WPN_ARMS(1.00f, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeShoot[] = {
    WPN_UPPER(0.00f, -9, 0, 0, -5, 0, 0, -72, 0, 12, -86, 0, -2),
    WPN_UPPER(0.06f, -20, 0, 0, -13, 0, 0, -76, 0, 14, -130, 0, -2),
    WPN_UPPER(0.16f, -6, 0, 0, -2, 0, 0, -72, 0, 12, -86, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeJustShot[] = {
    WPN_UPPER(0.00f, -5, 0, 0, -2, 0, 0, -72, 0, 12, -94, 0, -2),
    WPN_UPPER(0.14f, 0, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeReload[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 14, 0, 0, 58, 0, 32, -60, 0, 0),
    WPN_UPPER(0.35f, 10, -16, 0, 8, -12, 0, -25, 0, 14, -58, 0, 0),
    WPN_UPPER(0.70f, 2, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeEquip[] = {
    WPN_UPPER(0.00f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -4),
    WPN_UPPER(0.22f, 0, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
    WPN_UPPER(0.22f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -4),
};
static const ToolAnimPhaseV1 kGrenadePhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kGrenadeIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.16f, 0, HotAnim::MaskUpper, kGrenadeShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.14f, 0, HotAnim::MaskUpper, kGrenadeJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.70f, 0, HotAnim::MaskUpper, kGrenadeReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.22f, 0, HotAnim::MaskUpper, kGrenadeEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.22f, 0, HotAnim::MaskUpper, kGrenadeUnequip),
};

// Swordsword.
static constexpr HotAnim::Keyframe kSwordIdle[] = {
    WPN_ARMS(0.00f, -30, 0, 10, -52, 0, -4),
    WPN_ARMS(1.00f, -30, 0, 10, -52, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordShoot[] = {
    WPN_UPPER(0.00f, 4, 0, 0, -2, 0, 0, -25, 0, 8, -70, 0, -4),
    WPN_UPPER(0.18f, 16, 0, 0, -8, 0, 0, -25, 0, 8, -108, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordSlash[] = {
    WPN_UPPER(0.00f, 0, -16, 0, 0, -12, 0, -20, 0, 6, -155, -6, 0),
    WPN_UPPER(0.12f, 0, 22, 0, 0, 14, 0, -25, 0, 6, -30, 45, 0),
    WPN_UPPER(0.30f, 0, 10, 0, 0, 6, 0, -15, 0, 6, 12, 22, 0),
};
static constexpr HotAnim::Keyframe kSwordLunge[] = {
    WPN_UPPER(0.00f, 24, 0, 0, -6, 0, 0, -30, 0, 6, -110, 0, -4),
    WPN_UPPER(0.28f, 34, 0, 0, -9, 0, 0, -20, 0, 6, -128, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordEquip[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 6, 0, 0, -15, 0, 6, -28, 0, -4),
    WPN_UPPER(0.20f, 0, 0, 0, 0, 0, 0, -30, 0, 10, -52, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -30, 0, 10, -52, 0, -4),
    WPN_UPPER(0.20f, 8, 0, 0, 6, 0, 0, -15, 0, 6, -28, 0, -4),
};
static const ToolAnimPhaseV1 kSwordPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kSwordIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.18f, 0, HotAnim::MaskUpper, kSwordShoot),
    WPN_PH(TOOL_PHASE_SLASH, 0.30f, 0, HotAnim::MaskUpper, kSwordSlash),
    WPN_PH(TOOL_PHASE_LUNGE, 0.28f, 0, HotAnim::MaskUpper, kSwordLunge),
    WPN_PH(TOOL_PHASE_EQUIP, 0.20f, 0, HotAnim::MaskUpper, kSwordEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.20f, 0, HotAnim::MaskUpper, kSwordUnequip),
};

// Spy knife.
static constexpr HotAnim::Keyframe kKnifeIdle[] = {
    WPN_ARMS(0.00f, -20, 0, 8, -40, 0, -6),
    WPN_ARMS(1.00f, -20, 0, 8, -40, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeShoot[] = {
    WPN_UPPER(0.00f, 2, 0, 0, -1, 0, 0, -20, 0, 8, -40, 0, -6),
    WPN_UPPER(0.10f, 10, -14, 0, -4, -10, 0, -18, 0, 8, -105, 0, -6),
    WPN_UPPER(0.25f, 0, 0, 0, 0, 0, 0, -20, 0, 8, -40, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeSlash[] = {
    WPN_UPPER(0.00f, 0, -14, 0, 0, -10, 0, -20, 0, 8, -40, 0, -6),
    WPN_UPPER(0.10f, 0, 20, 0, 0, 14, 0, -18, 0, 8, -120, 0, -6),
    WPN_UPPER(0.25f, 0, 8, 0, 0, 4, 0, -20, 0, 8, -35, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeEquip[] = {
    WPN_UPPER(0.00f, 7, 0, 0, 5, 0, 0, -12, 0, 6, -22, 0, -6),
    WPN_UPPER(0.18f, 0, 0, 0, 0, 0, 0, -20, 0, 8, -40, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -20, 0, 8, -40, 0, -6),
    WPN_UPPER(0.18f, 7, 0, 0, 5, 0, 0, -12, 0, 6, -22, 0, -6),
};
static const ToolAnimPhaseV1 kKnifePhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kKnifeIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.25f, 0, HotAnim::MaskUpper, kKnifeShoot),
    WPN_PH(TOOL_PHASE_SLASH, 0.25f, 0, HotAnim::MaskUpper, kKnifeSlash),
    WPN_PH(TOOL_PHASE_EQUIP, 0.18f, 0, HotAnim::MaskUpper, kKnifeEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.18f, 0, HotAnim::MaskUpper, kKnifeUnequip),
};

constexpr std::uint32_t kAllGameplayFields =
    GAME_TOOL_FIELD_DAMAGE | GAME_TOOL_FIELD_BEHAVIOR | GAME_TOOL_FIELD_TIMING |
    GAME_TOOL_FIELD_AMMO | GAME_TOOL_FIELD_PROJECTILE | GAME_TOOL_FIELD_MODEL |
    GAME_TOOL_FIELD_SOUNDS | GAME_TOOL_FIELD_SLOT;

void setDefinition(ToolVisualRecipeV1& r, WeaponBehaviorType behavior,
                   WeaponFireMode fireMode, WeaponNetworkMode networkMode,
                   bool hitscan, std::uint32_t slot, float damage,
                   float headshot, float fireDelay, float reloadTime, float spread,
                   float recoil, int mag, int reserve, int pellets,
                   float projSpeed, float projRadius, float projLife,
                   float equipTime, float unequipTime,
                   const ToolAnimPhaseV1* phases, std::uint32_t phaseCount)
{
    r.definition.presentMask = kAllGameplayFields;
    r.definition.behaviorType = static_cast<std::uint32_t>(behavior);
    r.definition.fireMode = static_cast<std::uint32_t>(fireMode);
    r.definition.networkMode = static_cast<std::uint32_t>(networkMode);
    r.definition.hitscan = hitscan ? 1u : 0u;
    r.definition.slot = slot;
    r.definition.damage = damage;
    r.definition.headshotMultiplier = headshot;
    r.definition.fireDelay = fireDelay;
    r.definition.reloadTime = reloadTime;
    r.definition.spread = spread;
    r.definition.recoil = recoil;
    r.definition.magazineSize = mag;
    r.definition.reserveAmmo = reserve;
    r.definition.pelletCount = pellets;
    r.definition.projectileSpeed = projSpeed;
    r.definition.projectileRadius = projRadius;
    r.definition.projectileLifetime = projLife;
    r.definition.equipPoseTime = equipTime;
    r.definition.unequipPoseTime = unequipTime;
    r.definition.phases = phases;
    r.definition.phaseCount = phaseCount;
}

void setIdentity(ToolVisualRecipeV1& r, const char* id, const char* displayName)
{
    r.definition.id = id;
    r.definition.displayName = displayName;
}

// Only the revolver supplies customParams through the hot definition today
// (values mirror config/weapons.json). Other weapons leave params empty so their
// JSON customParams remain; a brand-new hot weapon can supply its own.
static const ToolParamV1 kRevParams[] = {
    {"distanceFalloffStart", 60.0f},
    {"minDamageFraction", 0.02f},
    {"falloffExponent", 2.0f},
    {"limbDamageMultiplier", 0.75f},
    {"minAngleFactor", 0.45f},
    {"shootPoseTime", 0.12f},
    {"equipPoseTime", 0.18f},
};

} // namespace

ToolVisualRecipeV1 makeRevolverVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("revolver"),
        "assets/objects/weapons/mimita-revolver-v1.glb",
        gameHash("mesh.tool.revolver"));
    r.viewPosition[0] = 0.35f; r.viewPosition[1] = 0.10f; r.viewPosition[2] = -0.45f;
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.22f;
    addFirearmMuzzle(r, 1.0f, 0.9f, 0.55f, 0.16f, 1.0f, 0.85f, 0.5f, 3.0f, 6.0f);
    r.sounds.fire = "revolvershoot";
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 1u, 9.0f, 3.0f, 0.008f, 1.0f, 0.0f,
                  0.0f, 6, 1337, 1, 0.0f, 0.0f, 0.0f, 0.12f, 0.18f, kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "revolverreload";
    r.sounds.equip = "weapon/revolver/revolverequip";
    r.sounds.unequip = "weapon/revolver/revolverequip";
    setIdentity(r, "revolver", "Revolver");
    r.definition.soundHit = "player_hurt";
    r.definition.soundDryFire = "ui/click";
    r.definition.params = kRevParams;
    r.definition.paramCount =
        static_cast<std::uint32_t>(sizeof(kRevParams) / sizeof(ToolParamV1));
    return r;
}

ToolVisualRecipeV1 makeShotgunVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("shotgun"),
        "assets/objects/weapons/mimita-shotgun-v1.glb",
        gameHash("mesh.tool.shotgun"));
    r.viewPosition[0] = 0.35f; r.viewPosition[1] = 0.08f; r.viewPosition[2] = -0.5f;
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.5f;
    addFirearmMuzzle(r, 1.0f, 0.82f, 0.45f, 0.2f, 1.0f, 0.78f, 0.42f, 3.2f, 7.0f);
    r.sounds.fire = "shotgunshoot";
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 3u, 5.0f, 3.0f, 0.025f, 1.0f, 10.0f,
                  10.0f, 2, 1337, 15, 0.0f, 0.0f, 0.0f, 0.14f, 0.22f, kShotPhases,
                  (std::uint32_t)(sizeof(kShotPhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "shotgunreload";
    r.sounds.equip = "weapon/shotgun/shotgunequip";
    r.sounds.unequip = "weapon/shotgun/shotgunequip";
    setIdentity(r, "shotgun", "Shotgun");
    return r;
}

ToolVisualRecipeV1 makeRocketLauncherVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("rocket_launcher"),
        "assets/objects/weapons/mimita-rpg-v3.glb",
        gameHash("mesh.tool.rocket_launcher"));
    r.viewPosition[0] = 0.32f; r.viewPosition[1] = 0.02f; r.viewPosition[2] = -0.55f;
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.55f;
    addFirearmMuzzle(r, 1.0f, 0.7f, 0.3f, 0.3f, 1.0f, 0.6f, 0.25f, 4.0f, 9.0f);
    r.projectile.meshId = HOT_MESH_ROCKET;
    r.projectile.textureId = HOT_TEX_ROCKET;
    r.projectile.scale = 1.0f;
    r.projectile.color[0] = r.projectile.color[1] = r.projectile.color[2] = 1.0f;
    r.projectile.color[3] = 1.0f;
    r.projectile.length = 1.5f;
    r.projectile.radius = 0.18f;
    r.projectile.orientFromVelocity = 1u;
    r.sounds.fire = "rocketlauncher/rocketshoot";
    setDefinition(r, WeaponBehaviorType::RocketLauncher, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, false, 7u, 0.0f, 1.0f, 0.65f, 1.5f, 0.0f,
                  50.0f, 666, 1337, 1, 45.0f, 0.7f, 5.0f, 0.04f, 0.04f, kRocketPhases,
                  (std::uint32_t)(sizeof(kRocketPhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "rocketlauncher/rocketlauncherreload";
    setIdentity(r, "rocket_launcher", "Rocket Launcher");
    return r;
}

ToolVisualRecipeV1 makeGrenadeLauncherVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("grenade_launcher"),
        "assets/objects/weapons/mimita-nadelauncher-v1.glb",
        gameHash("mesh.tool.grenade_launcher"));
    r.viewPosition[0] = 0.32f; r.viewPosition[1] = 0.02f; r.viewPosition[2] = -0.5f;
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.4f;
    addFirearmMuzzle(r, 1.0f, 0.85f, 0.5f, 0.18f, 1.0f, 0.8f, 0.45f, 3.0f, 7.0f);
    r.projectile.meshId = HOT_MESH_GRENADE;
    r.projectile.textureId = HOT_TEX_GRENADE;
    r.projectile.scale = 1.0f;
    r.projectile.color[0] = r.projectile.color[1] = r.projectile.color[2] = 1.0f;
    r.projectile.color[3] = 1.0f;
    r.projectile.radius = 0.28f;
    r.projectile.orientFromVelocity = 1u;
    r.sounds.fire = "grenadelauncher/grenadelaunchershoot";
    setDefinition(r, WeaponBehaviorType::GrenadeLauncher, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 8u, 0.0f, 1.0f, 0.6f, 0.01f, 0.0f,
                  30.0f, 4, 1337, 1, 40.0f, 1.6f, 3.0f, 0.24f, 0.24f, kGrenadePhases,
                  (std::uint32_t)(sizeof(kGrenadePhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "grenadelauncher/grenadelauncherload";
    setIdentity(r, "grenade_launcher", "Grenade Launcher");
    return r;
}

ToolVisualRecipeV1 makeSpyknifeVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("spyknife"),
        "assets/objects/weapons/mimita-spy-knife-v2.glb",
        gameHash("mesh.tool.spyknife"));
    r.viewPosition[0] = 0.35f; r.viewPosition[1] = 0.05f; r.viewPosition[2] = -0.45f;
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.2f;
    // Melee: no muzzle flash / light.
    r.muzzle.hasLight = 0u;
    r.muzzle.scale = 0.0f;
    r.muzzle.lifetime = 0.0f;
    r.sounds.fire = "weapon/hafs/hafsswing";
    setDefinition(r, WeaponBehaviorType::SpyKnife, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 12u, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 0, 0, 1, 0.0f, 0.0f, 0.0f, 0.18f, 0.18f, kKnifePhases,
                  (std::uint32_t)(sizeof(kKnifePhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "spyknife", "Spy Knife");
    return r;
}

ToolVisualRecipeV1 makeSwordswordVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("swordsword"),
        "assets/objects/weapons/mimita-hafs-v1.glb",
        gameHash("mesh.tool.swordsword"));
    r.viewPosition[0] = 0.35f; r.viewPosition[1] = 0.10f; r.viewPosition[2] = -0.45f;
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.3f;
    r.muzzle.hasLight = 0u;
    r.muzzle.scale = 0.0f;
    r.muzzle.lifetime = 0.0f;
    setDefinition(r, WeaponBehaviorType::Swordsword, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 4u, 35.0f, 1.5f, 0.03f, 0.0f, 0.0f,
                  10.0f, 0, 0, 1, 0.0f, 0.0f, 0.0f, 0.20f, 0.20f, kSwordPhases,
                  (std::uint32_t)(sizeof(kSwordPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "swordsword", "Swordsword");
    return r;
}

const ToolVisualRecipeV1* findProjectileVisual(std::uint64_t projectileTypeId)
{
    // Network projectile ids (network/packets.h): rocket launcher = 5,
    // grenade launcher = 7. Kept as plain data so a new projectile only needs a
    // recipe, not a renderer branch.
    if (projectileTypeId == 5)
        return findToolVisual(gameHash("rocket_launcher"));
    if (projectileTypeId == 7)
        return findToolVisual(gameHash("grenade_launcher"));
    return nullptr;
}

// Hot-only test tool: does not exist as a cold builtin, so its presence in the
// cold registry proves a brand-new weapon can be added with no cold edit.
ToolVisualRecipeV1 makeHotSelftestGunVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("hot_selftest_gun"),
        "assets/objects/weapons/mimita-revolver-v1.glb",
        gameHash("mesh.tool.hot_selftest_gun"));
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.22f;
    r.sounds.fire = "revolvershoot";
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 99u, 7.0f, 2.0f, 0.1f, 1.0f,
                  0.0f, 0.0f, 4, 100, 1, 0.0f, 0.0f, 0.0f, 0.15f, 0.15f, kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "hot_selftest_gun", "Hot Selftest Gun");
    return r;
}

// The single tool registry. Enumerable so the cold weapon system can register a
// brand-new hot tool without a cold edit.
const ToolVisualRecipeV1* allToolVisuals(std::uint32_t& count)
{
    static const ToolVisualRecipeV1 recipes[] = {
        makeRevolverVisual(),
        makeShotgunVisual(),
        makeRocketLauncherVisual(),
        makeGrenadeLauncherVisual(),
        makeSpyknifeVisual(),
        makeSwordswordVisual(),
        makeHotSelftestGunVisual(),
    };
    count = static_cast<std::uint32_t>(sizeof(recipes) /
                                       sizeof(ToolVisualRecipeV1));
    return recipes;
}

const ToolVisualRecipeV1* findToolVisual(std::uint64_t toolKey)
{
    if (toolKey == 0)
        return nullptr;
    std::uint32_t count = 0;
    const ToolVisualRecipeV1* recipes = allToolVisuals(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (recipes[i].toolKey == toolKey)
            return &recipes[i];
    }
    return nullptr;
}

ServerDisagreementVisualV1 makeServerDisagreementVisual()
{
    ServerDisagreementVisualV1 r{};
    // Default = the shipping JSON values (uniform small, dark turquoise). Now
    // owned by hot C++; editing this changes the running client with no EXE
    // rebuild and no JSON dependency.
    r.pulseColor[0] = 0.0f; r.pulseColor[1] = 0.8f; r.pulseColor[2] = 0.8f;
    r.pulseColor[3] = 0.6f;
    r.pulseStartScale = 0.1f;
    r.pulseEndScale = 1.0f;
    r.pulseAlpha = 0.6f;
    r.pulseLifetime = 0.6f;
    r.beamHeight = 1.3f;
    r.beamThickness = 0.06f;
    r.beamLifetime = 0.36f;
    r.beamColor[0] = 0.0f; r.beamColor[1] = 0.8f; r.beamColor[2] = 0.8f;
    r.beamColor[3] = 0.4f;
    r.tracerThickness = 0.06f;
    r.tracerLifetime = 0.4f;
    r.tracerColor[0] = 0.0f; r.tracerColor[1] = 1.0f; r.tracerColor[2] = 1.0f;
    r.tracerColor[3] = 0.8f;
    r.textPrefix = "server disagree: ";
    r.textScale = 0.25f;
    r.textZOffset = 0.5f;
    r.textLifetimeExtra = 0.01f;
    r.particleCount = 8;
    r.particleMinSpeed = 1.0f;
    r.particleMaxSpeed = 3.0f;
    r.particleGravity = 2.0f;
    r.particleLifetime = 0.8f;
    r.particleScale = 0.54f;
    r.particleEndScale = 0.08f;
    r.particleAlpha = 0.7f;
    r.particleColor[0] = 0.0f; r.particleColor[1] = 0.8f; r.particleColor[2] = 0.8f;
    r.particleColor[3] = 0.7f;
    r.sound = "serverdisagree";
    r.soundVolumeMin = 0.5f;
    r.soundVolumeMax = 0.9f;
    r.soundRange = 150.0f;
    r.smallCorrectionMag = 0.25f;
    r.largeCorrectionMag = 1.0f;
    r.smallScale = 0.8f;
    r.largeScale = 3.0f;
    r.minEffectInterval = 60.0f / 60.0f;
    for (int i = 0; i < 10; ++i) {
        r.reasonColor[i][0] = 0.0f;
        r.reasonColor[i][1] = 0.8f;
        r.reasonColor[i][2] = 0.8f;
        r.reasonColor[i][3] = 1.0f;
    }
    r.reasonColor[1][0] = 0.0f; r.reasonColor[1][1] = 0.9f; r.reasonColor[1][2] = 0.9f;
    r.reasonColor[2][0] = 0.0f; r.reasonColor[2][1] = 0.7f; r.reasonColor[2][2] = 1.0f;
    r.reasonColor[3][0] = 0.0f; r.reasonColor[3][1] = 1.0f; r.reasonColor[3][2] = 0.8f;
    r.reasonColor[4][0] = 0.2f; r.reasonColor[4][1] = 0.8f; r.reasonColor[4][2] = 1.0f;
    r.reasonColor[5][0] = 0.3f; r.reasonColor[5][1] = 0.9f; r.reasonColor[5][2] = 0.9f;
    r.reasonColor[6][0] = 0.0f; r.reasonColor[6][1] = 0.8f; r.reasonColor[6][2] = 1.0f;
    r.reasonColor[7][0] = 0.2f; r.reasonColor[7][1] = 0.6f; r.reasonColor[7][2] = 0.9f;
    r.reasonColor[8][0] = 0.3f; r.reasonColor[8][1] = 0.7f; r.reasonColor[8][2] = 0.8f;
    r.reasonColor[9][0] = 0.1f; r.reasonColor[9][1] = 0.5f; r.reasonColor[9][2] = 0.8f;
    return r;
}

LocalDisagreementVisualV1 makeLocalDisagreementIndicatorVisual()
{
    LocalDisagreementVisualV1 r{};
    r.arrowColor[0] = 0.0f; r.arrowColor[1] = 0.6f; r.arrowColor[2] = 1.0f;
    r.arrowColor[3] = 0.9f;
    r.arrowLifetime = 1.2f;
    r.arrowThickness = 0.06f;
    r.arrowAlpha = 0.9f;
    r.textColor[0] = 0.0f; r.textColor[1] = 0.6f; r.textColor[2] = 1.0f;
    r.textColor[3] = 1.0f;
    r.textZOffset = 0.3f;
    r.textLifetime = 1.2f;
    r.textScale = 0.25f;
    r.textLabel = "CORRECTED";
    return r;
}

const float* serverDisagreementReasonColor(const ServerDisagreementVisualV1& recipe,
                                           std::uint32_t reason)
{
    if (reason < 10)
        return recipe.reasonColor[reason];
    return recipe.reasonColor[0];
}

namespace {

// tool.definition: map a hot recipe/definition to the plain-data POD the cold
// weapon system reads. No pointers/STL cross; strings are copied into fixed
// buffers. toolKey != 0 selects by key; toolKey == 0 enumerates by index. A
// missing tool returns found = 0 so the cold side keeps JSON/builtin.
bool MIMITA_GAME_CALL toolDefinitionQuery(void*, GameToolDefinitionV1* q)
{
    if (!q || q->structSize != sizeof(GameToolDefinitionV1))
        return false;
    q->found = 0;
    const ToolVisualRecipeV1* r = nullptr;
    if (q->toolKey != 0) {
        r = findToolVisual(q->toolKey);
    } else {
        std::uint32_t count = 0;
        const ToolVisualRecipeV1* recipes = allToolVisuals(count);
        const std::uint32_t index = q->enumerateIndex;
        if (index >= count)
            return true;  // end of list; found stays 0
        r = &recipes[index];
    }
    if (!r)
        return true;
    const ToolDefinitionV1& d = r->definition;
    q->found = 1;
    q->toolKey = r->toolKey;
    q->presentMask = static_cast<std::uint32_t>(d.presentMask);
    q->behaviorType = d.behaviorType;
    q->fireMode = d.fireMode;
    q->networkMode = d.networkMode;
    q->hitscan = d.hitscan;
    q->slot = d.slot;
    q->damage = d.damage;
    q->headshotMultiplier = d.headshotMultiplier;
    q->fireDelay = d.fireDelay;
    q->reloadTime = d.reloadTime;
    q->spread = d.spread;
    q->recoil = d.recoil;
    q->equipPoseTime = d.equipPoseTime;
    q->unequipPoseTime = d.unequipPoseTime;
    q->projectileSpeed = d.projectileSpeed;
    q->projectileRadius = d.projectileRadius;
    q->projectileLifetime = d.projectileLifetime;
    q->magazineSize = d.magazineSize;
    q->reserveAmmo = d.reserveAmmo;
    q->pelletCount = d.pelletCount;
    std::snprintf(q->id, sizeof(q->id), "%s", d.id ? d.id : "");
    std::snprintf(q->displayName, sizeof(q->displayName), "%s",
                  d.displayName ? d.displayName : "");
    std::snprintf(q->modelPath, sizeof(q->modelPath), "%s",
                  r->modelPath ? r->modelPath : "");
    q->socket = r->socket;
    for (int i = 0; i < 3; ++i) {
        q->attachmentPosition[i] = r->worldPosition[i];
        q->attachmentRotation[i] = 0.0f;
    }
    q->scale = r->worldScale;
    std::snprintf(q->soundShoot, sizeof(q->soundShoot), "%s",
                  r->sounds.fire ? r->sounds.fire : "");
    std::snprintf(q->soundReload, sizeof(q->soundReload), "%s",
                  r->sounds.reload ? r->sounds.reload : "");
    std::snprintf(q->soundEquip, sizeof(q->soundEquip), "%s",
                  r->sounds.equip ? r->sounds.equip : "");
    std::snprintf(q->soundUnequip, sizeof(q->soundUnequip), "%s",
                  r->sounds.unequip ? r->sounds.unequip : "");
    std::snprintf(q->soundHit, sizeof(q->soundHit), "%s",
                  d.soundHit ? d.soundHit : "");
    std::snprintf(q->soundDryFire, sizeof(q->soundDryFire), "%s",
                  d.soundDryFire ? d.soundDryFire : "");
    const std::uint32_t paramCount =
        d.paramCount < 8 ? d.paramCount : 8u;
    q->paramCount = paramCount;
    for (std::uint32_t i = 0; i < paramCount; ++i) {
        std::snprintf(q->params[i].key, sizeof(q->params[i].key), "%s",
                      d.params[i].key ? d.params[i].key : "");
        q->params[i].value = d.params[i].value;
    }
    return true;
}

const MimitaHotPackage::CapabilityRegistrar s_toolDefinitionProvider{
    {GAME_CAP_TOOL_DEFINITION, gameHash("sig.tool.definition.v1"), 0,
     reinterpret_cast<void*>(&toolDefinitionQuery), "tool.definition"}};

} // namespace

#endif
