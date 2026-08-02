// 07 31 2026, 20 15
/* purpose
* Implements the networking configuration loader, validator, and hot reload.
* Reads config/networkingconfig.json into a typed NetworkingConfigData and swaps
* it atomically only when the entire file validates.
* Does NOT parse packets, send data, or own server authority.
* Does NOT render debug overlays or run the fixed-step simulation tick.
*/

#include "config/networking-config.h"

#include "network/badconn/badconn.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/path_utils.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

double readDouble(const json& j, const char* key, double def)
{
    if (!j.contains(key)) return def;
    const json& v = j[key];
    if (v.is_number()) return v.get<double>();
    return def;
}

bool readBool(const json& j, const char* key, bool def)
{
    if (!j.contains(key)) return def;
    const json& v = j[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number()) return v.get<double>() != 0.0;
    return def;
}

std::string readString(const json& j, const char* key, const std::string& def)
{
    if (!j.contains(key)) return def;
    const json& v = j[key];
    if (v.is_string()) return v.get<std::string>();
    return def;
}

double clampMin(double value, double min)
{
    return value < min ? min : value;
}

} // namespace

NetworkingConfig& NetworkingConfig::instance()
{
    static NetworkingConfig config;
    return config;
}

std::string NetworkingConfig::defaultPath()
{
    // Resolve relative to the executable/repository config directory so the
    // game does not depend on the current working directory.
    return resolveAssetPath("config/networkingconfig.json");
}

void NetworkingConfig::resetToDefaults()
{
    mData = NetworkingConfigData{};
    mOverrideInterpolationDelayMs.reset();
    Debug::warn(Debug::Category::Networking,
                "[NETWORK CONFIG] reset to compiled defaults\n");
}

void NetworkingConfig::clearOverrides()
{
    mOverrideInterpolationDelayMs.reset();
}

void NetworkingConfig::setOverrideInterpolationDelayMs(double ms)
{
    mOverrideInterpolationDelayMs = clampMin(ms, 0.0);
}

double NetworkingConfig::effectiveRemoteInterpolationDelaySeconds() const
{
    if (mOverrideInterpolationDelayMs.has_value())
        return mOverrideInterpolationDelayMs.value() / 1000.0;
    return mData.remotePlayers.interpolationDelaySeconds;
}

