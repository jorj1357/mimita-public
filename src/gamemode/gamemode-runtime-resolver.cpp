#include "gamemode/gamemode-runtime-resolver.h"

#include <string>

#include "gamemode/gamemode.h"
#include "config/gameplay-config.h"
#include "config/impact-decals-config.h"
#include "config/player-visuals-config.h"
#include "config/camera-config.h"
#include "entities/aimbody-config.h"
#include "combat/actor-preset-weapons.h"
#include "gui/hud/healthbar-config.h"
#include "killfeed/killfeed.h"
#include "debug/debug-log.h"

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

    if (p.hasKillfeed)
        KillfeedManager::instance().setModeEnabledOverride(p.killfeed);
    else
        KillfeedManager::instance().clearModeEnabledOverride();

    if (p.hasHitFlinch)
        CamConfig::instance().setHitFlinchOverride(p.hitFlinch);
    else
        CamConfig::instance().clearHitFlinchOverride();

    if (p.hasBloodClientFeedback)
        ImpactDecalsConfig::instance().setRuntimeClientFeedbackEnabled(
            p.bloodClientFeedback);
    else
        ImpactDecalsConfig::instance().clearRuntimeClientFeedbackOverride();
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
    ActorPresetWeapons::clearModePresentationOverride();
    KillfeedManager::instance().clearModeEnabledOverride();
    ImpactDecalsConfig::instance().clearRuntimeClientFeedbackOverride();

    GameplayConfig::instance().clearAimModeOverride();
    AimBodyConfig& body = AimBodyConfig::instance();
    body.clearEnabledOverride();
    body.clearModeOverride();
}
