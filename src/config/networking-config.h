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

struct AdaptiveSnapshotBufferConfig
{
    bool enabled = true;
    double minimumDelaySeconds = 0.033;
    double maximumDelaySeconds = 0.140;
    double jitterMultiplier = 2.0;
    double arrivalJitterSmoothing = 0.15;
    double increaseRateMsPerSecond = 240.0;
    double decreaseRateMsPerSecond = 60.0;
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
    AdaptiveSnapshotBufferConfig adaptiveSnapshotBuffer;
    NetworkEventTimelineConfig eventTimeline;
    NetworkRuntimeRateConfig runtimeRates;
    NetworkRetryConfig retries;
    ReliableGameplayEventConfig reliableEvents;
    NetworkBufferLimitConfig bufferLimits;
    RemoteEntityLifecycleConfig remoteEntityLifecycle;
    NetworkingTimeoutConfig timeouts;
    NetworkingDebugConfig debug;
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
