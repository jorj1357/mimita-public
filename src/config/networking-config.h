// 07 31 2026, 20 15
/* purpose
* Declares the validated, hot-reloadable networking tuning configuration.
* Owns the typed NetworkingConfigData structs read by interpolation, snapshots, and reconciliation.
* Provides a singleton loader with atomic full-file replacement and change logging.
* Does NOT parse packets, send data, or own server authority.
* Does NOT render debug overlays or run the fixed-step simulation tick.
*/
#pragma once

#include <filesystem>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

struct RemotePlayerInterpolationConfig
{
    bool enabled = true;
    // Direct mode: render the newest authoritative snapshot for position,
    // velocity, and animation state together (zero artificial delay, humans
    // and NPCs share the same path). When false the time-based interpolation
    // buffer below is used.
    bool directRender = true;
    // Server-side gap smoothing between accepted movement reports. Defaults
    // off (raw broadcast) so remote bodies are not held back by a smoothing
    // segment; kept for when jitter needs filling later.
    bool serverSmoothing = false;
    // Fixed server-side lerp window (in server ticks) used to converge the
    // broadcast position to each newly accepted movement report. A small fixed
    // window keeps the broadcast stream smooth and bounded even when reports
    // arrive in bursts under jitter/loss/reorder (a report-gap-derived window
    // is what made remote bodies lag hundreds of ms behind their positions).
    // Only used as the fallback when serverBroadcastDelaySeconds == 0.
    uint32_t serverSmoothingDurationTicks = 2;
    // Server receive-time broadcast buffer. The server renders each remote
    // player's broadcast position at `now - serverBroadcastDelaySeconds` by
    // lerping between accepted reports keyed by acceptance time, so bursty
    // reports from a bad-connection client become a smooth stream for everyone.
    // 0 disables this and falls back to serverSmoothingDurationTicks.
    double serverBroadcastDelaySeconds = 0.050;
    // How long the server keeps a remote body moving along its last accepted
    // velocity once the receive-time buffer runs dry (no fresh reports), before
    // holding. 0 = hold immediately.
    double serverBroadcastExtrapolationSeconds = 0.100;
    // Extra ticks subtracted from the hit-rewind tick beyond the client-stamped
    // view tick. Positive = rewind further back into the past. Compensates for
    // residual see=hit drift under jitter and server broadcast smoothing.
    // Stored in seconds; exposed in ms.
    double rewindCompensationSeconds = 0.0;
    // How close a claimed hit must land to the rewound body (units) for the
    // pellet/shot validation path to accept it. Replaces the hardcoded 2.5f.
    float rewindHitTolerance = 2.5f;
    // Max units/sec the server-smoothed broadcast position may move per tick.
    // 0 = unlimited. Clamps residual catch-up spikes from bursty reports so
    // even a bad-connection client's bursts render as steady motion.
    double serverBroadcastMaxSpeed = 0.0;
    // server_sim broadcast smoothing: how many ticks the broadcast position
    // eases toward the authoritative sim position (0 = none). Removes the tiny
    // per-tick resting jitter from the server collision solver so other
    // clients' linear interpolation runs on a clean stream.
    uint32_t serverSimSmoothTicks = 2;
    double interpolationDelaySeconds = 0.033;
    std::size_t maximumBufferedSnapshots = 64;
    std::size_t minimumSnapshotsBeforeRendering = 2;
    bool allowExtrapolation = true;
    double maximumExtrapolationSeconds = 0.100;
    float teleportDistance = 1000.0f;
    // Time gap (in server ticks) between two buffered snapshots beyond which
    // the hole is treated as a discontinuity (e.g. a long blackout), snapping
    // the body to the newest snapshot instead of bridging a stale straight
    // line across seconds of missing motion. Short loss gaps (a few ticks)
    // still interpolate smoothly across the hole.
    uint32_t teleportGapTicks = 30;
    // What position the server broadcasts for each player:
    //   "server_sim"  — the server's own 60Hz input-driven simulation (p.pos).
    //                   Always smooth, immune to that player's network quality.
    //   "client_report" — the validated client-reported position (legacy).
    std::string broadcastSource = "server_sim";
    // When the interpolation buffer runs dry, keep extrapolating along the last
    // velocity past maximumExtrapolationSeconds (with a slow glide decay) instead
    // of stopping the body at the cap. Prevents "freeze for N ticks then snap".
    bool extrapolationKeepMoving = true;
    // When a hole wider than teleportGapTicks is crossed (e.g. a long blackout),
    // snap to the newest snapshot (true) or feed it to the motion filter so the
    // body converges smoothly instead of teleporting (false).
    bool teleportGapSnap = true;
    bool snapOnSpawn = true;
    bool snapOnRespawn = true;
    bool snapOnMapChange = true;
    // Render interpolation modes for remote actors. "linear" position lerps
    // straight lines; "slerp" rotation takes the shortest angular path.
    std::string positionMode = "linear";
    std::string rotationMode = "slerp";
};

