// 07 20 2026, 20 00
/* purpose
* Declares multiplayer client context, packets, prediction state, and public network helpers.
* Owns pending request bookkeeping shared by packet send, receive, and focused tests.
* Provides small inline reconciliation helpers for projectile fire result lifecycle state.
* Does NOT implement transport polling, rendering, audio, or server authority.
* Does NOT define movement math or client transform validation.
* Does NOT own weapon data, projectile physics constants, or map loading.
*/

#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "network/projectile-terminal-dedupe.h"
#include "network/movement-validation.h"
#include "network/remote-entity-lifecycle.h"
#include "network/connection-state.h"
#include "entities/player.h"

#include <string>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "combat/weapon-swordsword.h"
#include "network/game-transport.h"

namespace MimitaNet {

// Debug toggle for remote-player interpolation state (terminal: netinterp debug).
extern bool gNetInterpDebug;

// ── Connection state machine ──────────────────────────────────────────
// Honest client-side connection health. `WeakConnection` means game packets
// are stale but the session is still usable (input still flows). `Reconnecting`
// means reconnect attempts are running inside the grace window. The terminal
// states (ReconnectFailed / HostClosed / Kicked / ServerCrashed) describe why
// the session ended. UI derives "what the player should believe" from these.
// (enum + connectionStateName live in connection-state.h)

// ── Server disagreement event (for client-side visual effects) ────────
struct DisagreementEvent
{
    uint64_t timeMs = 0;
    DisagreementReason reason = DISAGREEMENT_NONE;
    uint32_t eventId = 0;
    uint32_t relatedSerial = 0;
    uint32_t sourcePlayerId = 0;
    uint32_t targetPlayerId = 0;
    glm::vec3 position{0.0f};
    glm::vec3 correction{0.0f};
    std::string description;
    float lifetime = 3.0f;
};

struct PlayerInfo
{
    std::string name;
    uint32_t id = 0;
    int pingMs = 0;
    MimitaVip::VipAppearance vipAppearance;
    MimitaVip::VipStyleDetail vipStyleDetail;
    uint32_t vipStyleEpoch = 0;
};

struct SnapshotTransform
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    int health = 100;
    bool onGround = false;
    int equippedSlot = 0;
    uint8_t weaponState = 0;
    glm::vec3 aimDirection{1.0f, 0.0f, 0.0f};
    int pingMs = 0;
    uint32_t serverTick = 0;
    uint64_t receivedMs = 0;
    uint16_t stateFlags = 0;
    float sizeScale = 1.0f;
    uint32_t networkEntityId = 0;
    uint16_t transformEpoch = 0;
    uint16_t dashSerial = 0;
    uint16_t groundJumpSerial = 0;
    uint16_t airJumpSerial = 0;
    uint16_t downDashSerial = 0;
    uint16_t directionChangeSerial = 0;
    uint16_t equipSerial = 0;
    uint16_t freezeSerial = 0;
    uint32_t spawnGeneration = 0;
    MimitaVip::VipAppearance vipAppearance;
};

struct NetworkShotEvent
{
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint64_t receivedMs = 0;
    uint32_t eventServerTick = 0;
    uint32_t visualServerTick = 0;
    uint32_t shooterPlayerId = 0;
    uint32_t targetPlayerId = 0;
    int damage = 0;
    int targetHealth = 0;
    float power = 0.0f;
    uint16_t effectFlags = 0;
    uint16_t targetTransformEpoch = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_NONE;
    bool killed = false;
    bool damageConfirmed = false;
    glm::vec3 origin{0.0f};
    glm::vec3 hit{0.0f};
    glm::vec3 direction{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec3 knockback{0.0f};
    glm::vec3 beamEnd{0.0f}; // origin + dir * max range (tracer continue-through)
};

struct NetworkProjectile
{
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weaponType = NETWORK_WEAPON_NONE;

    // Authoritative/server state (directly from packets, NOT for rendering)
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float age = 0.0f;
    float lifetime = 0.0f;
    float radius = 0.0f;
    float gravity = 0.0f;
    float drag = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    float armingDistance = 0.0f;
    float armingTime = 0.0f;
    float minBounceSpeed = 0.0f;
    float angularDrag = 0.0f;
    float distanceTraveled = 0.0f;
    float smokeAccumulator = 0.0f;
    int bounceCount = 0;
    int maxBounceCount = 0;
    bool predicted = false;
    bool exploded = false;
    bool worldTouched = false;
    bool explodeOnPlayerImpact = true;
    bool explodeOnWorldImpact = false;
    bool explodeOnLifetime = true;

    // Interpolation state (for smooth visual rendering)
    glm::vec3 renderPosition{0.0f};
    glm::vec3 renderVelocity{0.0f};
    glm::quat renderRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 renderAngularVelocity{0.0f};

    uint32_t prevStateTick = 0;
    glm::vec3 prevStatePos{0.0f};
    glm::vec3 prevStateVel{0.0f};
    glm::quat prevStateRot{1.0f, 0.0f, 0.0f, 0.0f};

    uint32_t targetStateTick = 0;
    glm::vec3 targetStatePos{0.0f};
    glm::vec3 targetStateVel{0.0f};
    glm::quat targetStateRot{1.0f, 0.0f, 0.0f, 0.0f};

