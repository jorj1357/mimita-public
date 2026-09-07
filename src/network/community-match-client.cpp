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

namespace MimitaNet {

CommunityMatchClient& CommunityMatchClient::instance()
{
    static CommunityMatchClient state;
    return state;
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
    if (mMode == "free_for_all") hud.updateFFA(leaders);
    else if (mMode == "team_deathmatch") {
        bool isRed = false;
        for (uint8_t i = 0; i < packet.participantCount; ++i)
            if (packet.participantIds[i] == MP_CONTEXT.localPlayerId)
                isRed = packet.participantTeams[i] == 0;
        newLocalScore = isRed ? mRedScore : mBlueScore;
        hud.updateTDM(mRedScore, mBlueScore, isRed);
    }
    if (newLocalScore > mLocalScore) hud.onConfirmedScoreGain();
    mLocalScore = newLocalScore;
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
