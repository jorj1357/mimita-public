// 09 01 2026, 13 25
/* purpose
* Owns client-side replicated state for the community FFA and TDM modes.
* Accepts generic match snapshots and exposes phase, score, timer, and leaderboard data.
* Feeds confirmed score changes to the community match HUD.
* Does not own Duel matchmaking or the authoritative server simulation.
* Does not decide win conditions or mutate player health.
* Does not render UI directly.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "network/packets.h"
#include "gui/hud/match-leaderboard.h"
#include "physics/movement/movement-types.h"
#include <glm/glm.hpp>

struct MatchRoleDefinition;

namespace MimitaNet {

class CommunityMatchClient
{
public:
    static CommunityMatchClient& instance();
    // Clears all replicated community-match state before leaving or replacing
    // a server session. Presentation caches are cleared through the same
    // session boundary so an old countdown cannot survive reconnect.
    void reset();
    void onState(const DuelStatePacket& packet);
    void onBombTagState(const BombTagStatePacket& packet);
    void onDisasterState(const DisasterStatePacket& packet);
    bool active() const { return !mMode.empty() && mMode != "duel" && mMode != "sandbox"; }
    bool isBombTag() const { return mMode == "bomb_tag"; }
    const std::string& mode() const { return mMode; }
    uint8_t phase() const { return mPhase; }
    float phaseTimer() const { return mPhaseTimer; }
    uint32_t matchStartTick() const { return mMatchStartTick; }
    // Last authoritative server tick seen, extrapolated forward at the fixed
    // 60 Hz simulation rate so the HUD keeps advancing between state packets.
    uint32_t serverTick() const;
    // True while the GO! overlay should be visible. It stays true for the
    // server-sent GO window even if the ACTIVE packet arrives before render.
    bool goVisible() const;
    bool waveBannerVisible() const { return mWaveBannerVisible; }
    uint32_t waveNumber() const { return mWaveNumber; }
    int waveLivesRemaining() const { return mWaveLivesRemaining; }
    uint32_t waveHighest() const { return mWaveHighest; }
    bool matchOver() const { return mMatchOver; }
    int timeLimitSeconds() const { return mTimeLimitSeconds; }
    int goal() const { return mGoal; }
    int redScore() const { return mRedScore; }
    int blueScore() const { return mBlueScore; }
    float cameraFov() const { return mCameraFov; }
    bool forceFirstPerson() const { return mForceFirstPerson; }
    // ── Round-based match fields ────────────────────────────────────
    uint32_t roundVersion() const { return mRoundVersion; }
    uint32_t roundNumber() const { return mRoundNumber; }
    int roundWins(int team) const { return (team >= 0 && team < 2) ? mRoundWins[team] : 0; }
    int winnerTeam() const { return mWinnerTeam; }
    uint8_t roundEndReason() const { return mRoundEndReason; }
    float roundSeconds() const { return mRoundSeconds; }
    float roundTimerLeft() const { return mRoundTimerLeft; }
    // Team display names in the mode's fixed order (from the gamemode JSON).
    std::string teamName(int team) const;

    // ── Generic objective item (bomb; future payload/capture) ───────
    struct ReplicatedObjective
    {
        bool active = false;
        uint8_t kind = 0;       // ObjectiveKind
        uint8_t state = 0;      // ObjectiveState
        uint8_t team = 0xFF;    // allowed carrier team
        uint32_t carrierId = 0;
        glm::vec3 position{0.0f};
        std::string id;
        std::string site;
        float progress = 0.0f;      // 0..1 plant or defuse progress
        float timerLeft = 0.0f;     // seconds to explosion when planted
        uint8_t progressKind = 0;   // 0=none, 1=plant, 2=defuse
    };
    const ReplicatedObjective& objective() const { return mObjective; }
    bool applyActorPreset(const MatchRoleDefinition& preset);
    void refreshActorPreset();
    void resetActorPreset();
    const std::string& actorPresetId() const { return mActorPresetId; }

    // ── Bomb Tag state (replicated from server) ──────────────────────
    uint8_t bombOwnerType() const { return mBombOwnerType; }
    uint32_t bombOwnerPlayerId() const { return mBombOwnerPlayerId; }
    uint32_t bombOwnerNpcIndex() const { return mBombOwnerNpcIndex; }
    uint32_t bombTimerTicks() const { return mBombTimerTicks; }
    uint32_t bombInactiveTicks() const { return mBombInactiveTicks; }
    float bombSecondsRemaining() const { return (float)mBombTimerTicks / 60.0f; }
    bool bombIsActive() const { return mBombInactiveTicks == 0; }
    glm::vec3 bombPosition() const { return mBombPos; }

    // ── Replicated match-actor identity (team / role / state) ────────
    // Mirrors the server's participantRoles/participantStates arrays. Role is a
    // 1-based MatchRoleRegistry index (0 = none); state is an ActorState.
    struct ReplicatedActorIdentity
    {
        uint32_t actorId = 0;
        uint8_t team = 0xFF;
        uint8_t roleIndex = 0;
        uint8_t state = 0;
    };
    const std::vector<ReplicatedActorIdentity>& actorIdentities() const { return mActors; }

    // Local player's replicated actor state (ActorState), or 0xFF when the
    // local actor is not present in the current match roster.
    uint8_t localActorState(uint32_t localPlayerId) const;
    // Local player's replicated team, or 0xFF when not present.
    uint8_t localTeam(uint32_t localPlayerId) const;
    // Replicated team for any actor id, or 0xFF when not present.
    uint8_t teamForActor(uint32_t actorId) const;
    // Replicated actor state (ActorState) for any actor id, or 0xFF.
    uint8_t actorState(uint32_t actorId) const;

    bool fighterWeaponChoiceVisible(uint32_t localPlayerId) const;
    bool fighterWeaponChoiceCommitted(uint32_t localPlayerId) const;
    int fighterWeaponChoice() const { return mFighterWeaponChoice; }
    bool selectFighterWeapon(int choice);
    static const char* fighterWeaponId(int choice);
    static const char* fighterWeaponName(int choice);

    // ── Disaster state (replicated mode-pack disaster) ───────────────
    // Server-authoritative identity, seed, window, and resolved winner. The
    // client only displays this; it never decides disaster outcomes.
    bool disasterConfigured() const { return mDisasterConfigured; }
    bool disasterActive() const { return mDisasterActive; }
    const std::string& disasterId() const { return mDisasterId; }
    const std::string& disasterName() const { return mDisasterName; }
    const std::string& disasterDescription() const { return mDisasterDescription; }
    uint32_t disasterSeed() const { return mDisasterSeed; }
    uint32_t disasterStartTick() const { return mDisasterStartTick; }
    uint32_t disasterDurationTicks() const { return mDisasterDurationTicks; }
    uint32_t disasterWinner() const { return mDisasterWinner; }
    uint8_t disasterResolveSource() const { return mDisasterResolveSource; }

    // ── Procedural world (Infinite Dungeon Slayer) ───────────────────
    // Server-owned room lifecycle state plus seed and generated-room counts.
    // The client only applies this; it never decides room completion.
    const ProceduralWorldNetworkState& procedural() const { return mProcedural; }

private:
    std::string mMode;
    uint8_t mPhase = DUEL_PHASE_WAITING;
    float mPhaseTimer = 0.0f;
    uint32_t mMatchStartTick = 0;
    uint32_t mServerTick = 0;
    uint64_t mServerTickAnchorMs = 0;  // client steady-clock ms when mServerTick was received
    uint32_t mGoVisibleUntilTick = 0;  // server tick at which the GO! window ends
    bool mSawGoThisMatch = false;      // true once a GO-phase packet was applied
    bool mWaveBannerVisible = false;
    uint32_t mWaveNumber = 0;
    uint32_t mWaveNpcTarget = 0;
    uint32_t mWaveNpcSpawned = 0;
    int mWaveLivesRemaining = 0;
    uint32_t mWaveHighest = 0;
    bool mMatchOver = false;
    int mTimeLimitSeconds = 0;
    int mGoal = 0;
    int mRedScore = 0;
    int mBlueScore = 0;
    int mLocalScore = 0;
    uint32_t mMatchId = 0;
    uint32_t mStateVersion = 0;
    uint32_t mRoundVersion = 0;
    uint32_t mFighterWeaponChoiceRoundVersion = 0;
    int mFighterWeaponChoice = -1;
    bool mFighterWeaponChoiceCommitted = false;
    uint32_t mRoundNumber = 0;
    int mRoundWins[2] = {0, 0};
    int mWinnerTeam = -1;
    uint8_t mRoundEndReason = 0;
    float mRoundSeconds = 0.0f;
    float mRoundTimerLeft = 0.0f;
    ReplicatedObjective mObjective;

    // ── Bomb Tag replicated state ────────────────────────────────────
    uint8_t mBombOwnerType = 0;        // 0=none, 1=player, 2=npc
    uint32_t mBombOwnerPlayerId = 0;
    uint32_t mBombOwnerNpcIndex = 0;
    uint32_t mBombTimerTicks = 0;
    uint32_t mBombInactiveTicks = 0;
    glm::vec3 mBombPos{0.0f};

    // ── Disaster replicated state ────────────────────────────────────
    bool mDisasterConfigured = false;
    bool mDisasterActive = false;
    std::string mDisasterId;
    std::string mDisasterName;
    std::string mDisasterDescription;
    uint32_t mDisasterSeed = 0;
    uint32_t mDisasterStartTick = 0;
    uint32_t mDisasterDurationTicks = 0;
    uint32_t mDisasterWinner = 0;
    uint8_t mDisasterResolveSource = 0;

    // ── Gamemode visual overrides ──────────────────────────────────
    float mCameraFov = 0.0f;         // 0 = no override
    bool mForceFirstPerson = false;
    bool mFirstPersonApplied = false;
    bool mPreviousThirdPerson = true;
    uint8_t mRagdollEnabled = 0;     // 0=no override, 1=disabled, 2=enabled
    uint8_t mBloodEnabled = 0;       // 0=no override, 1=disabled, 2=enabled
    bool mOverridesApplied = false;  // true if backups saved + overrides applied
    bool mActorPresetApplied = false;
    float mActorPresetPreviousFov = 100.0f;
    float mActorPresetPreviousPlayerFov = 100.0f;
    bool mActorPresetPreviousThirdPerson = true;
    std::string mActorPresetPreviousAvatar;
    MovementConfig mActorPresetPreviousMovement;
    std::string mActorPresetPreviousMovementName;
    std::string mActorPresetId;
    std::vector<ReplicatedActorIdentity> mActors;
    ProceduralWorldNetworkState mProcedural;
};

}