    uint32_t latestAcceptedTick = 0;
    uint64_t lastTargetReceivedMs = 0;
    bool hasTargetState = false;
};

struct EntityInterpolationState
{
    // Ordered snapshot history for time-based interpolation. Sorted by
    // serverTick; new snapshots insert in order (out-of-order allowed),
    // same-tick snapshots replace, and the newest entry mirrors `target`.
    std::deque<SnapshotTransform> buffer;

    // Missing-from-snapshot grace tracking. An entity absent from one
    // (or a few) snapshots is retained; removal requires repeated absence
    // across newer complete snapshots over a grace period. Counters reset
    // whenever the entity is seen again.
    MissingEntityTracker missingTracker;

    // Newest snapshot (== buffer.back() when buffer non-empty). Kept for the
    // legacy two-snapshot fallback path and for state/event VFX sourcing.
    SnapshotTransform previous;
    SnapshotTransform target;
    bool hasPrevious = false;
    bool hasTarget = false;
    bool renderRegistered = false;
    // Last render-time snapshot actually written to the rendered replica.
    // Kept for the respawn hard-snap and for the shot/effect timeline gate.
    bool hasRendered = false;
    SnapshotTransform lastRender;
    // Per-entity render clock in server-tick units, stored here for debug and
    // `netstats`. It mirrors `estimatedServerNow - adaptiveDelay` computed by
    // the continuous wall-clock clock in buildReceiveTimeRender (no clamping).
    double renderTick = 0.0;
    double previousRenderTick = 0.0;
    double renderTickDelta = 0.0;
    uint32_t sampleOlderTick = 0;
    uint32_t sampleNewerTick = 0;
    double sampleAlpha = 0.0;
    double previousSampleAlpha = 0.0;
    double sampleAlphaDelta = 0.0;
    bool holding = false;
    bool renderSampleJumped = false;
    bool snappedOrCorrected = false;
    double adaptiveDelaySeconds = 0.0;
    double estimatedArrivalJitterMs = 0.0;
    // Smoothed fraction of recent snapshots that arrived with a tick gap wider
    // than adaptiveSnapshotBuffer.lossGapTicks. Drives loss-based buffer growth.
    double recentLossFraction = 0.0;
    // Whether the current frame's render tick exceeded the newest buffered
    // snapshot (extrapolating). `wasExtrapolating` is the previous frame's
    // value, used to smoothly converge when data resumes (no snap-back).
    bool extrapolating = false;
    bool wasExtrapolating = false;
    uint32_t hardSnapCount = 0;
    // Frames where the linear anti-snap glide gate clamped the rendered body's
    // movement (linear_glide_max_units_per_second). 0 = never glided = no snap
    // events at all. Diagnostic via netstats.
    uint32_t glideSnapCount = 0;
    uint32_t bufferUnderrunCount = 0;
    uint32_t holdCount = 0;
    uint32_t renderJumpCount = 0;
    uint64_t lastInterpolationDebugLogMs = 0;
    glm::vec3 lastFinalRenderPosition{0.0f};
    glm::vec3 lastRawInterpolatedPosition{0.0f};
    float lastFinalRenderDelta = 0.0f;
    float lastFinalRenderVerticalDelta = 0.0f;
    float lastFinalRenderVerticalVelocity = 0.0f;
    bool hasFinalRenderPosition = false;
    int pendingPredictedDamage = 0;
    int predictedHealthCap = -1;
    uint64_t predictedHealthUpdatedMs = 0;
    uint32_t predictedHealthRollbackCount = 0;
    uint32_t predictedHealthConfirmCount = 0;
    // Always-on spring for render_filter == "spring". Reset on respawn so the
    // new life does not inherit the old corpse's filter state.
    SpringState renderSpring;
    // True once the render filter state has been seeded to a real first render
    // position (fresh entity or respawn), so spring/hybrid/ease never start
    // from the origin. Reset on respawn alongside renderSpring.
    bool renderFilterSeeded = false;
    // Low-passed feed-forward velocity for the hybrid spring (smoothed with
    // hybrid_feed_forward_smoothing so snapshot-boundary velocity slope changes
    // don't inject jitter). Reset on respawn.
    glm::vec3 renderSpringTargetVel{0.0f};
    uint64_t lastSnapshotArrivalMs = 0;
    uint32_t lastRenderedServerTick = 0;
    uint32_t staleSnapshotCount = 0;
    uint32_t duplicateSnapshotCount = 0;
    uint32_t outOfOrderSnapshotCount = 0;
    uint32_t networkEntityId = 0;
    uint16_t lastTransformEpoch = 0;
    uint32_t lastServerTick = 0;
    uint32_t lastSpawnGeneration = 0;
    uint32_t lastSnapshotTransformEpoch = 0;
    std::string displayName;
};

struct MultiplayerContext
{
    bool active = false;
    SOCKET sock = INVALID_SOCKET;
    std::unique_ptr<IGameTransport> transport;
    sockaddr_in serverAddr{};
    uint32_t localPlayerId = 0;
    uint32_t tick = 0;
    uint64_t lastHelloMs = 0;
    uint64_t lastSnapshotTick = 0;
    uint64_t lastSnapshotReceivedMs = 0;
    uint64_t connectStartMs = 0;
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t snapshotsReceived = 0;
    uint64_t snapshotsMissed = 0;

