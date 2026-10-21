// 09 01 2026, 13 25
/* purpose
* Implements the client-side replicated state owner for FFA and TDM matches.
* Converts authoritative match packets into HUD-ready values and confirmed score events.
* Keeps community match presentation independent from the DuelQueue lifecycle.
* Does not run server simulation or matchmaking.
* Does not render text or own JSON layout parsing.
* Does not accept client-authored scores or win conditions.
*/
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "procedural/procedural-world-client.h"
#include "terminal/terminal-state.h"
#include "auth/auth-system.h"
#include "killfeed/killfeed.h"
#include "config/settings-backup.h"
#include "config/ragdoll-death-config.h"
#include "config/impact-decals-config.h"
#include "config/player-visuals-config.h"
#include "config/camera-config.h"
#include "camera.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "config/player-settings.h"
#include "config/movement-config.h"
#include "combat/actor-preset-weapons.h"
#include "gui/hud/healthbar-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>

namespace MimitaNet {

namespace {
// Client-side steady clock in milliseconds. It extrapolates server tick-based
// match values and projects the countdown's authoritative wall time locally.
// This local clock is never sent to the server.
uint64_t clientSteadyNowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}
} // namespace

CommunityMatchClient& CommunityMatchClient::instance()
{
    static CommunityMatchClient state;
    return state;
}

uint8_t CommunityMatchClient::localActorState(uint32_t localPlayerId) const
{
    return actorState(localPlayerId);
}

uint8_t CommunityMatchClient::actorState(uint32_t actorId) const
{
    if (actorId == 0) return 0xFF;
    for (const auto& a : mActors)
        if (a.actorId == actorId) return a.state;
    return 0xFF;
}

uint8_t CommunityMatchClient::localTeam(uint32_t localPlayerId) const
{
    return teamForActor(localPlayerId);
}

uint8_t CommunityMatchClient::teamForActor(uint32_t actorId) const
{
    if (actorId == 0) return 0xFF;
    for (const auto& a : mActors)
        if (a.actorId == actorId) return a.team;
    return 0xFF;
}

const char* CommunityMatchClient::fighterWeaponId(int choice)
{
    static const char* ids[] = {"revolver", "shotgun", "spyknife", "rocket_launcher"};
    return choice >= 0 && choice < 4 ? ids[choice] : "";
}

const char* CommunityMatchClient::fighterWeaponName(int choice)
{
    static const char* names[] = {"Revolver", "Shotgun", "Spy Knife", "Rocket Launcher"};
    return choice >= 0 && choice < 4 ? names[choice] : "";
}

bool CommunityMatchClient::fighterWeaponChoiceVisible(uint32_t localPlayerId) const
{
    if (mMode != "juggernaut" || mFighterWeaponChoiceCommitted || localTeam(localPlayerId) != 0)
        return false;
    return mPhase == DUEL_PHASE_INTERMISSION || mPhase == DUEL_PHASE_COUNTDOWN ||
           mPhase == DUEL_PHASE_GO;
}

bool CommunityMatchClient::fighterWeaponChoiceCommitted(uint32_t localPlayerId) const
{
    return mMode == "juggernaut" && localTeam(localPlayerId) == 0 &&
           mFighterWeaponChoiceCommitted;
}

bool CommunityMatchClient::selectFighterWeapon(int choice)
{
    if (choice < 0 || choice >= 4 || mMode != "juggernaut" ||
        mFighterWeaponChoiceCommitted)
        return false;
    mFighterWeaponChoice = choice;
    mFighterWeaponChoiceCommitted = true;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Weapons, StructuredLevel::Important,
        "weapon.selection.requested", "local-fighter", "juggernaut-popup",
        mServerTick,
        nlohmann::json{{"choice", choice}, {"weapon", fighterWeaponId(choice)},
                       {"round", mRoundNumber}, {"round_version", mRoundVersion}});
    return true;
}

std::string CommunityMatchClient::teamName(int team) const
{
    const Gamemode& gm = GamemodeRegistry::instance().get(mMode);
    if (team >= 0 && team < (int)gm.teams.size())
        return gm.teams[(size_t)team].displayName;
    if (team >= 0 && team < (int)gm.teamNames.size())
        return gm.teamNames[(size_t)team];
    return {};
}

