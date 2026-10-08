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
// Client-side steady clock in milliseconds, used only to extrapolate the
// server tick between reliable state packets so the countdown and match clock
// keep moving smoothly. It is never sent to the server.
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
    mGoVisibleUntilTick = 0;
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
    // Same-session packets are ordered by (stateVersion, serverTick). Within a
    // phase the server keeps stateVersion constant while serverTick advances, so
    // rejecting every equal-version packet froze the HUD on the first countdown
    // number. Only drop packets that are genuinely older than what we applied.
    if (packet.duelId < mMatchId) return;
    if (packet.duelId == mMatchId) {
        if (packet.stateVersion < mStateVersion) return;
        if (packet.stateVersion == mStateVersion && packet.serverTick < mServerTick) return;
    }

    const uint8_t prevPhase = mPhase;

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
    mForceFirstPerson = modeConfig.forceFirstPerson || packet.forceFirstPerson != 0 ||
        (actorPreset && actorPreset->forceFirstPerson);
    HealthbarConfig::instance().setModeVisibilityOverride(modeConfig.hideHealthbars);
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
    mServerTick = packet.serverTick;
    mWaveBannerVisible = packet.waveBannerVisible != 0;
    mWaveNumber = packet.waveNumber;
    mWaveNpcTarget = packet.waveNpcTarget;
    mWaveNpcSpawned = packet.waveNpcSpawned;
    mWaveLivesRemaining = packet.waveLivesRemaining;
    mWaveHighest = packet.waveHighest;
    mMatchOver = packet.matchOver != 0;
    mServerTickAnchorMs = clientSteadyNowMs();
    if (packet.phase == DUEL_PHASE_GO)
    {
        // phaseTimer carries the remaining GO seconds. Hold the GO! overlay
        // until that server tick even if ACTIVE arrives first.
        const uint32_t goTicks = packet.phaseTimer > 0.0f
            ? (uint32_t)(packet.phaseTimer * 60.0f) : 60u;
        mGoVisibleUntilTick = packet.serverTick + goTicks;
        mSawGoThisMatch = true;
    }
    else if (packet.phase == DUEL_PHASE_ACTIVE)
    {
        // First round after a join: the client is busy loading and can miss the
        // short GO phase entirely. When ACTIVE starts the match without a GO
        // packet seen, hold GO! for the configured window so it always shows.
        if (!mSawGoThisMatch &&
            (prevPhase == DUEL_PHASE_COUNTDOWN || prevPhase == DUEL_PHASE_GO))
        {
            const uint32_t goTicks = packet.goSeconds > 0.0f
                ? (uint32_t)(packet.goSeconds * 60.0f) : 60u;
            mGoVisibleUntilTick = packet.serverTick + goTicks;
        }
    }
    else
    {
        mGoVisibleUntilTick = 0;
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
    if (mMode == "ffa") hud.updateFFA(leaders);
    else if (mMode == "tdm") {
        bool isRed = false;
        for (uint8_t i = 0; i < packet.participantCount; ++i)
            if (packet.participantIds[i] == MP_CONTEXT.localPlayerId)
                isRed = packet.participantTeams[i] == 0;
        newLocalScore = isRed ? mRedScore : mBlueScore;
        hud.updateTDM(mRedScore, mBlueScore, isRed);
    }
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
            if (localPreset && mActorPresetId != localPreset->id)
                applyActorPreset(*localPreset);
            if (localPreset) actorPreset = localPreset;
        }
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
        const uint32_t syncedTicksLeft = mMatchStartTick > mServerTick
            ? mMatchStartTick - mServerTick : 0;
        // Networking category is enabled in config/debuglogger.json, so this
        // is written to the network log for countdown/GO diagnosis.
        Debug::log(Debug::Category::Networking,
            "[CountdownSync] mode=%s duelId=%u stateVersion=%u phase=%u "
            "authoritativeTick=%u syncedTick=%u matchStartTick=%u "
            "ticksLeft=%u number=%d goVisible=%d\n",
            mMode.c_str(), packet.duelId, packet.stateVersion, (unsigned)mPhase,
            packet.serverTick, mServerTick, mMatchStartTick, syncedTicksLeft,
            (int)std::ceil((float)syncedTicksLeft / 60.0f),
            (int)(packet.phase == DUEL_PHASE_GO));
    }
}

uint32_t CommunityMatchClient::serverTick() const
{
    // Extrapolate the last authoritative server tick at the fixed 60 Hz
    // simulation rate so the countdown and match clock keep advancing even if a
    // state packet is delayed or retransmitted. onState() re-anchors this with
    // each accepted packet, correcting any drift.
    if (mServerTickAnchorMs == 0)
        return mServerTick;
    const uint64_t now = clientSteadyNowMs();
    const uint64_t elapsed = now >= mServerTickAnchorMs ? now - mServerTickAnchorMs : 0;
    return mServerTick + (uint32_t)(elapsed * 60ull / 1000ull);
}

bool CommunityMatchClient::goVisible() const
{
    if (mPhase == DUEL_PHASE_GO)
        return true;
    return mGoVisibleUntilTick != 0 && serverTick() < mGoVisibleUntilTick;
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
    mServerTick = packet.serverTick;
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