    // Remote-player interpolation clock: advances at the fixed 60 tick/s rate
    // using wall-clock frame time, so rendering is time-based, not packet-based.
    double interpolationRenderTick = 0.0;
    bool interpolationClockStarted = false;
    uint64_t interpolationClockLastUpdateMs = 0;
    uint64_t interpolationFrameNumber = 0;
    uint32_t interpolationReanchorCount = 0;
    double lastInterpolationClockStepMs = 0.0;
    double lastInterpolationReanchorMagnitudeMs = 0.0;
    std::string lastInterpolationReanchorReason;
    std::unordered_map<uint32_t, Player> remotePlayers;
    std::unordered_map<uint32_t, Player> remoteNpcs;
    std::unordered_map<uint32_t, EntityInterpolationState> remotePlayerInterpolation;
    std::unordered_map<uint32_t, EntityInterpolationState> remoteNpcInterpolation;
    std::unordered_map<uint32_t, PlayerInfo> playerRegistry;
    // Style events that arrived before the owning player's first snapshot.
    // Applied when the registry entry is created/refreshed.
    std::unordered_map<uint32_t, MimitaVip::VipStyleDetail> pendingVipStyles;
    glm::vec3 localServerPosition{0.0f};
    glm::vec3 localServerVelocity{0.0f};
    float localServerYaw = 0.0f;
    bool localServerOnGround = false;
    uint16_t localServerEpoch = 0;
    uint16_t lastAppliedEpoch = 0;
    bool hasLocalServerPosition = false;
    bool localPlayerReconciled = false;
    uint64_t lastLocalCorrectionLogMs = 0;
    glm::vec3 pendingTeleportPosition{0.0f};
    uint64_t pendingTeleportSentMs = 0;
    bool awaitingTeleportAck = false;
    bool awaitingExplodeDeath = false;
    uint64_t explodeRequestLastSendMs = 0;
    bool teleportResync = false;
    // Set when a snapshot tick gap indicates a blackout/reconnect. The local
    // player reconcile uses it to snap back to the server's authoritative
    // position even for a Medium correction, so a drifted local prediction
    // self-corrects instead of sticking until the next death.
    bool postGapResync = false;
    uint64_t postGapResyncDeadlineMs = 0;
    int localServerHealth = 100;
    int lastSeenServerHealth = 100;

    // Predicted kill-heal: killing restores the shooter to full HP. The heal is
    // applied instantly on a predicted kill, then reconciled on the server
    // confirm (confirmed -> heal sticks) or rolled back (disagreement -> restore
    // the pre-heal HP in the same tick the disagreement effect spawns).
    bool predictedKillHealPending = false;
    uint32_t predictedKillHealTargetEntityId = 0;
    bool predictedKillHealTargetIsNpc = false;
    int predictedKillHealBeforeHp = 0;

    // Victim health synced to the shot visual: the local player's HP is NOT
    // dropped from snapshot health. Server-confirmed damage events queue a
    // pending change that is applied in the same frame the corresponding shot
    // visual plays (the attacker's body renders past the shot's server tick),
    // so the health bar never drops before the bullet/attacker are visible.
    struct PendingVictimHealth
    {
        int healthAfter = 0;
        bool killed = false;
        glm::vec3 knockback{0.0f};    // applied in the same frame as the HP drop
        uint64_t applyAtMs = 0;       // max-hold wall-clock fallback
        uint32_t shooterId = 0;       // attacker entity (for the visual gate)
        uint32_t eventServerTick = 0; // shot's server tick (lastServerTick)
        uint64_t receivedMs = 0;
    };
    std::vector<PendingVictimHealth> pendingVictimHealth;
    std::string approvedLocalName;
    std::string serverAddress;
    std::string connectionStatus;
    bool connected = false;
    bool connectFailed = false;
    bool showPlayerList = false;
    bool showDebugOverlay = true;
    std::vector<NetworkShotEvent> shotEvents;
    std::vector<NetworkShotEvent> pendingShotEvents;
    struct PendingPelletBlastEvent {
        PelletBlastEventPacket packet{};
        uint64_t receivedMs = 0;
    };
    std::vector<PendingPelletBlastEvent> pendingPelletBlastEvents;
    std::unordered_map<uint32_t, NetworkProjectile> networkProjectiles;
    ProjectileTerminalDedupe projectileTerminals;
    struct IncomingChatMessage
    {
        std::string senderName;
        std::string text;
    };
    std::vector<IncomingChatMessage> incomingChatMessages;
    struct PendingChatRequest
    {
        uint32_t requestId = 0;
        uint64_t firstSentMs = 0;
        uint64_t lastSentMs = 0;
        int attempts = 0;
        std::string message;
    };
    std::unordered_map<uint32_t, PendingChatRequest> pendingChatRequests;
    std::unordered_map<uint32_t, uint32_t> lastReceivedShotSerial;
    std::unordered_set<uint64_t> processedChatMessageIds;
    uint32_t nextLocalShotSerial = 1;
    uint32_t nextLocalProjectileFireSerial = 1;
    uint32_t nextLocalMeleeAttackSerial = 1;
    uint32_t nextActionRequestId = 1;  // monotonic, shared across all action types
    uint32_t latestServerTick = 0;
    // Newest server tick the monotonic render clock was anchored to. If the
    // server ever regresses its tick (map change / server restart), the clock
    // domain is invalid and must be reset instead of pinned by monotonicity.
    uint32_t lastClockAnchorServerTick = 0;
    uint64_t lastInputSentMs = 0;
    uint64_t lastPingSentMs = 0;
    int localPingMs = 0;
    uint64_t lastHeardServerMs = 0;
    uint64_t lastDisconnectLogMs = 0;

