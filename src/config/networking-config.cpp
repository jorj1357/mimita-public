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
#include <algorithm>
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

double clampRange(double value, double min, double max)
{
    return std::clamp(value, min, max);
}

uint32_t readUintRange(const json& j, const char* key, uint32_t def,
                       uint32_t min, uint32_t max)
{
    const double value = readDouble(j, key, (double)def);
    return (uint32_t)clampRange(value, (double)min, (double)max);
}

std::size_t readSizeRange(const json& j, const char* key, std::size_t def,
                          std::size_t min, std::size_t max)
{
    const double value = readDouble(j, key, (double)def);
    return (std::size_t)clampRange(value, (double)min, (double)max);
}

double readMsRangeSeconds(const json& j, const char* key, double defSeconds,
                          double minMs, double maxMs)
{
    const double valueMs = readDouble(j, key, defSeconds * 1000.0);
    return clampRange(valueMs, minMs, maxMs) / 1000.0;
}

double readMsRange(const json& j, const char* key, double defMs,
                   double minMs, double maxMs)
{
    return clampRange(readDouble(j, key, defMs), minMs, maxMs);
}

void logResolvedConfig(const NetworkingConfigData& c, const std::string& fileName)
{
    Debug::warn(Debug::Category::Networking,
        "[NETWORK CONFIG] Resolved file=%s direct=%d interp=%d fixedDelayMs=%.0f "
        "adaptive=%d adaptiveMinMaxMs=%.0f/%.0f minSnapshots=%zu "
        "eventTimeline=%d eventHoldMs=%.0f inputHz=%.0f pingMs=%.0f "
        "attackRetryMs=%.0f attackAttempts=%u attackTimeoutMs=%.0f "
        "reconnectBackoffMs=%.0f reconnectAttempts=%u reconnectMaxMs=%.0f "
        "reliablePending=%zu reliableRetryMs=%.0f reliableTtlMs=%.0f "
        "historyTicks=%zu broadcastSamples=%zu\n",
        fileName.c_str(),
        (int)c.remotePlayers.directRender,
        (int)c.remotePlayers.enabled,
        c.remotePlayers.interpolationDelaySeconds * 1000.0,
        (int)c.adaptiveSnapshotBuffer.enabled,
        c.adaptiveSnapshotBuffer.minimumDelaySeconds * 1000.0,
        c.adaptiveSnapshotBuffer.maximumDelaySeconds * 1000.0,
        c.remotePlayers.minimumSnapshotsBeforeRendering,
        (int)c.eventTimeline.enabled,
        c.eventTimeline.remoteEffectMaximumHoldMs,
        c.runtimeRates.inputSendRateHz,
        c.runtimeRates.pingIntervalMs,
        c.retries.attackRetryIntervalMs,
        c.retries.attackRetryMaxAttempts,
        c.retries.attackRequestTimeoutMs,
        c.retries.reconnectInitialBackoffMs,
        c.retries.reconnectMaxAttempts,
        c.retries.reconnectMaxBackoffMs,
        c.reliableEvents.maxPendingPerPlayer,
        c.reliableEvents.retryMs,
        c.reliableEvents.ttlMs,
        c.bufferLimits.serverPositionHistoryTicks,
        c.bufferLimits.serverBroadcastSampleLimit);
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
        c.serverSmoothingDurationTicks = (uint32_t)std::max<uint32_t>(
            1u, (uint32_t)clampMin(
                    readDouble(r, "server_smoothing_duration_ticks",
                               (double)c.serverSmoothingDurationTicks),
                    1.0));
        c.serverBroadcastDelaySeconds =
            clampMin(readDouble(r, "server_broadcast_delay_ms",
                                c.serverBroadcastDelaySeconds * 1000.0) / 1000.0, 0.0);
        c.serverBroadcastExtrapolationSeconds =
            clampMin(readDouble(r, "server_broadcast_extrapolation_ms",
                                c.serverBroadcastExtrapolationSeconds * 1000.0) / 1000.0, 0.0);
        c.rewindCompensationSeconds =
            readDouble(r, "rewind_compensation_ms",
                       c.rewindCompensationSeconds * 1000.0) / 1000.0;
        c.rewindHitTolerance = (float)std::max(
            0.1, readDouble(r, "rewind_hit_tolerance",
                            (double)c.rewindHitTolerance));
        c.serverBroadcastMaxSpeed = clampMin(
            readDouble(r, "server_broadcast_max_speed",
                       c.serverBroadcastMaxSpeed), 0.0);
        c.serverSimSmoothTicks = (uint32_t)std::max<uint32_t>(
            0u, (uint32_t)clampMin(
                    readDouble(r, "server_sim_smooth_ticks",
                               (double)c.serverSimSmoothTicks), 0.0));
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
        c.teleportGapTicks = (uint32_t)std::max<uint32_t>(
            1u, (uint32_t)clampMin(
                    readDouble(r, "teleport_gap_ticks", (double)c.teleportGapTicks),
                    1.0));
        {
            const std::string src = readString(r, "broadcast_source", c.broadcastSource);
            c.broadcastSource = (src == "client_report") ? "client_report" : "server_sim";
        }
        c.extrapolationKeepMoving = readBool(r, "extrapolation_keep_moving", c.extrapolationKeepMoving);
        c.teleportGapSnap = readBool(r, "teleport_gap_snap", c.teleportGapSnap);
        c.snapOnSpawn = readBool(r, "snap_on_spawn", c.snapOnSpawn);
        c.snapOnRespawn = readBool(r, "snap_on_respawn", c.snapOnRespawn);
        c.snapOnMapChange = readBool(r, "snap_on_map_change", c.snapOnMapChange);
        c.positionMode = readString(r, "position_mode", c.positionMode);
        c.rotationMode = readString(r, "rotation_mode", c.rotationMode);
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

    // ── remote_motion_smoothing ──────────────────────────────────────
    if (root.contains("remote_motion_smoothing") &&
        root["remote_motion_smoothing"].is_object())
    {
        const json& r = root["remote_motion_smoothing"];
        RemoteMotionSmoothingConfig& c = next.remoteMotionSmoothing;
        {
            const std::string mode = readString(r, "render_filter", c.renderFilter);
            c.renderFilter = (mode == "direct" || mode == "spring" ||
                              mode == "hybrid" || mode == "linear")
                ? mode : "bounded";
        }
        c.correctionMaxStepUnitsPerSecond = clampMin(
            readDouble(r, "correction_max_step_units_per_second",
                       c.correctionMaxStepUnitsPerSecond), 0.0);
        c.correctionMinDeltaUnits = clampMin(
            readDouble(r, "correction_min_delta_units",
                       c.correctionMinDeltaUnits), 0.0);
        c.springStiffness = clampMin(
            readDouble(r, "spring_stiffness", c.springStiffness), 1.0);
        c.springDamping = clampMin(
            readDouble(r, "spring_damping", c.springDamping), 1.0);
        c.springFrequencyHz = clampRange(
            readDouble(r, "spring_frequency_hz", c.springFrequencyHz), 0.0, 60.0);
        c.springDampingRatio = clampRange(
            readDouble(r, "spring_damping_ratio", c.springDampingRatio), 0.0, 3.0);
        c.springFeedForward = clampRange(
            readDouble(r, "spring_feed_forward", c.springFeedForward), 0.0, 1.0);
        c.springFeedForwardSmoothing = clampRange(
            readDouble(r, "spring_feed_forward_smoothing", c.springFeedForwardSmoothing), 0.0, 1.0);
        c.springFrequencyZMultiplier = clampRange(
            readDouble(r, "spring_frequency_z_multiplier", c.springFrequencyZMultiplier), 0.5, 3.0);
        c.springMaxSpeedUnitsPerSecond = clampMin(
            readDouble(r, "spring_max_speed_units_per_second",
                       c.springMaxSpeedUnitsPerSecond), 0.0);
        c.springLinearDeadzoneUnits = clampMin(
            readDouble(r, "spring_linear_deadzone_units",
                       c.springLinearDeadzoneUnits), 0.0);
        c.hybridFrequencyHz = clampRange(
            readDouble(r, "hybrid_frequency_hz", c.hybridFrequencyHz), 1.0, 60.0);
        c.hybridDampingRatio = clampRange(
            readDouble(r, "hybrid_damping_ratio", c.hybridDampingRatio), 0.0, 3.0);
        c.hybridFeedForward = clampRange(
            readDouble(r, "hybrid_feed_forward", c.hybridFeedForward), 0.0, 1.0);
        c.hybridFrequencyZMultiplier = clampRange(
            readDouble(r, "hybrid_frequency_z_multiplier", c.hybridFrequencyZMultiplier), 0.5, 3.0);
        c.hybridFeedForwardSmoothing = clampRange(
            readDouble(r, "hybrid_feed_forward_smoothing", c.hybridFeedForwardSmoothing), 0.0, 1.0);
        c.hybridMaxSpeedUnitsPerSecond = clampMin(
            readDouble(r, "hybrid_max_speed_units_per_second",
                       c.hybridMaxSpeedUnitsPerSecond), 0.0);
        c.filterMaxStepUnitsPerSecond = clampMin(
            readDouble(r, "filter_max_step_units_per_second",
                       c.filterMaxStepUnitsPerSecond), 0.0);
        c.filterClampZBelowTarget = readBool(
            r, "filter_clamp_z_below_target", c.filterClampZBelowTarget);
        c.linearDelayTicks = (uint32_t)std::max<uint32_t>(
            1u, (uint32_t)clampMin(
                    readDouble(r, "linear_delay_ticks", (double)c.linearDelayTicks),
                    1.0));
        c.linearHoldOnDry = readBool(r, "linear_hold_on_dry", c.linearHoldOnDry);
        c.linearMinDelayTicks = (uint32_t)std::max<uint32_t>(
            1u, (uint32_t)clampMin(
                    readDouble(r, "linear_min_delay_ticks", (double)c.linearMinDelayTicks),
                    1.0));
        c.linearMaxDelayTicks = (uint32_t)clampMin(
            readDouble(r, "linear_max_delay_ticks", (double)c.linearMaxDelayTicks),
            0.0);
        if (c.linearMaxDelayTicks > 0 &&
            c.linearMaxDelayTicks < c.linearMinDelayTicks)
            c.linearMaxDelayTicks = c.linearMinDelayTicks;
        c.linearAllowExtrapolation = readBool(
            r, "linear_allow_extrapolation", c.linearAllowExtrapolation);
        c.linearCatchupRateTicksPerSecond = clampMin(
            readDouble(r, "linear_catchup_rate_ticks_per_second",
                       c.linearCatchupRateTicksPerSecond), 0.0);
        c.linearDelayJitterMultiplier = clampMin(
            readDouble(r, "linear_delay_jitter_multiplier",
                       c.linearDelayJitterMultiplier), 0.0);
        c.linearDelayLossWeight = clampMin(
            readDouble(r, "linear_delay_loss_weight",
                       c.linearDelayLossWeight), 0.0);
        c.linearSnapAfterGapTicks = (uint32_t)std::max<uint32_t>(
            0u, (uint32_t)clampMin(
                    readDouble(r, "linear_snap_after_gap_ticks",
                               (double)c.linearSnapAfterGapTicks), 0.0));
    }

    // ── death_effects ────────────────────────────────────────────────
    if (root.contains("death_effects") && root["death_effects"].is_object())
    {
        const json& r = root["death_effects"];
        NetworkDeathEffectsConfig& c = next.deathEffects;
        c.remotePlayerDeathEffect = readBool(r, "remote_player_death_effect", c.remotePlayerDeathEffect);
        c.localPlayerDeathEffect = readBool(r, "local_player_death_effect", c.localPlayerDeathEffect);
    }

    // ── adaptive_snapshot_buffer ─────────────────────────────────────
    if (root.contains("adaptive_snapshot_buffer") &&
        root["adaptive_snapshot_buffer"].is_object())
    {
        const json& r = root["adaptive_snapshot_buffer"];
        AdaptiveSnapshotBufferConfig& c = next.adaptiveSnapshotBuffer;
        c.enabled = readBool(r, "enabled", c.enabled);
        c.minimumDelaySeconds = readMsRangeSeconds(
            r, "minimum_delay_ms", c.minimumDelaySeconds, 0.0, 250.0);
        c.maximumDelaySeconds = readMsRangeSeconds(
            r, "maximum_delay_ms", c.maximumDelaySeconds, 0.0, 500.0);
        if (c.maximumDelaySeconds < c.minimumDelaySeconds)
            c.maximumDelaySeconds = c.minimumDelaySeconds;
        c.jitterMultiplier = clampRange(
            readDouble(r, "jitter_multiplier", c.jitterMultiplier), 0.0, 10.0);
        c.arrivalJitterSmoothing = clampRange(
            readDouble(r, "arrival_jitter_smoothing", c.arrivalJitterSmoothing),
            0.01, 1.0);
        c.increaseRateMsPerSecond = readMsRange(
            r, "increase_rate_ms_per_second", c.increaseRateMsPerSecond, 1.0, 2000.0);
        c.decreaseRateMsPerSecond = readMsRange(
            r, "decrease_rate_ms_per_second", c.decreaseRateMsPerSecond, 1.0, 2000.0);
        c.lossGapTicks = (uint32_t)std::max<uint32_t>(
            1u, (uint32_t)clampMin(
                    readDouble(r, "loss_gap_ticks", (double)c.lossGapTicks), 1.0));
        c.lossDelayBudgetSeconds =
            readMsRangeSeconds(r, "loss_delay_budget_ms", c.lossDelayBudgetSeconds,
                               0.0, 500.0);
        c.lossSmoothing = clampRange(
            readDouble(r, "loss_smoothing", c.lossSmoothing), 0.01, 1.0);
    }

    // ── snapshot_redundancy ──────────────────────────────────────────
    if (root.contains("snapshot_redundancy") &&
        root["snapshot_redundancy"].is_object())
    {
        const json& r = root["snapshot_redundancy"];
        NetworkSnapshotRedundancyConfig& c = next.snapshotRedundancy;
        c.enabled = readBool(r, "enabled", c.enabled);
    }

    // ── confirmed_hit_feedback ───────────────────────────────────────
    if (root.contains("confirmed_hit_feedback") &&
        root["confirmed_hit_feedback"].is_object())
    {
        const json& r = root["confirmed_hit_feedback"];
        NetworkHitFeedbackConfig& c = next.hitFeedback;
        c.showConfirmedHitmarker = readBool(r, "show_hitmarker", c.showConfirmedHitmarker);
        c.showConfirmedDamageNumber = readBool(r, "show_damage_number", c.showConfirmedDamageNumber);
        c.showConfirmedHitSound = readBool(r, "show_hit_sound", c.showConfirmedHitSound);
    }

    // ── disagreement_visuals ─────────────────────────────────────────
    if (root.contains("disagreement_visuals") &&
        root["disagreement_visuals"].is_object())
    {
        const json& r = root["disagreement_visuals"];
        NetworkDisagreementConfig& c = next.disagreement;
        c.enabled = readBool(r, "enabled", c.enabled);
    }

    // ── event_timeline ───────────────────────────────────────────────
    if (root.contains("event_timeline") && root["event_timeline"].is_object())
    {
        const json& r = root["event_timeline"];
        NetworkEventTimelineConfig& c = next.eventTimeline;
        c.enabled = readBool(r, "enabled", c.enabled);
        c.remoteEffectMaximumHoldMs = readMsRange(
            r, "remote_effect_maximum_hold_ms", c.remoteEffectMaximumHoldMs,
            0.0, 1000.0);
        c.logDelays = readBool(r, "log_delays", c.logDelays);
    }

    // ── runtime_rates ────────────────────────────────────────────────
    if (root.contains("runtime_rates") && root["runtime_rates"].is_object())
    {
        const json& r = root["runtime_rates"];
        NetworkRuntimeRateConfig& c = next.runtimeRates;
        c.inputSendRateHz = clampRange(
            readDouble(r, "input_send_rate_hz", c.inputSendRateHz), 1.0, 240.0);
        c.pingIntervalMs = readMsRange(
            r, "ping_interval_ms", c.pingIntervalMs, 100.0, 10000.0);
    }

    // ── retries ──────────────────────────────────────────────────────
    if (root.contains("retries") && root["retries"].is_object())
    {
        const json& r = root["retries"];
        NetworkRetryConfig& c = next.retries;
        c.attackRetryIntervalMs = readMsRange(
            r, "attack_retry_interval_ms", c.attackRetryIntervalMs, 10.0, 2000.0);
        c.attackRetryMaxAttempts = readUintRange(
            r, "attack_retry_max_attempts", c.attackRetryMaxAttempts, 1, 100);
        c.attackRequestTimeoutMs = readMsRange(
            r, "attack_request_timeout_ms", c.attackRequestTimeoutMs, 100.0, 30000.0);
        c.reconnectInitialBackoffMs = readMsRange(
            r, "reconnect_initial_backoff_ms", c.reconnectInitialBackoffMs, 100.0, 30000.0);
        c.reconnectMaxAttempts = readUintRange(
            r, "reconnect_max_attempts", c.reconnectMaxAttempts, 1, 100);
        c.reconnectMaxBackoffMs = readMsRange(
            r, "reconnect_max_backoff_ms", c.reconnectMaxBackoffMs,
            c.reconnectInitialBackoffMs, 60000.0);
    }

    // ── reliable_gameplay_events ─────────────────────────────────────
    if (root.contains("reliable_gameplay_events") &&
        root["reliable_gameplay_events"].is_object())
    {
        const json& r = root["reliable_gameplay_events"];
        ReliableGameplayEventConfig& c = next.reliableEvents;
        c.maxPendingPerPlayer = readSizeRange(
            r, "max_pending_per_player", c.maxPendingPerPlayer, 1, 1024);
        c.retryMs = readMsRange(r, "retry_ms", c.retryMs, 10.0, 5000.0);
        c.ttlMs = readMsRange(r, "ttl_ms", c.ttlMs, 100.0, 60000.0);
        c.maxAttempts = readUintRange(r, "max_attempts", c.maxAttempts, 1, 255);
    }

    // ── buffer_limits ────────────────────────────────────────────────
    if (root.contains("buffer_limits") && root["buffer_limits"].is_object())
    {
        const json& r = root["buffer_limits"];
        NetworkBufferLimitConfig& c = next.bufferLimits;
        c.serverPositionHistoryTicks = readSizeRange(
            r, "server_position_history_ticks", c.serverPositionHistoryTicks, 2, 600);
        c.serverBroadcastSampleLimit = readSizeRange(
            r, "server_broadcast_sample_limit", c.serverBroadcastSampleLimit, 2, 1024);
    }

    // ── remote_entity_lifecycle ───────────────────────────────────────
    if (root.contains("remote_entity_lifecycle") &&
        root["remote_entity_lifecycle"].is_object())
    {
        const json& r = root["remote_entity_lifecycle"];
        RemoteEntityLifecycleConfig& c = next.remoteEntityLifecycle;
        c.missingSnapshotConfirmationCount = (uint32_t)clampMin(
            readDouble(r, "missing_snapshot_confirmation_count",
                       (double)c.missingSnapshotConfirmationCount),
            1.0);
        c.missingSnapshotGraceMs = clampMin(
            readDouble(r, "missing_snapshot_grace_ms", c.missingSnapshotGraceMs),
            0.0);
        c.requireNewerCompleteSnapshots =
            readBool(r, "require_newer_complete_snapshots", c.requireNewerCompleteSnapshots);
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

    logResolvedConfig(mData, fileName);
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