struct LocalReconciliationConfig
{
    bool enabled = true;
    std::string correctionMode = "hard";
    double correctionDurationSeconds = 0.100;
    float hardSnapDistance = 100.0f;
};

struct SnapshotBufferConfig
{
    bool useServerTick = true;
    bool discardDuplicateSnapshots = true;
    bool allowOutOfOrderInsertion = true;
    double maximumSnapshotAgeSeconds = 2.0;
    double chunkReassemblyTimeoutSeconds = 1.0;
};

// Post-interpolation motion filter applied to the rendered remote body.
//   "direct"  — set the position directly each frame (zero smoothing; can snap).
//   "bounded" — cap the per-frame movement toward the interpolated target so a
//               discontinuity (loss hole, extrapolation resume, blackout)
//               converges smoothly over a few frames instead of snapping.
//               Normal smooth motion passes through untouched.
//   "spring"  — always-on critically-damped spring on the rendered position.
//               Literally cannot snap; adds a few ms of follow-lag on fast turns.
//   "hybrid"  — velocity-feed-forward spring. Feeds the interpolated velocity
//               forward so fast/up-down motion tracks with ~zero lag (no
//               follow-lag) while discontinuities still spring-converge with
//               zero snap. This is the best of spring + bounded.
//   "linear"  — pure CS-style delayed packet interpolation. Renders
//               linear_delay_ticks packets behind the newest and ALWAYS slides
//               linearly between the two bracketing snapshots; never renders
//               the newest directly. The delay is a loss buffer: up to
//               linear_delay_ticks consecutive lost packets are hidden. When
//               loss exceeds the buffer, linear_hold_on_dry decides between a
//               CS-style hold (seamless resume) or extrapolation (never stop).
struct RemoteMotionSmoothingConfig
{
    std::string renderFilter = "hybrid";
    // Max units/sec the rendered body may move toward the target in bounded mode.
    double correctionMaxStepUnitsPerSecond = 100.0;
    // Ignore sub-pixel deltas (units) so tiny noise never triggers the filter.
    double correctionMinDeltaUnits = 0.05;
    double springStiffness = 120.0;
    double springDamping = 20.0;
    // spring mode (all `spring_*` knobs): 0 = classic spring driven by
    // spring_stiffness/damping (unchanged legacy behavior). Set > 0 to switch
    // to frequency control: k=(2π·freq)², c=2ω·damping_ratio. High frequency =
    // tight, linear-looking tracking.
    double springFrequencyHz = 0.0;
    double springDampingRatio = 1.0;
    // 0..1 blend between classic spring (0) and full velocity feed-forward (1,
    // crisp linear-look with ~zero lag). Set 1 with a high frequency and the
    // deadzone to make spring render exactly like linear interpolation.
    double springFeedForward = 0.0;
    // Low-pass (0..1) on the velocity fed into the spring (anti-jitter).
    double springFeedForwardSmoothing = 0.4;
    // Scales spring stiffness/damping on the Z axis only (jumps/falls/dashes
    // crisper without changing horizontal smoothness).
    double springFrequencyZMultiplier = 1.0;
    // Caps the spring's velocity magnitude (units/sec). 0 = unlimited.
    double springMaxSpeedUnitsPerSecond = 0.0;
    // "Looks like linear" switch: when the spring has converged to within this
    // distance (units) of the interpolated linear target, render EXACTLY at the
    // linear target (pixel-identical to linear mode). When a real discontinuity
    // pushes the error past the deadzone, render the spring value, which glides
    // the correction (no snap). 0 = never snap to the linear target.
    double springLinearDeadzoneUnits = 0.05;
    // Hybrid (feed-forward spring): ω = 2π·frequency; stiffness = ω²,
    // damping = 2ω·damping_ratio. Higher frequency = crisper tracking (and
    // more sensitive to noise); damping_ratio 1.0 = critically damped (zero
    // overshoot), below 1.0 adds a little snap, above 1.0 is overdamped.
    double hybridFrequencyHz = 10.0;
    double hybridDampingRatio = 1.0;
    // 0..1 blend between a pure spring (0, smooth but follows with lag) and
    // full velocity feed-forward (1, crisp with ~zero lag). Tune smoothness
    // vs snappiness continuously.
    double hybridFeedForward = 1.0;
    // Scales hybrid frequency on the Z axis only, so up/down (jumps, falls,
    // dashes) can be made crisper without changing horizontal smoothness.
    double hybridFrequencyZMultiplier = 1.0;
    // Low-pass (0..1) applied to the velocity fed into the hybrid spring.
    // Higher = smoother but a touch laggier; reduces jitter caused by the
    // interpolated velocity's slope changes at snapshot boundaries.
    double hybridFeedForwardSmoothing = 0.4;
    // Caps the spring's velocity magnitude (units/sec) in hybrid/spring modes.
    // 0 = unlimited. Bounds corrections so a big discontinuity glides instead
    // of lurching.
    double hybridMaxSpeedUnitsPerSecond = 0.0;
    // Universal final hard cap on rendered per-frame movement (units/sec).
    // 0 = unlimited. The absolute "never teleport" guarantee across every mode.
    double filterMaxStepUnitsPerSecond = 0.0;
    // Floor guard (all modes): the rendered body's Z never drops below the
    // interpolated authoritative target's Z, so filter overshoot after a fast
    // landing cannot push the body through the floor.
    bool filterClampZBelowTarget = true;
    // linear mode: render N packets behind the newest. This is the loss buffer —
    // up to N consecutive lost packets are hidden because the body keeps
    // interpolating on older packets. Higher = smoother under loss, more delay.
    uint32_t linearDelayTicks = 6;
    // linear mode: when loss exceeds the buffer, true = CS-style hold at the
    // correct interpolated position (seamless resume, zero drift); false =
    // extrapolate along the last velocity (never stops, small pop if wrong).
    bool linearHoldOnDry = true;
    // linear mode adaptive buffer range (in whole ticks). When
    // linear_max_delay_ticks > 0, the delay auto-deepens from
    // linear_min_delay_ticks toward linear_max_delay_ticks under jitter/loss so
    // the render never runs out of packets (no extrapolation/hold pops). When
    // 0, the delay is fixed at linear_delay_ticks (legacy behavior).
    uint32_t linearMinDelayTicks = 6;
    uint32_t linearMaxDelayTicks = 16;
    // linear mode: explicit extrapolation switch, independent of hold_on_dry.
    // Extrapolation is used only when this is true AND linear_hold_on_dry is
    // false; otherwise the body holds its last interpolated position when the
    // buffer runs dry (seamless resume, no snap).
    bool linearAllowExtrapolation = false;
    // linear mode: max ticks/sec the render clock may catch up after a data
    // gap. 0 = the clock only advances at real time (smoothest resume; after a
    // gap the body just continues from where it held). >0 = allow a bounded
    // catch-up burst so the body re-syncs to live data faster.
    double linearCatchupRateTicksPerSecond = 0.0;
    // linear mode: scales how much arrival jitter deepens the loss buffer
    // (multiplied on top of adaptive_snapshot_buffer.jitter_multiplier).
    // 1.0 = standard; raise to deepen faster on jittery connections.
    double linearDelayJitterMultiplier = 1.0;
    // linear mode: scales how much packet loss deepens the loss buffer toward
    // linear_max_delay_ticks (weight on the smoothed loss fraction).
    // 1.0 = standard; 0 = ignore loss when sizing the buffer.
    double linearDelayLossWeight = 1.0;
    // linear mode: a gap wider than this many ticks between two buffered
    // snapshots is a discontinuity (blackout/teleport) and snaps to the newest
    // instead of bridging a stale straight line. 0 = use the global
    // teleport_gap_ticks.
    uint32_t linearSnapAfterGapTicks = 30;
    // linear mode: rendered deltas below this many units from the previous
    // rendered position snap to the previous position exactly (kills
    // standing-still micro-jitter from tiny broadcast noise). 0 = disabled.
    // Real movement above the threshold passes through untouched.
    double linearDeadzoneUnits = 0.0;
    // linear mode clock source. "wall_time" advances from monotonic real time;
    // any other value falls back to frame dt for debugging only.
    std::string linearClockSource = "wall_time";
    // linear mode safety re-anchor. Ordinary snapshot bursts must not move the
    // render clock; this fires only when the estimated server clock falls far
    // behind the newest received tick (usually after a long pause/blackout).
    bool linearReanchorEnabled = true;
    double linearReanchorOnlyIfErrorSeconds = 0.250;
    // Diagnostic threshold only: logs when the delayed render sample advances
    // by more than this many ms in one rendered frame.
    double maxRenderTimeJumpSeconds = 0.005;
};