    // ── Connection lifecycle ──────────────────────────────────────────
    ConnectionState connectionState = ConnectionState::Disconnected;
    std::string roomCode;
    std::string joinToken;
    std::string vipJoinTicket;
    bool vipJoinTicketRequested = false;
    std::string reconnectToken;
    std::string requiredMapId;
    int reconnectAttempts = 0;
    uint64_t lastReconnectAttemptMs = 0;
    uint64_t reconnectBackoffMs = 1000;

    // ── Honest connection-health state (packet-freshness driven) ──────
    // Wall-clock deadline (nowMs()) after which the 60s reconnect grace ends
    // and the client gives up. 0 = not in a grace window.
    uint64_t reconnectGraceDeadlineMs = 0;
    // nowMs() when the connection was first judged lost (entered Reconnecting).
    uint64_t disconnectStartedMs = 0;
    // nowMs() when the last packet was SENT (for last-packet-tx-age debug).
    uint64_t lastPacketSentMs = 0;
    // Last ConnectionState we surfaced to the player (notification + UI),
    // so a state change fires exactly one notification per transition.
    ConnectionState lastNotifiedConnectionState = ConnectionState::Disconnected;
    // Per-remote-player disconnect state so observers can show red effects
    // and ticking "connection lost" labels. Cleared on teardown.
    std::unordered_map<uint32_t, RemoteConnectionState> remoteConnectionStates;

    // ── Session identity (monotonically increasing, never reset) ──────
    uint32_t connectionAttemptId = 0;
    std::string sessionId; // server-session identifier for reconnect-token policy

    // ── Migration: disagreement events from server ────────────────────
    std::vector<DisagreementEvent> disagreementEvents;
    std::unordered_set<uint32_t> processedDisagreementIds;
    uint32_t reliableEventSessionId = 0;
    std::unordered_set<uint64_t> processedReliableEventIds;
    std::deque<uint64_t> processedReliableEventOrder;
    std::unordered_set<uint64_t> presentedDamageEventIds;
    std::deque<uint64_t> presentedDamageEventOrder;
    std::unordered_set<uint64_t> processedPelletBlastSerials;

    // ── Migration: server process tracking ────────────────────────────
    bool serverProcessLaunched = false;
    uint64_t serverProcessLaunchMs = 0;
    uint16_t serverPort = 1357;

    // ── Transform epoch for spawn/resync detection ────────────────────
    uint32_t transformEpoch = 0;
    uint32_t lastKnownSpawnGeneration = 0;  // from server's PlayerRespawned
    uint32_t nextMovementSequence = 1;
    uint32_t nextInputCommandSequence = 1;  // spec: increasing inputCommandSequence per sent input
    bool gameplayActive = false;  // true after SpawnActivated received (not just transport connected)

    // ── Input redundancy (badconn loss resilience) ────────────────────
    // Last 3 sent input commands (newest last). Each InputPacket re-sends the
    // previous two so a lost input packet still delivers its movement command.
    std::vector<InputCommandRedundancySlot> recentInputCommands;

    // ── Prediction-accuracy counters (netstats) ───────────────────────
    // predictedHits: local trace claimed a hit on a remote player (instant).
    // confirmedHits: server's DamageConfirmedEvent for the local attacker.
    // rejectedHits: server rejected / never confirmed a predicted hit.
    uint64_t predictedHits = 0;
    uint64_t confirmedHits = 0;
    uint64_t rejectedHits = 0;

    // ── Ghost: show authoritative server position ─────────────────────
    bool showServerGhost = false;
    bool waitingForMapLoad = false;

    // ── Remote sword state for visual reconstruction ──────────────────
    std::unordered_map<uint32_t, SwordswordState> remoteSwordStates;

    // ── ClientMapReady tracking ───────────────────────────────────────
    bool clientMapReadySent = false;

    // ── Local event serials for remote replication ────────────────────
    // Persistent monotonic counters. Never reset to 0.
    // Incremented when the corresponding gameplay event flag is set.
    uint16_t nextLocalDashSerial = 0;
    uint16_t nextLocalGroundJumpSerial = 0;
    uint16_t nextLocalAirJumpSerial = 0;
    uint16_t nextLocalDownDashSerial = 0;
    uint16_t nextLocalFreezeSerial = 0;
    uint16_t nextLocalMovementDirectionSerial = 0;
    uint16_t nextLocalEquipSerial = 0;
    // ── Respawn lifecycle ─────────────────────────────────────────────
    uint16_t nextLocalRespawnSerial = 1;   // permanently monotonic, never reset to 0
    uint16_t pendingRespawnSerial = 0;     // 0 = no pending request
    uint16_t pendingRespawnStartEpoch = 0;
    uint64_t pendingRespawnStartedMs = 0;
    uint64_t pendingRespawnLastSendLogMs = 0;