void CommunityMatchClient::reset()
{
    CamConfig::instance().clearHitFlinchOverride();
    resetActorPreset();
    if (mModeMovementPresetApplied) {
        MovementJsonConfig::instance().applyRuntimeConfig(
            mModeMovementPrevious, mModeMovementPreviousName);
        mModeMovementPresetApplied = false;
    }
    // Restore backups if overrides were applied
    if (mOverridesApplied) {
        SettingsBackup::instance().restoreBackups();
        mOverridesApplied = false;
    }

    mMode.clear();
    mPhase = DUEL_PHASE_WAITING;
    mPhaseTimer = 0.0f;
    mMatchStartTick = 0;
    mServerTick = 0;
    mServerTickAnchorMs = 0;
    mCountdownDeadlineMs = 0;
    mCountdownSecondsLeftSnapshot = 0.0f;
    mGoVisibleUntilMs = 0;
    mSawGoThisMatch = false;
    mWaveBannerVisible = false;
    mWaveNumber = 0;
    mWaveNpcTarget = 0;
    mWaveNpcSpawned = 0;
    mWaveLivesRemaining = 0;
    mWaveHighest = 0;
    mMatchOver = false;
    mTimeLimitSeconds = 0;
    mGoal = 0;
    mRedScore = 0;
    mBlueScore = 0;
    mLocalScore = 0;
    mMatchId = 0;
    mStateVersion = 0;
    mRoundVersion = 0;
    mFighterWeaponChoiceRoundVersion = 0;
    mFighterWeaponChoice = -1;
    mFighterWeaponChoiceCommitted = false;
    mRoundNumber = 0;
    mRoundWins[0] = mRoundWins[1] = 0;
    mWinnerTeam = -1;
    mRoundEndReason = 0;
    mRoundSeconds = 0.0f;
    mRoundTimerLeft = 0.0f;
    mObjective = ReplicatedObjective{};
    mZtAttemptResult = false;
    mZtFinalResult = false;
    mZtRunWon = false;
    mZtAttemptsRemaining = 0;
    mZtRunElapsedTicks = 0;
    mZtBossActive = false;
    mZtBossName.clear();
    mZtBossHp = 0;
    mZtBossMaxHp = 0;
    mZtMostDamageName.clear();
    mZtMostDamage = 0;
    mZtMostHealingName.clear();
    mZtMostHealing = 0;
    mZtTopSpeedName.clear();
    mZtTopSpeed = 0.0f;
    mZtCheckpointSerial = 0;
    mZtCheckpointName.clear();
    mZtMusicSerial = 0;
    mZtMusicTrack.clear();
    mZtMusicPlaying = false;
    mZtFog = false;
    mZtRain = false;
    mZtTorchLighting = false;
    mBombOwnerType = 0;
    mBombOwnerPlayerId = 0;
    mBombOwnerNpcIndex = 0;
    mBombTimerTicks = 0;
    mBombInactiveTicks = 0;
    mBombPos = glm::vec3(0.0f);
    mLastOutlineLocalTeam = 0xFF;
    mLastOutlineParticipantCount = 0;
    mCameraFov = 0.0f;
    mForceFirstPerson = false;
    HealthbarConfig::instance().setModeVisibilityOverride(false);
    HealthbarConfig::instance().setModePercentAboveHead(false);
    RagdollDeathConfig::instance().clearRuntimeOverride();
    ImpactDecalsConfig::instance().clearRuntimeBloodOverride();
    PlayerVisualsConfig::instance().clearPlayerOutlinesOverride();
    PlayerVisualsConfig::instance().clearTeamOutlineColorsOverride();
    if (mFirstPersonApplied) {
        THE_CAMERA.thirdPerson = mPreviousThirdPerson;
        mFirstPersonApplied = false;
    }
    mRagdollEnabled = 0;
    mBloodEnabled = 0;

    mDisasterConfigured = false;
    mDisasterActive = false;
    mDisasterId.clear();
    mDisasterName.clear();
    mDisasterDescription.clear();
    mDisasterSeed = 0;
    mDisasterStartTick = 0;
    mDisasterDurationTicks = 0;
    mDisasterWinner = 0;
    mDisasterResolveSource = 0;

    mActors.clear();
    mProcedural = ProceduralWorldNetworkState{};
    // Forget the client-side door handle so a map change cannot leave it
    // pointing at a reused physical-entity id.
    clientProceduralWorldReset();

    MatchLeaderboard::instance().clear();
    KillfeedManager::instance().clear();
}