struct NetworkDeathEffectsConfig
{
    // Spawn the death ellipsoid on remote actor deaths (players + NPCs) detected
    // from the snapshot health transition, so every client (attacker, victim,
    // observers) sees it even under packet loss.
    bool remotePlayerDeathEffect = true;
    // Spawn the death ellipsoid on the local player's server-confirmed death.
    bool localPlayerDeathEffect = true;
};

struct AdaptiveSnapshotBufferConfig
{
    bool enabled = true;
    double minimumDelaySeconds = 0.016;
    double maximumDelaySeconds = 0.120;
    double jitterMultiplier = 2.0;
    double arrivalJitterSmoothing = 0.15;
    double increaseRateMsPerSecond = 200.0;
    double decreaseRateMsPerSecond = 150.0;
    // Loss-driven buffer growth: when snapshots arrive with a tick gap larger
    // than `lossGapTicks`, the delay grows by up to `lossDelayBudgetSeconds`
    // (scaled by a smoothed loss fraction), so the render stays deep inside
    // the buffer and never extrapolates/holds under packet loss.
    uint32_t lossGapTicks = 2;
    double lossDelayBudgetSeconds = 0.080;
    double lossSmoothing = 0.10;
};

struct NetworkSnapshotRedundancyConfig
{
    // Re-send the previous tick's chunk alongside the current one when the
    // snapshot is a single datagram, so one lost packet rarely drops a whole
    // snapshot tick on the receiving client.
    bool enabled = true;
};