    struct PendingKnockback
    {
        glm::vec3 impulse{0.0f};
        uint32_t targetSpawnGeneration = 0;
        uint16_t targetTransformEpoch = 0;
        std::string source;
    };
    std::vector<PendingKnockback> pendingKnockbacks;
    std::string clientMapReadySentForMap;
    uint32_t clientMapReadySentForPlayerId = 0;

    // ── Snapshot lifecycle tracking for stale-state rejection ─────────
    uint32_t latestLocalSnapshotTick = 0;
    uint32_t latestAliveSnapshotTick = 0;
    // Newest complete authoritative snapshot tick whose entity membership
    // (creation/destruction/removal/player-list) was applied. A snapshot
    // with tick < latestAppliedMembershipTick must never create, destroy,
    // remove, or revive remote entities; it may only feed interpolation.
    uint32_t latestAppliedMembershipTick = 0;

    // ── Pending projectile fire requests (retransmission) ────────────
    struct PendingFireRequest {
        uint32_t fireSerial = 0;
        uint8_t weapon = NETWORK_WEAPON_NONE;
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f};
        uint64_t firstSentMs = 0;
        uint64_t lastSentMs = 0;
        int attempts = 0;
        bool acknowledged = false;
    };
    std::unordered_map<uint32_t, PendingFireRequest> pendingFireRequests;

    // ── Generic pending attack requests (retransmission) ──────────────
    struct PendingAttackRequest {
        uint32_t requestId = 0;
        uint32_t spawnGeneration = 0;
        uint16_t weaponDefNetworkId = 0;
        int16_t equippedSlot = 0;
        uint32_t clientSimulationTick = 0;
        uint16_t basedOnInputSequence = 0;
        glm::vec3 aimOrigin{0.0f};
        glm::vec3 aimDirection{0.0f};
        glm::vec3 predictedMuzzle{0.0f};
        uint32_t deterministicSeed = 0;
        uint8_t attackVariant = 0;
        uint32_t claimedTargetId = 0;
        glm::vec3 claimedHit{0.0f};
        uint8_t claimedBodyPart = 0;
        uint64_t firstSentMs = 0;
        uint64_t lastSentMs = 0;
        int attempts = 0;
        bool accepted = false;
        bool rejected = false;
    };
    std::unordered_map<uint32_t, PendingAttackRequest> pendingAttackRequests;

    // ── Pending reload requests (retransmission like attacks) ─────────
    struct PendingReloadRequest {
        uint32_t requestId = 0;
        uint32_t spawnGeneration = 0;
        uint16_t weaponDefNetworkId = 0;
        int32_t magazineAmmo = -1;
        int32_t reserveAmmo = -1;
        uint64_t firstSentMs = 0;
        uint64_t lastSentMs = 0;
        int attempts = 0;
    };
    std::unordered_map<uint32_t, PendingReloadRequest> pendingReloadRequests;

    // ── Pending reload result (queued outside mpTick for player-scoped processing) ──
    std::vector<ReloadResultPacket> pendingReloadResults;

    // ── Pending attack result (queued outside mpTick for player-scoped weapon reconciliation) ──
    std::vector<AttackResultPacket> pendingAttackResults;

    // ── Pending hit claims (client-local "server disagree" detection) ──
    // Records every hitscan shot where the client's local trace claimed a hit
    // on a remote player, so a mismatch with the server's authoritative trace
    // (rejection, different target, or a miss) can spawn a disagreement effect.
    struct PendingHitClaim {
        uint32_t requestId = 0;
        uint32_t claimedTargetId = 0;
        glm::vec3 claimedHit{0.0f};
        uint64_t sentMs = 0;
        bool confirmed = false;
        bool resolved = false;
    };
    std::unordered_map<uint32_t, PendingHitClaim> pendingHitClaims;

    // ── Predicted remote-NPC hit timestamps ─────────────────────────────
    // npcEntityId -> nowMs() of the most recent locally-predicted hit. Used by
    // mpProcessNpcDamageEventPacket to suppress the server-confirm hitmarker/
    // killfeed that the local shooter already predicted (avoiding duplicates).
    std::unordered_map<uint32_t, uint64_t> predictedNpcHitMs;


    // ── Pending authoritative spawn (queued outside mpTick for player-scoped weapon reconciliation) ──
    // Stores the most recent PlayerRespawnedPacket until weapon runtimes are reconciled
    // and SpawnAck is sent in engineTickNet where Player is in scope.
    // Cleared after processing or when the spawn handshake completes.
    std::optional<PlayerRespawnedPacket> pendingAuthoritativeSpawn;

    // ── SpawnAck retry (reliable respawn handshake) ───────────────────
    // The client re-sends its one-shot SpawnAck until the server's
    // SpawnActivated confirms receipt, so a dropped ack under packet loss can
    // never leave the server frozen in AwaitingSpawnAck (player stuck at the
    // spawn point on other clients' screens). Cleared by SpawnActivated.
    uint32_t pendingSpawnAckGeneration = 0;   // 0 = no ack in flight
    uint32_t pendingSpawnAckEpoch = 0;
    uint64_t pendingSpawnAckLastSendMs = 0;