bool NetworkingConfig::loadFromFile(const std::string& path,
                                    NetworkingConfigData& out,
                                    std::string& error) const
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        error = "cannot open file";
        return false;
    }

    json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& e)
    {
        error = std::string("malformed JSON: ") + e.what();
        return false;
    }

    if (!root.is_object())
    {
        error = "root is not a JSON object";
        return false;
    }

    if (root.contains("version"))
    {
        if (!root["version"].is_number())
        {
            error = "version must be a number";
            return false;
        }
        const int version = root["version"].get<int>();
        if (version != 1)
        {
            error = "unsupported config version " + std::to_string(version);
            return false;
        }
    }

    NetworkingConfigData next;

    next.hotReloadEnabled = readBool(root, "hot_reload_enabled", next.hotReloadEnabled);
    if (root.contains("hot_reload") && root["hot_reload"].is_object())
    {
        const json& hr = root["hot_reload"];
        next.hotReloadEnabled = readBool(hr, "enabled", next.hotReloadEnabled);
        next.pollIntervalMs = clampMin(readDouble(hr, "poll_interval_ms", next.pollIntervalMs), 50.0);
        next.logChanges = readBool(hr, "log_changes", next.logChanges);
    }

    // ── remote_player_interpolation ───────────────────────────────────
    if (root.contains("remote_player_interpolation") &&
        root["remote_player_interpolation"].is_object())
    {
        const json& r = root["remote_player_interpolation"];
        RemotePlayerInterpolationConfig& c = next.remotePlayers;
        c.enabled = readBool(r, "enabled", c.enabled);
        c.directRender = readBool(r, "direct_render", c.directRender);
        c.serverSmoothing = readBool(r, "server_smoothing", c.serverSmoothing);
        c.interpolationDelaySeconds =
            clampMin(readDouble(r, "interpolation_delay_ms", c.interpolationDelaySeconds * 1000.0) / 1000.0, 0.0);
        c.maximumBufferedSnapshots = (std::size_t)clampMin(
            readDouble(r, "maximum_buffered_snapshots", (double)c.maximumBufferedSnapshots), 2.0);
        c.minimumSnapshotsBeforeRendering = (std::size_t)clampMin(
            readDouble(r, "minimum_snapshots_before_rendering", (double)c.minimumSnapshotsBeforeRendering), 1.0);
        c.allowExtrapolation = readBool(r, "allow_extrapolation", c.allowExtrapolation);
        c.maximumExtrapolationSeconds =
            clampMin(readDouble(r, "maximum_extrapolation_ms", c.maximumExtrapolationSeconds * 1000.0) / 1000.0, 0.0);
        c.teleportDistance = (float)clampMin(readDouble(r, "teleport_distance", (double)c.teleportDistance), 0.1);
        c.snapOnSpawn = readBool(r, "snap_on_spawn", c.snapOnSpawn);
        c.snapOnRespawn = readBool(r, "snap_on_respawn", c.snapOnRespawn);
        c.snapOnMapChange = readBool(r, "snap_on_map_change", c.snapOnMapChange);
    }

    // ── local_player_reconciliation ───────────────────────────────────
    if (root.contains("local_player_reconciliation") &&
        root["local_player_reconciliation"].is_object())
    {
        const json& r = root["local_player_reconciliation"];
        LocalReconciliationConfig& c = next.localReconciliation;
        c.enabled = readBool(r, "enabled", c.enabled);
        c.correctionMode = readString(r, "correction_mode", c.correctionMode);
        c.correctionDurationSeconds =
            clampMin(readDouble(r, "correction_duration_ms", c.correctionDurationSeconds * 1000.0) / 1000.0, 0.0);
        c.hardSnapDistance = (float)clampMin(readDouble(r, "hard_snap_distance", (double)c.hardSnapDistance), 0.1);
    }

    // ── snapshot_buffer ───────────────────────────────────────────────
    if (root.contains("snapshot_buffer") && root["snapshot_buffer"].is_object())
    {
        const json& r = root["snapshot_buffer"];
        SnapshotBufferConfig& c = next.snapshotBuffer;
        c.useServerTick = readBool(r, "use_server_tick", c.useServerTick);
        c.discardDuplicateSnapshots = readBool(r, "discard_duplicate_snapshots", c.discardDuplicateSnapshots);
        c.allowOutOfOrderInsertion = readBool(r, "allow_out_of_order_insertion", c.allowOutOfOrderInsertion);
        c.maximumSnapshotAgeSeconds =
            clampMin(readDouble(r, "maximum_snapshot_age_ms", c.maximumSnapshotAgeSeconds * 1000.0) / 1000.0, 0.1);
        c.chunkReassemblyTimeoutSeconds =
            clampMin(readDouble(r, "chunk_reassembly_timeout_ms", c.chunkReassemblyTimeoutSeconds * 1000.0) / 1000.0, 0.05);
    }

    // ── network_timeouts ──────────────────────────────────────────────
    if (root.contains("network_timeouts") && root["network_timeouts"].is_object())
    {
        const json& r = root["network_timeouts"];
        NetworkingTimeoutConfig& c = next.timeouts;
        c.clientTimeoutMs = clampMin(readDouble(r, "client_timeout_ms", c.clientTimeoutMs), 100.0);
        c.serverTimeoutMs = clampMin(readDouble(r, "server_timeout_ms", c.serverTimeoutMs), 100.0);
        c.connectTimeoutMs = clampMin(readDouble(r, "connect_timeout_ms", c.connectTimeoutMs), 100.0);
    }

    // ── debug ─────────────────────────────────────────────────────────
    if (root.contains("debug") && root["debug"].is_object())
    {
        const json& r = root["debug"];
        NetworkingDebugConfig& c = next.debug;
        c.showRemoteSnapshotPositions = readBool(r, "show_remote_snapshot_positions", c.showRemoteSnapshotPositions);
        c.showInterpolatedPosition = readBool(r, "show_interpolated_position", c.showInterpolatedPosition);
        c.showBufferSize = readBool(r, "show_buffer_size", c.showBufferSize);
        c.logSnapshotArrival = readBool(r, "log_snapshot_arrival", c.logSnapshotArrival);
        c.logInterpolationState = readBool(r, "log_interpolation_state", c.logInterpolationState);
    }

    out = next;
    return true;
}