bool CommunityMatchClient::applyActorPreset(const MatchRoleDefinition& preset)
{
    if (!mActorPresetApplied) {
        mActorPresetPreviousFov = CamConfig::instance().data().fov;
        mActorPresetPreviousPlayerFov = GetPlayerSettings().fov;
        mActorPresetPreviousThirdPerson = THE_CAMERA.thirdPerson;
        mActorPresetPreviousAvatar = GetPlayerSettings().avatarName;
        mActorPresetPreviousMovement = MovementJsonConfig::instance().config();
        mActorPresetPreviousMovementName = MovementJsonConfig::instance().activePresetName();
    }
    mActorPresetId = preset.id;
    mActorPresetApplied = true;

    if (preset.forceFov && preset.cameraFov > 0.0f) {
        CamConfig::instance().data().fov = preset.cameraFov;
        GetPlayerSettings().fov = preset.cameraFov;
    }
    if (preset.forceFirstPerson)
        THE_CAMERA.thirdPerson = false;
    if (preset.avatarForced && !preset.avatarName.empty())
        GetPlayerSettings().avatarName = preset.avatarName;
    if (!preset.movementPreset.empty()) {
        MovementConfig movement;
        if (MovementJsonConfig::instance().loadPresetInto(preset.movementPreset, movement))
            MovementJsonConfig::instance().applyRuntimeConfig(movement, preset.movementPreset);
    }

    ActorPresetWeapons::apply(preset);

    Debug::log(Debug::Category::General,
        "[ACTOR PRESET] applied id=%s fov=%.0f forcedFov=%d firstPerson=%d avatar=%s movement=%s weaponSet=%s\n",
        preset.id.c_str(), preset.cameraFov, (int)preset.forceFov,
        (int)preset.forceFirstPerson,
        preset.avatarName.empty() ? "none" : preset.avatarName.c_str(),
        preset.movementPreset.empty() ? "none" : preset.movementPreset.c_str(),
        preset.weaponSet.empty() ? "none" : preset.weaponSet.c_str());
    return true;
}

void CommunityMatchClient::refreshActorPreset()
{
    if (!mActorPresetApplied) return;
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(mActorPresetId);
    if (preset) applyActorPreset(*preset);
}

void CommunityMatchClient::resetActorPreset()
{
    if (!mActorPresetApplied)
        return;
    CamConfig::instance().data().fov = mActorPresetPreviousFov;
    GetPlayerSettings().fov = mActorPresetPreviousPlayerFov;
    THE_CAMERA.thirdPerson = mActorPresetPreviousThirdPerson;
    GetPlayerSettings().avatarName = mActorPresetPreviousAvatar;
    MovementJsonConfig::instance().applyRuntimeConfig(
        mActorPresetPreviousMovement, mActorPresetPreviousMovementName);
    ActorPresetWeapons::clear();
    Debug::log(Debug::Category::General,
        "[ACTOR PRESET] reset id=%s restoredFov=%.0f restoredThirdPerson=%d\n",
        mActorPresetId.c_str(), mActorPresetPreviousFov,
        (int)mActorPresetPreviousThirdPerson);
    mActorPresetApplied = false;
    mActorPresetId.clear();
}