    // ── Predicted projectile IDs (locally simulated, suppress server interpolation) ──
    std::unordered_set<uint32_t> predictedProjectileIds;
    // fireSerial -> predicted explosion position for client-side projectile
    // explosion prediction. Used to reconcile against the server's explode event.
    std::unordered_map<uint32_t, glm::vec3> predictedExplosions;

    // ── Predicted self-knockback (fireSerial -> impulse) ────────────────
    // The local owner's client applies its own knockback instantly when its
    // predicted rocket/grenade explodes. The authoritative explode event
    // supersedes it (skip the pendingKnockback add for that fireSerial) so the
    // shooter never gets knocked twice by their own blast.
    std::unordered_map<uint32_t, glm::vec3> predictedSelfKnockbacks;

    // ── Snapshot chunk reassembly buffers ─────────────────────────────
    struct SnapshotChunkBuffer {
        std::unordered_map<uint16_t, SnapshotChunkPacket> chunks;
        uint64_t lastReceiveMs = 0;
    };
    std::unordered_map<uint32_t, SnapshotChunkBuffer> snapshotChunkBuffers;

    // ── Rejected projectile fire requests (for ammo refund) ──────────
    struct FireRejection {
        uint32_t fireSerial = 0;
        uint8_t weapon = 0;
        uint8_t reason = 0;
        float cooldownRemaining = 0.0f;
    };
    std::vector<FireRejection> fireRejections;
    // Tracks processed fireSerials to prevent double-refund across frames
    std::unordered_set<uint32_t> processedRefundSerials;

    // ── Room code for HUD display ─────────────────────────────────────
    // Set on host register or client join, cleared on disconnect/leave.
    std::string currentRoomCode;
};

struct ProjectileFireResultApplyOutcome
{
    bool accepted = false;
    bool clearedGenericPending = false;
    bool clearedProjectilePending = false;
    bool queuedRejection = false;
    bool duplicateOrStaleRejection = false;
};

inline ProjectileFireResultApplyOutcome mpApplyProjectileFireResultToPending(
    MultiplayerContext& ctx,
    const ProjectileFireResultPacket& event)
{
    ProjectileFireResultApplyOutcome outcome;
    outcome.accepted = event.accepted != 0;

    auto genIt = ctx.pendingAttackRequests.find(event.fireSerial);
    if (genIt != ctx.pendingAttackRequests.end())
    {
        ctx.pendingAttackRequests.erase(genIt);
        outcome.clearedGenericPending = true;
    }

    auto fireIt = ctx.pendingFireRequests.find(event.fireSerial);
    if (outcome.accepted)
    {
        if (fireIt != ctx.pendingFireRequests.end())
        {
            ctx.pendingFireRequests.erase(fireIt);
            outcome.clearedProjectilePending = true;
        }
        return outcome;
    }

    if (fireIt == ctx.pendingFireRequests.end())
    {
        outcome.duplicateOrStaleRejection =
            ctx.processedRefundSerials.count(event.fireSerial) != 0;
        return outcome;
    }

    if (ctx.processedRefundSerials.count(event.fireSerial) == 0)
    {
        MultiplayerContext::FireRejection fr;
        fr.fireSerial = event.fireSerial;
        fr.weapon = event.weapon;
        fr.reason = event.reason;
        fr.cooldownRemaining = event.cooldownRemaining;
        ctx.fireRejections.push_back(fr);
        ctx.processedRefundSerials.insert(event.fireSerial);
        outcome.queuedRejection = true;
    }
    else
    {
        outcome.duplicateOrStaleRejection = true;
    }

    ctx.pendingFireRequests.erase(fireIt);
    outcome.clearedProjectilePending = true;
    return outcome;
}

struct MpInput
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 externalImpulse{0.0f};
    float yaw = 0.0f;
    float lookPitch = 0.0f;
    glm::vec3 camForward{1.0f, 0.0f, 0.0f};
    float wishX = 0.0f;
    float wishY = 0.0f;
    uint64_t movementSimulationTick = 0;
    bool onGround = false;
    bool stableOnGround = false;
    bool hasWorldContact = false;
    bool realWorldContactThisFrame = false;
    bool airJumpArmed = false;
    bool airJumpLocked = false;
    bool dashAvailable = true;
    bool dashMomentumProtectionActive = false;
    bool downDashAvailable = true;
    bool freezeActive = false;
    bool freezeAvailable = true;
    bool groundReturnAvailable = true;
    bool jumpHeld = false;
    bool dashPressed = false;
    bool downDashPressed = false;
    bool attackPressed = false;
    bool freezeHeld = false;
    int equippedSlot = 0;
    uint8_t weaponState = 0;
    float sizeScale = 1.0f;
    glm::vec3 godballPosition{0.0f};
    bool godballActive = false;
};