struct NetworkHitFeedbackConfig
{
    // Server-confirmed hit feedback. The client already shows an instant
    // predicted hitmarker/number/sound on its local trace; these gate whether
    // the server's DamageConfirmedEvent replays them (default: prediction-only,
    // so a hit produces exactly one crisp feedback).
    bool showConfirmedHitmarker = false;
    bool showConfirmedDamageNumber = false;
    bool showConfirmedHitSound = false;
};

struct NetworkDisagreementConfig
{
    // "Server disagree" visuals (correction indicators, HIT REJECTED effects).
    // Disable when client prediction is trusted and disagreements are rare.
    bool enabled = true;
};

struct NetworkEventTimelineConfig
{
    bool enabled = true;
    double remoteEffectMaximumHoldMs = 250.0;
    bool logDelays = false;
};

struct NetworkRuntimeRateConfig
{
    double inputSendRateHz = 60.0;
    double pingIntervalMs = 1000.0;
};

struct NetworkRetryConfig
{
    double attackRetryIntervalMs = 100.0;
    uint32_t attackRetryMaxAttempts = 10;
    double attackRequestTimeoutMs = 3000.0;
    double reconnectInitialBackoffMs = 1000.0;
    uint32_t reconnectMaxAttempts = 10;
    double reconnectMaxBackoffMs = 15000.0;
};

struct ReliableGameplayEventConfig
{
    std::size_t maxPendingPerPlayer = 64;
    double retryMs = 100.0;
    double ttlMs = 10000.0;
    uint32_t maxAttempts = 80;
};

