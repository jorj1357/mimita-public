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
#include "terminal/terminal-state.h"
#include "auth/auth-system.h"
#include "killfeed/killfeed.h"
#include "config/settings-backup.h"
#include "config/camera-config.h"
#include "config/ragdoll-death-config.h"
#include "config/impact-decals-config.h"
#include "debug/debug-log.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace MimitaNet {

CommunityMatchClient& CommunityMatchClient::instance()
{
    static CommunityMatchClient state;
    return state;
}

void CommunityMatchClient::reset()
{
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
    mTimeLimitSeconds = 0;
    mGoal = 0;
    mRedScore = 0;
    mBlueScore = 0;
    mLocalScore = 0;
    mMatchId = 0;
    mStateVersion = 0;
    mBombOwnerType = 0;
    mBombOwnerPlayerId = 0;
    mBombOwnerNpcIndex = 0;
    mBombTimerTicks = 0;
    mBombInactiveTicks = 0;
    mBombPos = glm::vec3(0.0f);
    mCameraFov = 0.0f;
    mRagdollEnabled = 0;
    mBloodEnabled = 0;

    MatchLeaderboard::instance().clear();
    KillfeedManager::instance().clear();
}

void CommunityMatchClient::onState(const DuelStatePacket& packet)
{
    // Accept all community match modes (FFA, TDM, Bomb Tag, and any future mode).
    // The old character-prefix filter (matchMode[0] == 'f' || 't') is deprecated.
    if (packet.duelId < mMatchId ||
        (packet.duelId == mMatchId && packet.stateVersion < mStateVersion)) return;

    mMatchId = packet.duelId;
    mStateVersion = packet.stateVersion;
    mMode = packet.matchMode;
    mPhase = packet.phase;
    mPhaseTimer = packet.phaseTimer;
    mMatchStartTick = packet.matchStartTick;
    mServerTick = packet.serverTick;
    mTimeLimitSeconds = packet.timeLimitSeconds;
    mGoal = packet.goalValue;
    mRedScore = packet.redTeamKills;
    mBlueScore = packet.blueTeamKills;

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

    // ── Apply gamemode visual overrides ────────────────────────────
    const float newFov = packet.cameraFov;
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
            auto& ragdollCfg = RagdollDeathConfig::instance();
            bool desiredEnabled = (newRagdoll == 2);
            if (ragdollCfg.data().enabled != desiredEnabled) {
                // Write to ragdolldeath.json
                const std::string path = "config/ragdolldeath.json";
                std::ifstream inFile(path);
                if (inFile.is_open()) {
                    nlohmann::json j;
                    inFile >> j;
                    inFile.close();
                    j["enabled"] = desiredEnabled;
                    std::ofstream outFile(path);
                    if (outFile.is_open()) {
                        outFile << j.dump(4);
                        outFile.close();
                        Debug::log(Debug::Category::General,
                            "[GAMEMODE OVERRIDE] Ragdoll death set to %s\n",
                            desiredEnabled ? "enabled" : "disabled");
                    }
                }
            }
        }

        // Apply blood override
        if (newBlood != 0) {
            auto& decalsCfg = ImpactDecalsConfig::instance();
            bool desiredEnabled = (newBlood == 2);
            if (decalsCfg.data().blood.enabled != desiredEnabled) {
                // Write to impact_decals.json
                const std::string path = "config/impact_decals.json";
                std::ifstream inFile(path);
                if (inFile.is_open()) {
                    nlohmann::json j;
                    inFile >> j;
                    inFile.close();
                    if (j.contains("blood") && j["blood"].is_object()) {
                        j["blood"]["enabled"] = desiredEnabled;
                    }
                    std::ofstream outFile(path);
                    if (outFile.is_open()) {
                        outFile << j.dump(4);
                        outFile.close();
                        Debug::log(Debug::Category::General,
                            "[GAMEMODE OVERRIDE] Blood visuals set to %s\n",
                            desiredEnabled ? "enabled" : "disabled");
                    }
                }
            }
        }

        mCameraFov = newFov;
        mRagdollEnabled = newRagdoll;
        mBloodEnabled = newBlood;
    }
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

}
