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
    uint32_t serverSmoothingDurationTicks = 2;
    double interpolationDelaySeconds = 0.033;
    std::size_t maximumBufferedSnapshots = 64;
    std::size_t minimumSnapshotsBeforeRendering = 2;
    bool allowExtrapolation = true;
    double maximumExtrapolationSeconds = 0.100;
    float teleportDistance = 1000.0f;
    bool snapOnSpawn = true;
    bool snapOnRespawn = true;
    bool snapOnMapChange = true;
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