void CommunityMatchClient::onState(const DuelStatePacket& packet)
{
    // Accept all community match modes (FFA, TDM, Bomb Tag, and any future mode).
    // The old character-prefix filter (matchMode[0] == 'f' || 't') is deprecated.
    // State versions order match content. The server-tick anchor is separately
    // constrained to move forward, because a newer state version may be carried
    // by a delayed packet with an older simulation tick.
    if (packet.duelId < mMatchId) return;
    if (packet.duelId == mMatchId) {
        if (packet.stateVersion < mStateVersion) return;
        if (packet.stateVersion == mStateVersion && packet.serverTick < mServerTick) return;
    }

    const uint8_t prevPhase = mPhase;
    const bool newCountdown = packet.phase == DUEL_PHASE_COUNTDOWN &&
        (prevPhase != DUEL_PHASE_COUNTDOWN || packet.duelId != mMatchId ||
         packet.roundVersion != mRoundVersion || packet.matchStartTick != mMatchStartTick);
    const uint64_t receivedAtMs = clientSteadyNowMs();

    if (packet.phase == DUEL_PHASE_COUNTDOWN) {
        mCountdownSecondsLeftSnapshot = std::max(0.0f, packet.countdownLeft);
        const uint64_t incomingDeadlineMs = receivedAtMs +
            (uint64_t)std::ceil(mCountdownSecondsLeftSnapshot * 1000.0f);
        if (newCountdown || mCountdownDeadlineMs == 0) {
            mCountdownDeadlineMs = incomingDeadlineMs;
        } else {
            // Later snapshots can shorten the estimate, but a delayed or
            // reordered packet can never make the visible countdown increase.
            mCountdownDeadlineMs = std::min(mCountdownDeadlineMs, incomingDeadlineMs);
        }
    }

    mMatchId = packet.duelId;
    mStateVersion = packet.stateVersion;
    mMode = packet.matchMode;
    if (mMode != "juggernaut" || packet.phase == DUEL_PHASE_RESULTS) {
        mFighterWeaponChoiceRoundVersion = packet.roundVersion;
        mFighterWeaponChoice = -1;
        mFighterWeaponChoiceCommitted = false;
    }
    // Server-owned procedural-world state. The client never derives room
    // completion; it stores and renders exactly what the server sent.
    mProcedural = packet.procedural;
    const Gamemode& modeConfig = GamemodeRegistry::instance().get(mMode);
    std::string replicatedPresetId = packet.actorPresetId;
    if (replicatedPresetId.empty()) replicatedPresetId = modeConfig.actorPresetId;
    const MatchRoleDefinition* actorPreset =
        MatchRoleRegistry::instance().getActorPreset(replicatedPresetId);
    if (actorPreset && mActorPresetId != actorPreset->id) {
        applyActorPreset(*actorPreset);
    } else if (!actorPreset && mActorPresetApplied && mActorPresetId == modeConfig.actorPresetId) {
        resetActorPreset();
    }
    // A mode may force one movement profile for every actor (e.g. Melee's
    // "smash"). Apply it for local prediction so the client kernel matches the
    // server-authoritative preset. An actor preset's own movement wins.
    if (modeConfig.movementPreset.empty()) {
        if (mModeMovementPresetApplied) {
            MovementJsonConfig::instance().applyRuntimeConfig(mModeMovementPrevious, mModeMovementPreviousName);
            mModeMovementPresetApplied = false;
        }
    } else if (!actorPreset || actorPreset->movementPreset != modeConfig.movementPreset) {
        MovementConfig movement;
        if (MovementJsonConfig::instance().loadPresetInto(modeConfig.movementPreset, movement)) {
            if (!mModeMovementPresetApplied) {
                mModeMovementPrevious = MovementJsonConfig::instance().config();
                mModeMovementPreviousName = MovementJsonConfig::instance().activePresetName();
            }
            MovementJsonConfig::instance().applyRuntimeConfig(movement, modeConfig.movementPreset);
            mModeMovementPresetApplied = true;
        }
    }
    HealthbarConfig::instance().setModeVisibilityOverride(modeConfig.hideHealthbars);
    // Melee-style modes show each fighter's percent above the head instead of
    // the ordinary healthbar (data-driven per mode).
    HealthbarConfig::instance().setModePercentAboveHead(modeConfig.features.percentAboveHead);
    // Gamemode outline policy: a mode may force player outlines off. A missing
    // key means "no policy" and leaves the user's config untouched.
    if (modeConfig.presentation.hasPlayerOutlines)
        PlayerVisualsConfig::instance().setPlayerOutlinesEnabled(modeConfig.presentation.playerOutlines);
    else
        PlayerVisualsConfig::instance().clearPlayerOutlinesOverride();
    if (modeConfig.presentation.hasHitFlinch)
        CamConfig::instance().setHitFlinchOverride(modeConfig.presentation.hitFlinch);
    else
        CamConfig::instance().clearHitFlinchOverride();
    if (modeConfig.presentation.hasTeamOutlineColors)
        PlayerVisualsConfig::instance().setTeamOutlineColors(
            modeConfig.presentation.friendlyOutlineColor,
            modeConfig.presentation.enemyOutlineColor);
    else
        PlayerVisualsConfig::instance().clearTeamOutlineColorsOverride();
    if (mForceFirstPerson && !mFirstPersonApplied) {
        mPreviousThirdPerson = THE_CAMERA.thirdPerson;
        THE_CAMERA.thirdPerson = false;
        mFirstPersonApplied = true;
        Debug::log(Debug::Category::General,
            "[GAMEMODE OVERRIDE] Forced first person for mode=%s\n",
            mMode.c_str());
    } else if (!mForceFirstPerson && mFirstPersonApplied) {
        THE_CAMERA.thirdPerson = mPreviousThirdPerson;
        mFirstPersonApplied = false;
    }
    mPhase = packet.phase;
    mPhaseTimer = packet.phaseTimer;
    mMatchStartTick = packet.matchStartTick;
    if (packet.serverTick >= mServerTick) {
        mServerTick = packet.serverTick;
        mServerTickAnchorMs = receivedAtMs;
    }
    mWaveBannerVisible = packet.waveBannerVisible != 0;
    mWaveNumber = packet.waveNumber;
    mWaveNpcTarget = packet.waveNpcTarget;
    mWaveNpcSpawned = packet.waveNpcSpawned;
    mWaveLivesRemaining = packet.waveLivesRemaining;
    mWaveHighest = packet.waveHighest;
    mMatchOver = packet.matchOver != 0;
    if (packet.phase == DUEL_PHASE_GO)
    {
        // phaseTimer carries the remaining GO wall duration. Project it onto
        // the client's monotonic clock so an old server tick cannot hide GO.
        const uint64_t goMs = packet.phaseTimer > 0.0f
            ? (uint64_t)std::ceil(packet.phaseTimer * 1000.0f) : 1000u;
        mGoVisibleUntilMs = receivedAtMs + goMs;
        mSawGoThisMatch = true;
    }
    else if (packet.phase == DUEL_PHASE_ACTIVE)
    {
        // First round after a join: the client is busy loading and can miss the
        // short GO phase entirely. When ACTIVE starts the match without a GO
        // packet seen, hold GO! for the configured window so it always shows.
        if (!mSawGoThisMatch &&
            (prevPhase == DUEL_PHASE_COUNTDOWN || prevPhase == DUEL_PHASE_GO ||
             countdownElapsed()))
        {
            const uint64_t goMs = packet.goSeconds > 0.0f
                ? (uint64_t)std::ceil(packet.goSeconds * 1000.0f) : 1000u;
            mGoVisibleUntilMs = receivedAtMs + goMs;
        }
    }
    else
    {
        mGoVisibleUntilMs = 0;
        mSawGoThisMatch = false;
    }
    mTimeLimitSeconds = packet.timeLimitSeconds;
    mGoal = packet.goalValue;
    mRedScore = packet.redTeamKills;
    mBlueScore = packet.blueTeamKills;
    mRoundVersion = packet.roundVersion;
    mRoundNumber = packet.roundNumber;
    mRoundWins[0] = packet.roundWins[0];
    mRoundWins[1] = packet.roundWins[1];
    mWinnerTeam = packet.winnerTeam;
    mRoundEndReason = packet.roundEndReason;
    mRoundSeconds = packet.roundSeconds;
    mRoundTimerLeft = packet.roundTimerLeft;
    mObjective.active = packet.objectiveActive != 0;
    mObjective.kind = packet.objectiveKind;
    mObjective.state = packet.objectiveState;
    mObjective.team = packet.objectiveTeam;
    mObjective.carrierId = packet.objectiveCarrierId;
    mObjective.position = glm::vec3(packet.objectiveX, packet.objectiveY, packet.objectiveZ);
    mObjective.id = packet.objectiveId;
    mObjective.site = packet.objectiveSite;
    mObjective.progress = packet.objectiveProgress;
    mObjective.timerLeft = packet.objectiveTimerLeft;
    mObjective.progressKind = packet.objectiveProgressKind;

    mZtAttemptResult = packet.ztAttemptResult != 0;
    mZtFinalResult = packet.ztFinalResult != 0;
    mZtRunWon = packet.ztRunWon != 0;
    mZtAttemptsRemaining = packet.ztAttemptsRemaining;
    mZtRunElapsedTicks = packet.ztRunElapsedTicks;
    mZtBossActive = packet.ztBossActive != 0;
    mZtBossName = packet.ztBossName;
    mZtBossHp = packet.ztBossHp;
    mZtBossMaxHp = packet.ztBossMaxHp;
    mZtMostDamageName = packet.ztMostDamageName;
    mZtMostDamage = packet.ztMostDamage;
    mZtMostHealingName = packet.ztMostHealingName;
    mZtMostHealing = packet.ztMostHealing;
    mZtTopSpeedName = packet.ztTopSpeedName;
    mZtTopSpeed = packet.ztTopSpeed;
    mZtCheckpointSerial = packet.ztCheckpointSerial;
    mZtCheckpointName = packet.ztCheckpointName;
    mZtMusicSerial = packet.ztMusicSerial;
    mZtMusicTrack = packet.ztMusicTrack;
    mZtMusicPlaying = packet.ztMusicPlaying != 0;
    mZtFog = packet.ztFog != 0;
    mZtRain = packet.ztRain != 0;
    mZtTorchLighting = packet.ztTorchLighting != 0;

    int newLocalScore = 0;
    std::vector<MatchLeaderboardEntry> leaders;
    for (int i = 0; i < 3 && packet.ffaLeaderIds[i] != 0; ++i) {
        MatchLeaderboardEntry entry;
        entry.name = packet.ffaLeaderNames[i];
        entry.score = packet.ffaLeaderScores[i];
        entry.rank = i;
        entry.isLocalPlayer = packet.ffaLeaderIds[i] == MP_CONTEXT.localPlayerId;
        if (entry.isLocalPlayer)
        {
            entry.vipAppearance = AuthSystem::instance().user().vipAppearance;
            entry.vipStyleDetail = AuthSystem::instance().user().vipStyleDetail;
            newLocalScore = entry.score;
        }
        else
        {
            auto it = MP_CONTEXT.remotePlayers.find(packet.ffaLeaderIds[i]);
            if (it != MP_CONTEXT.remotePlayers.end())
            {
                entry.vipAppearance = it->second.vipAppearance;
                entry.vipStyleDetail = it->second.vipStyleDetail;
            }
        }
        leaders.push_back(entry);
    }

    MatchLeaderboard& hud = MatchLeaderboard::instance();
    hud.setMode(mMode, mGoal);
    if (mMode != "tdm") hud.updateFFA(leaders);
    else if (mMode == "tdm") {
        bool isRed = false;
        for (uint8_t i = 0; i < packet.participantCount; ++i)
            if (packet.participantIds[i] == MP_CONTEXT.localPlayerId)
                isRed = packet.participantTeams[i] == 0;
        newLocalScore = isRed ? mRedScore : mBlueScore;
        hud.updateTDM(mRedScore, mBlueScore, isRed);
    }
    std::string winnerName;
    if (packet.winnerPlayerId != 0) {
        for (int i = 0; i < 3; ++i) {
            if (packet.ffaLeaderIds[i] == packet.winnerPlayerId) {
                winnerName = packet.ffaLeaderNames[i];
                break;
            }
        }
    }
    hud.setWinner(winnerName,
        packet.matchOver != 0 && packet.phase == DUEL_PHASE_RESULTS);
    if (newLocalScore > mLocalScore) hud.onConfirmedScoreGain();
    mLocalScore = newLocalScore;

    // ── Replicate actor identity (team / role / state) ─────────────
    // The server owns these values; the client only mirrors them for HUD and
    // diagnostics. Role is a 1-based MatchRoleRegistry index (0 = none).
    mActors.clear();
    mActors.reserve(packet.participantCount);
    for (uint8_t i = 0; i < packet.participantCount; ++i) {
        ReplicatedActorIdentity identity;
        identity.actorId = packet.participantIds[i];
        identity.team = packet.participantTeams[i];
        identity.roleIndex = packet.participantRoles[i];
        identity.state = packet.participantStates[i];
        mActors.push_back(identity);
    }

    if (mMode == "juggernaut") {
        const uint8_t resolvedLocalTeam = localTeam(MP_CONTEXT.localPlayerId);
        size_t teamZeroCount = 0;
        size_t teamOneCount = 0;
        for (const auto& actor : mActors) {
            if (actor.team == 0) ++teamZeroCount;
            else if (actor.team == 1) ++teamOneCount;
        }
        if (resolvedLocalTeam != mLastOutlineLocalTeam ||
            packet.participantCount != mLastOutlineParticipantCount) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "client.juggernaut.outline-identity", std::to_string(MP_CONTEXT.localPlayerId),
                "resolved local team for friendly/enemy outline colors", packet.serverTick,
                nlohmann::json{
                    {"local_player_id", MP_CONTEXT.localPlayerId},
                    {"local_team", resolvedLocalTeam == 0xFF ? -1 : (int)resolvedLocalTeam},
                    {"participant_count", packet.participantCount},
                    {"team_zero_count", teamZeroCount},
                    {"team_one_count", teamOneCount},
                    {"local_present", resolvedLocalTeam != 0xFF}});
            mLastOutlineLocalTeam = resolvedLocalTeam;
            mLastOutlineParticipantCount = packet.participantCount;
        }
    }

    // The mode preset supplies shared arcade presentation. Role-specific
    // camera and movement values are selected after replicated actor identity
    // arrives, so Fighters and Juggernauts do not inherit one another's FOV.
    if (mMode == "juggernaut") {
        const uint8_t team = localTeam(MP_CONTEXT.localPlayerId);
        const char* localPresetId = team == 1 ? "juggernaut_arcade" :
                                     team == 0 ? "juggernaut_fighter" : nullptr;
        if (localPresetId) {
            const MatchRoleDefinition* localPreset =
                MatchRoleRegistry::instance().getActorPreset(localPresetId);
            if (localPreset && mActorPresetId != localPreset->id) {
                // The mode packet is applied before replicated team identity,
                // so it may have temporarily applied the shared Juggernaut
                // first-person preset. Restore the user's camera choice at
                // this role boundary before applying the local role preset.
                if (mActorPresetApplied)
                    resetActorPreset();
                applyActorPreset(*localPreset);
            }
            if (localPreset) actorPreset = localPreset;
        }
    }

    // The shared mode packet carries the arcade presentation preset for every
    // client. In Juggernaut mode, the resolved local team preset owns the
    // camera policy, so Fighters can undo the Juggernaut first-person policy.
    if (mMode == "juggernaut" && actorPreset) {
        mForceFirstPerson = actorPreset->forceFirstPerson;
    } else {
        mForceFirstPerson = modeConfig.forceFirstPerson || packet.forceFirstPerson != 0 ||
            (actorPreset && actorPreset->forceFirstPerson);
    }

    // ── Apply gamemode visual overrides ────────────────────────────
    float newFov = packet.cameraFov;
    if (mMode == "juggernaut" && actorPreset && actorPreset->forceFov)
        newFov = actorPreset->cameraFov;
    const uint8_t newRagdoll = packet.ragdollEnabled;
    const uint8_t newBlood = packet.bloodEnabled;

    // Only act if overrides changed or first apply
    if (newFov != mCameraFov || newRagdoll != mRagdollEnabled || newBlood != mBloodEnabled || !mOverridesApplied)
    {
        // Save backups before first override
        if (!mOverridesApplied && (newFov > 0.0f || newRagdoll != 0 || newBlood != 0)) {
            SettingsBackup::instance().saveBackups();
            mOverridesApplied = true;
        }

        // Apply camera FOV override
        if (newFov > 0.0f) {
            auto& camCfg = CamConfig::instance();
            auto& data = const_cast<CameraConfigData&>(camCfg.data());
            if (data.fov != newFov) {
                data.fov = newFov;
                Debug::log(Debug::Category::General,
                    "[GAMEMODE OVERRIDE] Camera FOV set to %.0f\n", newFov);
            }
        }

        // Apply ragdoll override
        if (newRagdoll != 0) {
            bool desiredEnabled = (newRagdoll == 2);
            RagdollDeathConfig::instance().setRuntimeEnabled(desiredEnabled);
            Debug::log(Debug::Category::General,
                "[GAMEMODE OVERRIDE] Ragdoll death runtime value=%s\n",
                desiredEnabled ? "enabled" : "disabled");
        }

        // Apply blood override
        if (newBlood != 0) {
            bool desiredEnabled = (newBlood == 2);
            ImpactDecalsConfig::instance().setRuntimeBloodEnabled(desiredEnabled);
            Debug::log(Debug::Category::General,
                "[GAMEMODE OVERRIDE] Blood visuals runtime value=%s\n",
                desiredEnabled ? "enabled" : "disabled");
        }

        mCameraFov = newFov;
        mRagdollEnabled = newRagdoll;
        mBloodEnabled = newBlood;
    }

    // Focused countdown diagnostics: which phase/number the client believes it
    // should show, and the incoming vs last-applied authoritative tick, so
    // reordering or a stalled countdown is visible in the Network/Duel log.
    if (mMode == "ffa" || mMode == "tdm") {
        const float countdownLeft = countdownSecondsLeft();
        // Networking category is enabled in config/debuglogger.json, so this
        // is written to the network log for countdown/GO diagnosis.
        Debug::log(Debug::Category::Networking,
            "[CountdownSync] mode=%s duelId=%u stateVersion=%u phase=%u "
            "authoritativeTick=%u syncedTick=%u matchStartTick=%u "
            "countdownLeft=%.3f number=%d countdownElapsed=%d goVisible=%d\n",
            mMode.c_str(), packet.duelId, packet.stateVersion, (unsigned)mPhase,
            packet.serverTick, mServerTick, mMatchStartTick, countdownLeft,
            countdownElapsed() ? 0 : std::max(1, (int)std::ceil(countdownLeft)),
            (int)countdownElapsed(),
            (int)(packet.phase == DUEL_PHASE_GO));
    }
}