// ── Connection lifecycle — central session management ─────────────────
enum class DisconnectPolicy : uint8_t {
    Leave,               // explicit leave — clear all
    Timeout,             // unintentional — preserve reconnectToken
    NewConnection,       // new room/server — clear all
    Rejected,            // server/coordinator rejected — clear all
    AuthFailure,         // token/auth failure — clear all
    ConnectionFailure,   // transport failure — clear all
    ServerStopped        // local server stopped — clear all
};
const char* disconnectPolicyName(DisconnectPolicy policy);
void teardownPreviousSession(MultiplayerContext& ctx, DisconnectPolicy policy);
bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName);
void mpShutdown(MultiplayerContext& ctx);
void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input, const class World& world);
void mpReconcileLocalPlayer(MultiplayerContext& ctx, Player& player, float dt);
void applyAuthoritativeSpawn(MultiplayerContext& ctx, const PlayerRespawnedPacket* spawn);

// ── Migration: disagreement packet processing ─────────────────────────
void mpProcessDisagreementPacket(MultiplayerContext& ctx, const DisagreementPacket* packet);

// ── Migration: reconnect helpers ──────────────────────────────────────
void mpStartReconnect(MultiplayerContext& ctx);
void mpTickReconnect(MultiplayerContext& ctx);
// Drives the honest connection-health state machine (packet-freshness based).
// Called once per mpTick; applies state transitions, starts/stops reconnect,
// tears down on give-up, and surfaces red notifications on state changes.
void mpUpdateConnectionHealth(MultiplayerContext& ctx);
// Surface a red notification + honest connectionStatus for a state transition
// (used by the health machine and by the Welcome/JoinAccept/ReconnectAccept
// packet handlers). No-op when the state did not actually change.
void mpNotifyConnectionStateChange(MultiplayerContext& ctx,
                                   ConnectionState before, ConnectionState next);
// Current honest connection-status text derived from state + packet freshness
// (replaces the transport-level "Connected via ICE" lie in persistent UI).
std::string mpConnectionHealthText(const MultiplayerContext& ctx);
void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position, float difficulty = 1.0f);
void mpRequestTeleport(MultiplayerContext& ctx, const glm::vec3& position);
void mpRequestExplode(MultiplayerContext& ctx);
void mpSendNpcDamageRequest(MultiplayerContext& ctx, uint32_t npcEntityId, int damage,
    const glm::vec3& origin, const glm::vec3& hit, const glm::vec3& direction,
    const glm::vec3& normal, const glm::vec3& knockback, uint16_t effectFlags, uint8_t weapon);
uint32_t mpSendAttackRequest(MultiplayerContext& ctx,
    uint16_t weaponDefNetworkId,
    int16_t equippedSlot,
    const glm::vec3& aimOrigin,
    const glm::vec3& aimDirection,
    const glm::vec3& predictedMuzzle,
    uint8_t attackVariant = 0,
    uint32_t claimedTargetId = 0,
    const glm::vec3& claimedHit = glm::vec3(0.0f),
    uint8_t claimedBodyPart = 0);
void mpSendServerCommand(MultiplayerContext& ctx, const std::string& command);
uint32_t mpSendShotEvent(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    float power,
    uint16_t effectFlags,
    uint8_t weapon,
    uint8_t impactType,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& direction,
    const glm::vec3& normal,
    const glm::vec3& knockbackImpulse = glm::vec3(0.0f));
uint32_t mpSendProjectileFireRequest(
    MultiplayerContext& ctx,
    uint8_t weapon,
    const glm::vec3& origin,
    const glm::vec3& direction);
uint32_t mpPredictProjectileAttack(
    MultiplayerContext& ctx,
    uint32_t requestId,
    uint16_t weaponDefNetworkId,
    const glm::vec3& origin,
    const glm::vec3& direction);
void mpCancelPredictedProjectileAttack(MultiplayerContext& ctx,
                                       uint32_t requestId);
// ── Async ICE connect (background thread; never blocks the main thread) ──
// Status of an in-flight ICE connect job. On success, `transport` is the
// finished ICE transport and must be installed into ctx via
// mpInstallIceConnectSuccess. The worker never touches ctx directly.
struct IceConnectStatus
{
    bool active = false;      // job still running
    bool done = false;        // job finished (success or failure)
    bool success = false;
    bool cancelled = false;
    ConnectionState state = ConnectionState::Disconnected;
    std::string message;
    std::string roomCode;
    std::string serverAddress;
    std::string joinToken;
    std::unique_ptr<IGameTransport> transport;
};
bool mpIceConnectStart(MultiplayerContext& ctx, const std::string& roomCode,
                       const std::string& playerName);
IceConnectStatus mpIceConnectPoll();
bool mpIceConnectActive();
void mpIceConnectCancel();
void mpInstallIceConnectSuccess(MultiplayerContext& ctx, IceConnectStatus& status);
uint32_t mpSendMeleeHitRequest(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    uint8_t weapon,
    uint8_t attackType,
    const glm::vec3& hit,
    const glm::vec3& normal,
    const glm::vec3& knockback,
    float weaponSpeed);
void mpSendPacket(MultiplayerContext& ctx, const void* data, int bytes);

