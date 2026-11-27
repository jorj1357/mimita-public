#include "gamemode/gamemode-runtime-resolver.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "gamemode/gamemode.h"
#include "config/gameplay-config.h"
#include "config/impact-decals-config.h"
#include "config/player-visuals-config.h"
#include "config/camera-config.h"
#include "render/post-fx.h"
#include "render/render-player.h"
#include "entities/aimbody-config.h"
#include "entities/actor-body.h"
#include "combat/actor-preset-weapons.h"
#include "gui/hud/healthbar-config.h"
#include "gui/ui-system.h"
#include "killfeed/killfeed.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

namespace {

bool parseAimMode(const std::string& value, GameplayAimMode& out)
{
    if (value == "crosshair") { out = GameplayAimMode::Crosshair; return true; }
    if (value == "world_hit") { out = GameplayAimMode::WorldHit; return true; }
    if (value == "farpoint") { out = GameplayAimMode::Farpoint; return true; }
    if (value == "camforward") { out = GameplayAimMode::CamForward; return true; }
    if (value == "physical") { out = GameplayAimMode::Physical; return true; }
    return false;
}

bool isAimBodyMode(const std::string& value)
{
    return value == "default" || value == "smooth" ||
           value == "physical" || value == "hybrid";
}

void applyHealthbarPolicy(const Gamemode& mode)
{
    HealthbarConfig& healthbars = HealthbarConfig::instance();
    healthbars.setModeVisibilityOverride(mode.hideHealthbars);
    if (mode.presentation.hasEnemyHealthbars)
        healthbars.setModeEnemyVisibilityOverride(mode.presentation.enemyHealthbars);
    else
        healthbars.clearModeEnemyVisibilityOverride();
    if (mode.presentation.hasFriendlyHealthbars)
        healthbars.setModeFriendlyVisibilityOverride(mode.presentation.friendlyHealthbars);
    else
        healthbars.clearModeFriendlyVisibilityOverride();
    healthbars.setModePercentAboveHead(mode.features.percentAboveHead);
}

void applyVisualPolicy(const Gamemode& mode)
{
    PlayerVisualsConfig& visuals = PlayerVisualsConfig::instance();
    const GamemodePresentation& p = mode.presentation;

    visuals.setPlayerOutlinesEnabled(p.hasPlayerOutlines && p.playerOutlines);
    visuals.setFriendlyOutlinesEnabled(p.hasFriendlyOutlines && p.friendlyOutlines);
    if (p.hasPlayerWireframes)
        visuals.setPlayerWireframesEnabled(p.playerWireframes);
    else
        visuals.clearPlayerWireframesOverride();
    visuals.setFriendlyWireframesEnabled(p.hasFriendlyWireframes && p.friendlyWireframes);
    if (p.hasTeamOutlineColors)
        visuals.setTeamOutlineColors(p.friendlyOutlineColor, p.enemyOutlineColor);
    else
        visuals.clearTeamOutlineColorsOverride();
    if (p.hasTeamWireframeColors)
        visuals.setTeamWireframeColors(p.friendlyWireframeColor, p.enemyWireframeColor);
    else
        visuals.clearTeamWireframeColorsOverride();
    if (p.hasEnemyOutlineAlpha)
        visuals.setEnemyOutlineAlpha(p.enemyOutlineAlpha);
    else
        visuals.clearEnemyOutlineAlphaOverride();

    auto applyTrail = [](const GamemodeTrailSettings& src, PlayerTrailSettings& dst) {
        dst.enabled = src.enabled;
        dst.minSpeed = src.minSpeed;
        dst.sampleIntervalSeconds = src.sampleIntervalSeconds;
        dst.lifetimeSeconds = src.lifetimeSeconds;
        dst.maxSamples = src.maxSamples;
        dst.alpha = src.alpha;
        dst.width = src.width;
        dst.color = src.color;
    };
    if (p.hasEnemyTrail) {
        PlayerTrailSettings trail;
        applyTrail(p.enemyTrail, trail);
        visuals.setEnemyTrailOverride(trail);
    } else {
        visuals.clearEnemyTrailOverride();
    }
    if (p.hasTeammateTrail) {
        PlayerTrailSettings trail;
        applyTrail(p.teammateTrail, trail);
        visuals.setTeammateTrailOverride(trail);
    } else {
        visuals.clearTeammateTrailOverride();
    }
}

void applyPresentationPolicy(const Gamemode& mode)
{
    const GamemodePresentation& p = mode.presentation;
    ActorPresetWeapons::setModePresentationOverride(p);

    uiSetHudPolicy(p.hud.configured && p.hud.enabled && p.hud.sceneLayer,
                   !p.hud.configured || (p.hud.enabled && p.hud.showChat),
                   !p.hud.configured || (p.hud.enabled && p.hud.showHotbar),
                   !p.hud.configured || (p.hud.enabled && p.hud.showCrosshair),
                   !p.hud.configured || (p.hud.enabled && p.hud.showStatus),
                   // Lens CA is applied by the single PostFX pass. A second
                   // HUD CA would double-fringe mode 4 and leak CA into modes
                   // 2/3 after the world pass.
                   0.0f);

    if (p.hasKillfeed)
        KillfeedManager::instance().setModeEnabledOverride(p.killfeed);
    else
        KillfeedManager::instance().clearModeEnabledOverride();

    if (p.hasHitFlinch)
        CamConfig::instance().setHitFlinchOverride(p.hitFlinch);
    else
        CamConfig::instance().clearHitFlinchOverride();

    if (p.hasProximityFade)
        CamConfig::instance().setProximityFadeOverride(p.proximityFade);
    else
        CamConfig::instance().clearProximityFadeOverride();
    setFirstPersonHideCosmetics(
        p.hasFirstPersonHide ? p.firstPersonHide : false);
    ActorBodyConfig::instance().setGoreMode(
        p.hasGoreMode ? p.goreMode : 1);

    // Bounded mode-policy evidence. The resolver runs on every state packet;
    // only a changed resolved policy writes an event.
    const std::string policyFingerprint =
        mode.id + "|" + (p.hud.configured && p.hud.enabled && p.hud.sceneLayer
            ? "scene" : "normal") + "|" +
        (p.hasProximityFade ? (p.proximityFade ? "on" : "off") : "inherit") +
        "|" + (p.hasFirstPersonHide && p.firstPersonHide ? "hide" : "show") +
        "|" + std::to_string(p.hasGoreMode ? p.goreMode : 1);
    static std::string lastPolicyFingerprint;
    if (policyFingerprint != lastPolicyFingerprint) {
        lastPolicyFingerprint = policyFingerprint;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "gamemode.presentation.policy", mode.id,
            "resolved per-mode presentation policy", 0,
            nlohmann::json{
                {"mode_id", mode.id},
                {"hud_scene_layer", p.hud.configured && p.hud.enabled &&
                    p.hud.sceneLayer},
                {"proximity_fade", p.hasProximityFade
                    ? (p.proximityFade ? "on" : "off") : "inherit"},
                {"first_person_hide_cosmetics", p.hasFirstPersonHide
                    ? p.firstPersonHide : false},
                {"gore_mode", p.hasGoreMode ? p.goreMode : 1},
                {"camconfig_ptr", reinterpret_cast<std::uintptr_t>(
                    &CamConfig::instance())}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    if (p.hasBloodClientFeedback)
        ImpactDecalsConfig::instance().setRuntimeClientFeedbackEnabled(
            p.bloodClientFeedback);
    else
        ImpactDecalsConfig::instance().clearRuntimeClientFeedbackOverride();

    if (p.bodycam.configured && p.bodycam.enabled) {
        CamConfig::ModeBodycamOverride bodycam;
        bodycam.orientationMode = p.bodycam.orientationMode;
        bodycam.eyeOffsetMeters = p.bodycam.eyeOffsetMeters;
        bodycam.positionResponseSeconds = p.bodycam.positionResponseSeconds;
        bodycam.rotationResponseSeconds = p.bodycam.rotationResponseSeconds;
        bodycam.swayScale = p.bodycam.cameraSwayScale;
        bodycam.settleDeadbandDegrees = p.bodycam.settleCameraDeadbandDegrees;
        bodycam.settlePositionMeters = p.bodycam.settleCameraPositionMeters;
        bodycam.settleHoldTicks = p.bodycam.settleCameraHoldTicks;
        bodycam.wallCollision = p.bodycam.wallCollision;
        bodycam.wallProbeRadiusMeters = p.bodycam.wallProbeRadiusMeters;
        CamConfig::instance().setBodycamOverride(bodycam);

        AimBodyConfig::ModeResponseOverride response;
        response.headResponseSeconds = p.bodycam.headResponseSeconds;
        response.torsoResponseSeconds = p.bodycam.torsoResponseSeconds;
        response.armResponseSeconds = p.bodycam.armResponseSeconds;
        response.weaponStabilitySeconds = p.bodycam.weaponStabilitySeconds;
        response.headFollowsBody = p.bodycam.headFollowsBody;
        response.settleDeadbandDegrees = p.bodycam.settleDeadbandDegrees;
        AimBodyConfig::instance().setBodycamResponseOverride(response);

        PostFX::ModeBodycamLensOverride lens;
        lens.chromaticAberration = p.bodycam.chromaticAberration;
        lens.chromaticAberrationBackgroundOnly =
            p.bodycam.chromaticAberrationMode == "mixed" ||
            p.bodycam.chromaticAberrationMode == "background_only";
        lens.lensMode =
            p.bodycam.chromaticAberrationMode == "world_only" ||
            p.bodycam.chromaticAberrationMode == "full_world" ? 2 :
            p.bodycam.chromaticAberrationMode == "normal" ? 3 :
            p.bodycam.chromaticAberrationMode == "world_and_ui" ? 4 :
            p.bodycam.chromaticAberrationMode == "bodycam_special" ||
            p.bodycam.chromaticAberrationMode == "world_ca_lens" ? 5 : 1;
        lens.vignette = p.bodycam.vignette;
        lens.distortion = p.bodycam.lensDistortion;
        lens.curve = p.bodycam.lensCurve;
        lens.zoom = p.bodycam.lensZoom;
        lens.edgeMode = p.bodycam.lensEdgeMode == "black" ? 0
            : p.bodycam.lensEdgeMode == "vignette" ? 1
            : p.bodycam.lensEdgeMode == "clamp" ? 2
            : p.bodycam.lensEdgeMode == "repeat" ? 3 : 4;
        lens.edgeRadius = p.bodycam.lensEdgeRadius;
        lens.edgeSoftness = p.bodycam.lensEdgeSoftness;
        lens.edgeDarkness = p.bodycam.lensEdgeDarkness;
        lens.peripheralBlur = p.bodycam.lensPeripheralBlur;
        lens.radialChromaticAberration = p.bodycam.radialChromaticAberration;
        PostFX::instance().setBodycamLensOverride(lens);

        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "gamemode.bodycam.applied", mode.id, "bodycam overlay resolved and applied",
            0,
            nlohmann::json{
                {"mode_id", mode.id},
                {"orientation_mode", p.bodycam.orientationMode},
                {"effective_fov", mode.cameraFov},
                {"force_first_person", mode.forceFirstPerson},
                {"position_response_seconds", p.bodycam.positionResponseSeconds},
                {"rotation_response_seconds", p.bodycam.rotationResponseSeconds},
                {"head_response_seconds", p.bodycam.headResponseSeconds},
                {"torso_response_seconds", p.bodycam.torsoResponseSeconds},
                {"arm_response_seconds", p.bodycam.armResponseSeconds},
                {"weapon_stability_seconds", p.bodycam.weaponStabilitySeconds},
                {"head_follows_body", p.bodycam.headFollowsBody},
                {"settle_deadband_degrees", p.bodycam.settleDeadbandDegrees},
                {"settle_camera_deadband_degrees", p.bodycam.settleCameraDeadbandDegrees},
                {"settle_camera_position_meters", p.bodycam.settleCameraPositionMeters},
                {"settle_camera_hold_ticks", p.bodycam.settleCameraHoldTicks},
                {"sway_scale", p.bodycam.cameraSwayScale},
                {"wall_collision", p.bodycam.wallCollision},
                {"wall_probe_radius_m", p.bodycam.wallProbeRadiusMeters},
                {"lens", {
                    {"chromatic_aberration", p.bodycam.chromaticAberration},
                    {"chromatic_aberration_mode", p.bodycam.chromaticAberrationMode},
                    {"vignette", p.bodycam.vignette},
                    {"distortion", p.bodycam.lensDistortion},
                    {"curve", p.bodycam.lensCurve},
                    {"zoom", p.bodycam.lensZoom},
                    {"edge_mode", p.bodycam.lensEdgeMode},
                    {"radial_chromatic_aberration", p.bodycam.radialChromaticAberration}}}},
            __FILE__, __LINE__, __FUNCTION__);

        // The 2026-11-26 lens ranges are no longer validation gates. Record
        // any value outside that band so an extreme setting is traceable; do
        // not clamp it here.
        {
            nlohmann::json extremes = nlohmann::json::object();
            auto note = [&](const char* key, float value, float lo, float hi) {
                if (!std::isfinite(value) || value < lo || value > hi)
                    extremes[key] = value;
            };
            note("chromatic_aberration", lens.chromaticAberration, 0.0f, 0.8f);
            note("vignette", lens.vignette, 0.0f, 0.8f);
            note("distortion", lens.distortion, 0.0f, 100.0f);
            note("curve", lens.curve, 0.05f, 8.0f);
            note("zoom", lens.zoom, 0.1f, 5.0f);
            note("edge_radius", lens.edgeRadius, 0.05f, 1.2f);
            note("edge_softness", lens.edgeSoftness, 0.0f, 1.0f);
            note("edge_darkness", lens.edgeDarkness, 0.0f, 1.0f);
            note("peripheral_blur", lens.peripheralBlur, 0.0f, 4.0f);
            if (!extremes.empty()) {
                static std::string lastExtremeKey;
                const std::string key = extremes.dump();
                if (key != lastExtremeKey) {
                    lastExtremeKey = key;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::General, StructuredLevel::Important,
                        "config.value.extreme", mode.id,
                        "authored lens value outside the 2026-11-26 range was applied",
                        0,
                        nlohmann::json{
                            {"path", "presentation.bodycam.lens"},
                            {"baseline", "2026-11-26"},
                            {"values", extremes},
                            {"count", extremes.size()}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
            }
        }
    } else {
        CamConfig::instance().clearBodycamOverride();
        AimBodyConfig::instance().clearBodycamResponseOverride();
        PostFX::instance().clearBodycamLensOverride();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "gamemode.bodycam.cleared", mode.id, "bodycam overlay cleared; shared defaults restored",
            0, nlohmann::json{{"mode_id", mode.id}},
            __FILE__, __LINE__, __FUNCTION__);
    }
}

void applyAimPolicy(const Gamemode& mode)
{
    GameplayConfig& gameplay = GameplayConfig::instance();
    if (mode.aim.hasMode) {
        GameplayAimMode aimMode = GameplayAimMode::Crosshair;
        if (parseAimMode(mode.aim.mode, aimMode))
            gameplay.setAimModeOverride(aimMode);
        else
            gameplay.clearAimModeOverride();
    } else {
        gameplay.clearAimModeOverride();
    }

    AimBodyConfig& body = AimBodyConfig::instance();
    if (mode.aim.hasBodyEnabled)
        body.setEnabledOverride(mode.aim.bodyEnabled);
    else
        body.clearEnabledOverride();

    if (mode.aim.hasBodyMode && isAimBodyMode(mode.aim.bodyMode))
        body.setModeOverride(mode.aim.bodyMode);
    else
        body.clearModeOverride();
}

} // namespace

void GamemodeRuntimeResolver::applyClient(const Gamemode& mode)
{
    applyHealthbarPolicy(mode);
    applyVisualPolicy(mode);
    applyPresentationPolicy(mode);
    applyAimPolicy(mode);
    Debug::log(Debug::Category::General,
        "[GAMEMODE RESOLVER] applied mode=%s aim=%s body=%s\n",
        mode.id.c_str(),
        mode.aim.hasMode ? mode.aim.mode.c_str() : "inherit",
        mode.aim.hasBodyMode ? mode.aim.bodyMode.c_str() : "inherit");
}

void GamemodeRuntimeResolver::clearClient()
{
    HealthbarConfig& healthbars = HealthbarConfig::instance();
    healthbars.setModeVisibilityOverride(false);
    healthbars.clearModeEnemyVisibilityOverride();
    healthbars.clearModeFriendlyVisibilityOverride();
    healthbars.setModePercentAboveHead(false);

    PlayerVisualsConfig& visuals = PlayerVisualsConfig::instance();
    visuals.clearPlayerOutlinesOverride();
    visuals.clearTeamOutlineColorsOverride();
    visuals.clearPlayerWireframesOverride();
    visuals.clearTeamWireframeColorsOverride();
    visuals.clearEnemyOutlineAlphaOverride();
    visuals.clearEnemyTrailOverride();
    visuals.clearTeammateTrailOverride();

    CamConfig::instance().clearHitFlinchOverride();
    CamConfig::instance().clearProximityFadeOverride();
    CamConfig::instance().clearBodycamOverride();
    setFirstPersonHideCosmetics(false);
    ActorBodyConfig::instance().setGoreMode(1);
    AimBodyConfig::instance().clearBodycamResponseOverride();
    PostFX::instance().clearBodycamLensOverride();
    uiSetHudPolicy(false, true, true, true, true, 0.12f);
    ActorPresetWeapons::clearModePresentationOverride();
    KillfeedManager::instance().clearModeEnabledOverride();
    ImpactDecalsConfig::instance().clearRuntimeClientFeedbackOverride();

    GameplayConfig::instance().clearAimModeOverride();
    AimBodyConfig& body = AimBodyConfig::instance();
    body.clearEnabledOverride();
    body.clearModeOverride();
}