float CommunityMatchClient::countdownSecondsLeft() const
{
    if (mCountdownDeadlineMs == 0)
        return mCountdownSecondsLeftSnapshot;
    const uint64_t now = clientSteadyNowMs();
    return mCountdownDeadlineMs > now
        ? (float)(mCountdownDeadlineMs - now) / 1000.0f
        : 0.0f;
}

bool CommunityMatchClient::countdownElapsed() const
{
    return mCountdownDeadlineMs != 0 && clientSteadyNowMs() >= mCountdownDeadlineMs;
}

uint32_t CommunityMatchClient::serverTick() const
{
    // Extrapolate the last authoritative server tick at the fixed 60 Hz
    // simulation rate for tick-based match values. The countdown uses its own
    // wall-clock deadline so server stalls cannot lengthen it.
    if (mServerTickAnchorMs == 0)
        return mServerTick;
    const uint64_t now = clientSteadyNowMs();
    const uint64_t elapsed = now >= mServerTickAnchorMs ? now - mServerTickAnchorMs : 0;
    return mServerTick + (uint32_t)(elapsed * 60ull / 1000ull);
}

bool CommunityMatchClient::goVisible() const
{
    const uint64_t now = clientSteadyNowMs();
    if (mPhase == DUEL_PHASE_GO && mGoVisibleUntilMs == 0)
        return true;
    return mGoVisibleUntilMs != 0 && now < mGoVisibleUntilMs;
}