// Returns the server tick the client is currently rendering remote entities at.
// In interpolation mode (direct_render=false) this is the delayed buffer clock,
// so attack/melee requests carry the exact tick the shooter saw and the server's
// hit-rewind lands on the rendered body (see=hit). In direct-render mode the
// rendered position IS the newest snapshot, so fallbackNewestTick is returned.
uint32_t mpFireRenderTick(const MultiplayerContext& ctx, uint32_t fallbackNewestTick);
uint32_t mpFireRenderTickForTarget(const MultiplayerContext& ctx, uint32_t targetId,
                                   uint32_t fallbackNewestTick);

// Returns the render-bias delta (rendered replica position minus newest
// authoritative snapshot position) for a remote shooter, so muzzle/tracer shot
// visuals can be re-based onto the body the client actually renders. Zero when
// the shooter is local/unknown or interpolation is inactive (direct render).
glm::vec3 mpRemoteShooterRenderDelta(const MultiplayerContext& ctx, uint32_t shooterId);
glm::vec3 mpRemoteShooterMuzzle(const MultiplayerContext& ctx, uint32_t shooterId,
                                const glm::vec3& fallbackMuzzle);

// Called from mpTick (defined in multiplayer-shots.cpp)
void mpProcessShotEventPacket(MultiplayerContext& ctx, const ShotEventPacket* event);
void mpProcessNpcDamageEventPacket(MultiplayerContext& ctx, const NpcDamageEventPacket* event);
void mpProcessProjectileSpawnEventPacket(MultiplayerContext& ctx, const ProjectileSpawnEventPacket* event);
void mpProcessProjectileStateEventPacket(MultiplayerContext& ctx, const ProjectileStateEventPacket* event);
void mpProcessProjectileExplodeEventPacket(MultiplayerContext& ctx, const ProjectileExplodeEventPacket* event);
void mpProcessProjectileDespawnEventPacket(MultiplayerContext& ctx, const ProjectileDespawnEventPacket* event);
void mpProcessProjectileFireResultPacket(MultiplayerContext& ctx, const ProjectileFireResultPacket* event);
void mpProcessAttackResultPacket(MultiplayerContext& ctx, const AttackResultPacket* event);
void mpProcessMeleeHitEventPacket(MultiplayerContext& ctx, const MeleeHitEventPacket* event);
struct ConfirmedDamagePresentationSink;
void mpProcessDamageConfirmedEventPacket(MultiplayerContext& ctx,
                                         const DamageConfirmedEventPacket* event,
                                         const ConfirmedDamagePresentationSink* sink = nullptr);
// Called from mpTick: times out unresolved hit claims so a client-local
// "server disagree: HIT REJECTED" effect spawns when the server's trace missed.
void mpSweepHitClaims(MultiplayerContext& ctx);
void mpUpdateRemoteSwordStates(MultiplayerContext& ctx, float dt);
void mpSendPelletBlastRequest(MultiplayerContext& ctx, uint8_t weapon, const glm::vec3& origin, const glm::vec3& baseDirection, uint32_t spreadSeed);
void mpProcessPelletBlastEventPacket(MultiplayerContext& ctx, const PelletBlastEventPacket* event);
void mpReleaseTimelineEvents(MultiplayerContext& ctx);
void mpUpdateNetworkProjectiles(MultiplayerContext& ctx, float dt, const class World& world);
void mpRenderNetworkProjectiles(const MultiplayerContext& ctx, const Camera& camera);

// Called from mpTick (defined in multiplayer-chat.cpp)
void mpProcessChatPacket(MultiplayerContext& ctx, const ChatPacket* chat);

// Interpolation helpers (defined in multiplayer-interpolation.cpp)
bool pushInterpolationTarget(EntityInterpolationState& interpolation, const SnapshotEntity& entity, uint32_t serverTick);
void updateRenderedReplica(Player& player, EntityInterpolationState& interpolation,
                           double renderTick, float dt, bool spawnDeathEffects);
void mpUpdateRemoteEntities(MultiplayerContext& ctx, float dt);
void mpApplyPredictedDamage(MultiplayerContext& ctx, uint32_t entityId, int damage, bool npc);
void mpConfirmPredictedDamage(MultiplayerContext& ctx, uint32_t entityId, int healthAfter, bool killed, bool npc);
void mpRollbackPredictedDamage(MultiplayerContext& ctx, uint32_t entityId, bool npc, const char* reason);
void mpApplyPredictedKillHeal(MultiplayerContext& ctx, uint32_t entityId, bool npc);
void mpConfirmPredictedKillHeal(MultiplayerContext& ctx, uint32_t entityId);
void mpRollbackPredictedKillHeal(MultiplayerContext& ctx, uint32_t entityId);
bool mpAcceptReliableEventOnce(MultiplayerContext& ctx, uint32_t eventId, uint32_t eventSessionId);
void mpDrainPendingVictimHealth(MultiplayerContext& ctx, Player& player);
// True when the shooter's rendered replica has passed the shot's server tick
// (or the event-timeline max hold elapsed) — the gate shot visuals already use.
bool mpVisualTimelineReady(const MultiplayerContext& ctx, uint32_t shooterId,
                           uint32_t visualServerTick, uint64_t receivedMs);

// Debug flags for damage/hit/net diagnostics (extern, set from terminal commands)
extern bool gNetDamageDebug;
extern bool gNetHitDebug;

} // namespace MimitaNet