bool NetworkingConfig::load(const std::string& path)
{
    if (mPath != path)
    {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged)
    {
        Debug::warn(Debug::Category::Networking,
                    "[NETWORK CONFIG] Watching: %s\n", mPath.c_str());
        mWatchLogged = true;
    }

    if (!std::filesystem::exists(mPath))
    {
        mLastWrite = getLastWrite(mPath);
        Debug::warn(Debug::Category::Networking,
                    "[NETWORK CONFIG] Missing %s; using compiled defaults. "
                    "Expected file: %s\n",
                    fileName.c_str(), mPath.c_str());
        return false;
    }

    NetworkingConfigData next;
    std::string error;
    if (!loadFromFile(mPath, next, error))
    {
        mLastWrite = getLastWrite(mPath);
        Debug::error(Debug::Category::Networking,
                     "[NETWORK CONFIG] Reload failed (%s); keeping previous "
                     "valid configuration.\n",
                     error.c_str());
        return false;
    }

    const bool changed = !(next.version == mData.version &&
                           next.remotePlayers.interpolationDelaySeconds == mData.remotePlayers.interpolationDelaySeconds &&
                           next.remotePlayers.enabled == mData.remotePlayers.enabled &&
                           next.remotePlayers.allowExtrapolation == mData.remotePlayers.allowExtrapolation);
    mData = next;
    mOverrideInterpolationDelayMs.reset();
    mLastWrite = getLastWrite(mPath);

    // The same file also owns the badconn preset block — re-apply it so both
    // share one source of truth and hot-reload together.
    badconn::loadConfig(badconn::configPath());

    Debug::warn(Debug::Category::Networking,
                "[NETWORK CONFIG] Loaded: %s (delay=%.0fms buffer=%zu extrap=%d)\n",
                fileName.c_str(),
                mData.remotePlayers.interpolationDelaySeconds * 1000.0,
                mData.remotePlayers.maximumBufferedSnapshots,
                (int)mData.remotePlayers.allowExtrapolation);
    if (mData.logChanges && changed)
    {
        Debug::warn(Debug::Category::Networking,
                    "[NETWORK CONFIG] NETWORK_CONFIG_VALUE_CHANGED: "
                    "interpolationDelayMs=%.0f enabled=%d extrapolation=%d\n",
                    mData.remotePlayers.interpolationDelaySeconds * 1000.0,
                    (int)mData.remotePlayers.enabled,
                    (int)mData.remotePlayers.allowExtrapolation);
    }
    return true;
}

bool NetworkingConfig::reloadFromDisk()
{
    mOverrideInterpolationDelayMs.reset();
    return load(mPath);
}

bool NetworkingConfig::pollReload()
{
    if (!mWatchLogged)
    {
        load(mPath.empty() ? defaultPath() : mPath);
        return false;
    }

    if (!mData.hotReloadEnabled || mPath.empty())
        return false;

    using Clock = std::chrono::steady_clock;
    static Clock::time_point nextCheck;
    const auto now = Clock::now();
    if (now < nextCheck)
        return false;
    nextCheck = now + std::chrono::milliseconds((long long)mData.pollIntervalMs);

    std::error_code ec;
    const auto wt = std::filesystem::last_write_time(mPath, ec);
    if (ec)
        return false;
    if (wt != mLastWrite)
    {
        mLastWrite = wt;
        Debug::warn(Debug::Category::Networking,
                    "[NETWORK CONFIG] NETWORK_CONFIG_CHANGE_DETECTED\n");
        return load(mPath);
    }
    return false;
}