void CommunityMatchClient::onBombTagState(const BombTagStatePacket& packet)
{
    if (packet.duelId < mMatchId ||
        (packet.duelId == mMatchId && packet.stateVersion < mStateVersion)) return;

    mMatchId = packet.duelId;
    mStateVersion = packet.stateVersion;
    mPhase = packet.phase;
    mBombOwnerType = packet.bombOwnerType;
    mBombOwnerPlayerId = packet.bombOwnerPlayerId;
    mBombOwnerNpcIndex = packet.bombOwnerNpcIndex;
    mBombTimerTicks = packet.timerTicksRemaining;
    mBombInactiveTicks = packet.inactiveTicksRemaining;
    if (packet.serverTick >= mServerTick) {
        mServerTick = packet.serverTick;
        mServerTickAnchorMs = clientSteadyNowMs();
    }
    mBombPos = glm::vec3(packet.bombPosX, packet.bombPosY, packet.bombPosZ);

    // Set mode to bombtag if we receive bomb tag state
    if (mMode != "bomb_tag") {
        mMode = "bomb_tag";
        MatchLeaderboard& hud = MatchLeaderboard::instance();
        hud.setMode(mMode, 0);
    }
}

void CommunityMatchClient::onDisasterState(const DisasterStatePacket& packet)
{
    // Same stale-packet guard as the main state: the disaster packet shares the
    // match stateVersion, so an equal version is current and accepted.
    if (packet.duelId < mMatchId ||
        (packet.duelId == mMatchId && packet.stateVersion < mStateVersion)) return;

    mMatchId = packet.duelId;
    mStateVersion = packet.stateVersion;
    mDisasterConfigured = true;
    mDisasterActive = packet.active != 0;
    mDisasterId = packet.disasterId;
    mDisasterName = packet.name;
    mDisasterDescription = packet.description;
    mDisasterSeed = packet.seed;
    mDisasterStartTick = packet.startTick;
    mDisasterDurationTicks = packet.durationTicks;
    mDisasterWinner = packet.winnerActor;
    mDisasterResolveSource = packet.resolveSource;
}

}