struct NetworkBufferLimitConfig
{
    std::size_t serverPositionHistoryTicks = 30;
    std::size_t serverBroadcastSampleLimit = 128;
};

struct RemoteEntityLifecycleConfig
{
    // A remote entity is only removed after being absent from this many
    // complete, newer membership snapshots...
    uint32_t missingSnapshotConfirmationCount = 3;
    // ...and after this much wall-clock time has elapsed since first absence.
    double missingSnapshotGraceMs = 1000.0;
    // Removal requires a snapshot newer than the newest applied membership
    // snapshot (never an out-of-order older snapshot).
    bool requireNewerCompleteSnapshots = true;
};

struct NetworkingTimeoutConfig
{
    double clientTimeoutMs = 10000.0;
    double serverTimeoutMs = 10000.0;
    double connectTimeoutMs = 6000.0;
};

struct NetworkingDebugConfig
{
    bool showRemoteSnapshotPositions = false;
    bool showInterpolatedPosition = false;
    bool showBufferSize = false;
    bool logSnapshotArrival = false;
    bool logInterpolationState = false;
    double interpolationLogRateHz = 10.0;
    bool detectInterpolationJitter = true;
    double maxAllowedAlphaJump = 0.35;
    double maxAllowedVisualDeltaMultiplier = 2.5;
};

struct NetworkPredictionConfig
{
    // Predict the target's damage numbers + health bar on the local trace.
    // Hitmarker + hit sound are ALWAYS predicted regardless of this toggle.
    bool predictDamage = true;
    // Predict lethal deaths (instant death animation + kill heal) with rollback
    // when the server disagrees. When false, deaths are server-confirmed only.
    bool predictDeaths = false;
};

struct NetworkingConfigData
{
    int version = 1;
    bool hotReloadEnabled = true;
    double pollIntervalMs = 250.0;
    bool logChanges = true;
    RemotePlayerInterpolationConfig remotePlayers;
    LocalReconciliationConfig localReconciliation;
    SnapshotBufferConfig snapshotBuffer;
    RemoteMotionSmoothingConfig remoteMotionSmoothing;
    NetworkDeathEffectsConfig deathEffects;
    AdaptiveSnapshotBufferConfig adaptiveSnapshotBuffer;
    NetworkSnapshotRedundancyConfig snapshotRedundancy;
    NetworkHitFeedbackConfig hitFeedback;
    NetworkDisagreementConfig disagreement;
    NetworkEventTimelineConfig eventTimeline;
    NetworkRuntimeRateConfig runtimeRates;
    NetworkRetryConfig retries;
    ReliableGameplayEventConfig reliableEvents;
    NetworkBufferLimitConfig bufferLimits;
    RemoteEntityLifecycleConfig remoteEntityLifecycle;
    NetworkingTimeoutConfig timeouts;
    NetworkingDebugConfig debug;
    NetworkPredictionConfig prediction;
};

// Singleton config following the repo convention (WeaponHitFxConfig,
// GameplayConfig). Loads config/networkingconfig.json, validates the whole
// file, and only swaps the active data on success. Hot-reloads via pollReload().
class NetworkingConfig
{
public:
    static NetworkingConfig& instance();

    bool load(const std::string& path = defaultPath());
    bool pollReload();
    bool reloadFromDisk();
    void resetToDefaults();
    void clearOverrides();

    const NetworkingConfigData& data() const { return mData; }
    std::string configPath() const { return mPath; }

    // Runtime overrides (set by terminal commands). They only affect memory
    // until the file is reloaded; reloadFromDisk()/file change clears them.
    void setOverrideInterpolationDelayMs(double ms);
    std::optional<double> overrideInterpolationDelayMs() const { return mOverrideInterpolationDelayMs; }
    double effectiveRemoteInterpolationDelaySeconds() const;

    static std::string defaultPath();

private:
    NetworkingConfig() = default;

    bool loadFromFile(const std::string& path, NetworkingConfigData& out,
                      std::string& error) const;

    NetworkingConfigData mData;
    std::string mPath;
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
    std::optional<double> mOverrideInterpolationDelayMs;
};
