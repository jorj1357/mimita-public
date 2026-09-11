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

#include <chrono>
#include <cmath>
#include <fstream>
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
    mServerTickAnchorMs = 0;
    mGoVisibleUntilTick = 0;
    mSawGoThisMatch = false;
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

    mActors.clear();

    MatchLeaderboard::instance().clear();
    KillfeedManager::instance().clear();
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
    mPhase = packet.phase;
    mPhaseTimer = packet.phaseTimer;
    mMatchStartTick = packet.matchStartTick;
    mServerTick = packet.serverTick;
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

}
