#include "debug/jitter-probe.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"

namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t nowMs()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count());
}

struct ActorPoseState {
    bool hasLast = false;
    bool hasLast2 = false;
    glm::vec3 last{0.0f};
    glm::vec3 last2{0.0f};
};

struct Accum {
    std::uint64_t samples = 0;
    double sum = 0.0;
    double max = 0.0;
    void add(double value) {
        samples++;
        sum += value;
        max = std::max(max, value);
    }
    double mean() const { return samples ? sum / (double)samples : 0.0; }
    void reset() { samples = 0; sum = 0.0; max = 0.0; }
};

struct ProbeState {
    bool running = false;
    std::uint64_t startMs = 0;
    std::uint64_t endMs = 0;
    std::uint64_t nextWindowMs = 0;
    Accum interpAlphaDelta;
    Accum interpFinalDelta;
    Accum interpSnaps; // per sample derived count is added at window time
    Accum actorAccel;
    Accum actorRawLag;
    Accum cameraPosAccel;
    Accum cameraAngAccel;
    Accum hudError;
    std::uint32_t snapSeen = 0;
    std::uint32_t underrunSeen = 0;
    std::uint32_t holdSeen = 0;
    std::unordered_map<std::string, ActorPoseState> actorPose;
    bool hasCamera = false;
    glm::vec3 lastCameraPos{0.0f};
    glm::vec3 lastCameraPos2{0.0f};
    bool hasCameraPos2 = false;
    glm::vec3 lastCameraEuler{0.0f};
    glm::vec3 lastCameraEuler2{0.0f};
    bool hasCamera2 = false;
};

ProbeState gProbe;

void emit(const char* event, nlohmann::json fields)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Performance, StructuredLevel::Important,
        event, "jitter-probe", "bounded jitter investigation sample", 0,
        std::move(fields), __FILE__, __LINE__, __FUNCTION__);
}

float shortestAngleDeg(float a, float b)
{
    float d = std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
    return d;
}

void emitWindow()
{
    const std::uint64_t now = nowMs();
    const double elapsed = (double)(now - gProbe.startMs) / 1000.0;
    emit("jitter.window", {
        {"elapsed_s", elapsed},
        {"interp", {
            {"samples", gProbe.interpAlphaDelta.samples},
            {"alpha_delta_mean", gProbe.interpAlphaDelta.mean()},
            {"alpha_delta_max", gProbe.interpAlphaDelta.max},
            {"final_delta_mean_m", gProbe.interpFinalDelta.mean()},
            {"final_delta_max_m", gProbe.interpFinalDelta.max},
            {"hard_snaps", gProbe.snapSeen},
            {"underruns", gProbe.underrunSeen},
            {"holds", gProbe.holdSeen}}},
        {"actor", {
            {"samples", gProbe.actorAccel.samples},
            {"accel_mean_m", gProbe.actorAccel.mean()},
            {"accel_max_m", gProbe.actorAccel.max},
            {"raw_lag_mean_m", gProbe.actorRawLag.mean()},
            {"raw_lag_max_m", gProbe.actorRawLag.max}}},
        {"camera", {
            {"samples", gProbe.cameraAngAccel.samples},
            {"pos_accel_mean_m", gProbe.cameraPosAccel.mean()},
            {"pos_accel_max_m", gProbe.cameraPosAccel.max},
            {"ang_accel_mean_deg", gProbe.cameraAngAccel.mean()},
            {"ang_accel_max_deg", gProbe.cameraAngAccel.max}}},
        {"hud", {
            {"samples", gProbe.hudError.samples},
            {"error_mean_px", gProbe.hudError.mean()},
            {"error_max_px", gProbe.hudError.max}}},
        {"thresholds_provisional", true}});
}

void emitVerdictAndStop(const char* reason)
{
    const std::uint64_t now = nowMs();
    const double elapsed = (double)(now - gProbe.startMs) / 1000.0;
    const bool interpOk = gProbe.interpAlphaDelta.max <= 0.25 &&
        gProbe.interpFinalDelta.max <= 0.50;
    const bool actorOk = gProbe.actorAccel.max <= 0.05 &&
        gProbe.actorRawLag.max <= 0.25;
    const bool cameraOk = gProbe.cameraAngAccel.max <= 2.0;
    const bool hudOk = gProbe.hudError.max <= 0.5;
    emit("jitter.verdict", {
        {"reason", reason},
        {"elapsed_s", elapsed},
        {"interp", {
            {"samples", gProbe.interpAlphaDelta.samples},
            {"alpha_delta_max", gProbe.interpAlphaDelta.max},
            {"final_delta_max_m", gProbe.interpFinalDelta.max},
            {"hard_snaps", gProbe.snapSeen},
            {"underruns", gProbe.underrunSeen},
            {"holds", gProbe.holdSeen},
            {"matches", interpOk}}},
        {"actor", {
            {"samples", gProbe.actorAccel.samples},
            {"accel_max_m", gProbe.actorAccel.max},
            {"raw_lag_max_m", gProbe.actorRawLag.max},
            {"matches", actorOk}}},
        {"camera", {
            {"samples", gProbe.cameraAngAccel.samples},
            {"ang_accel_max_deg", gProbe.cameraAngAccel.max},
            {"matches", cameraOk}}},
        {"hud", {
            {"samples", gProbe.hudError.samples},
            {"error_max_px", gProbe.hudError.max},
            {"matches", hudOk}}},
        {"matches", interpOk && actorOk && cameraOk && hudOk},
        {"thresholds_provisional", true}});

    gProbe = ProbeState{};
}

}  // namespace

namespace JitterProbe {

bool active()
{
    return gProbe.running && nowMs() < gProbe.endMs;
}

void start(double seconds)
{
    if (seconds <= 0.0 || seconds > 120.0) seconds = 10.0;
    gProbe = ProbeState{};
    gProbe.running = true;
    gProbe.startMs = nowMs();
    gProbe.endMs = gProbe.startMs + (std::uint64_t)(seconds * 1000.0);
    gProbe.nextWindowMs = gProbe.startMs + 1000;
    emit("jitter.probe.started", {
        {"duration_s", seconds},
        {"categories", {"interp", "actor", "camera", "hud"}}});
}

void stop()
{
    if (gProbe.running)
        emitVerdictAndStop("stop");
}

void sampleInterpolation(std::uint32_t actorId,
                         double sampleAlpha,
                         double sampleAlphaDelta,
                         float finalRenderDelta,
                         float finalVerticalDelta,
                         std::uint32_t hardSnaps,
                         std::uint32_t underruns,
                         std::uint32_t holds,
                         bool holding,
                         bool extrapolating,
                         bool sampleJumped,
                         std::size_t bufferDepth)
{
    if (!active()) return;
    (void)sampleAlpha;
    gProbe.interpAlphaDelta.add(std::fabs(sampleAlphaDelta));
    gProbe.interpFinalDelta.add(std::fabs(finalRenderDelta));
    gProbe.interpFinalDelta.add(std::fabs(finalVerticalDelta));
    if (hardSnaps > gProbe.snapSeen) gProbe.snapSeen = hardSnaps;
    if (underruns > gProbe.underrunSeen) gProbe.underrunSeen = underruns;
    if (holds > gProbe.holdSeen) gProbe.holdSeen = holds;
    (void)holding;
    (void)extrapolating;
    (void)sampleJumped;
    (void)bufferDepth;
}

void sampleActorPose(std::uint32_t actorId, const char* part,
                     const glm::vec3& interpPosition,
                     const glm::vec3& rawPosition,
                     float dt)
{
    if (!active()) return;
    (void)dt;
    const std::string key = std::to_string(actorId) + "|" +
        (part ? part : "mesh");
    ActorPoseState& state = gProbe.actorPose[key];
    const float lag = glm::length(interpPosition - rawPosition);
    gProbe.actorRawLag.add((double)lag);
    if (state.hasLast) {
        const glm::vec3 delta = interpPosition - state.last;
        const float accel = state.hasLast2
            ? glm::length(delta - (state.last - state.last2))
            : glm::length(delta);
        gProbe.actorAccel.add((double)accel);
    }
    state.last2 = state.last;
    state.hasLast2 = state.hasLast;
    state.last = interpPosition;
    state.hasLast = true;
}

void sampleCamera(const glm::vec3& position, const glm::vec3& euler, float dt)
{
    if (!active()) return;
    (void)dt;
    if (gProbe.hasCamera) {
        const float posAccel = gProbe.hasCameraPos2
            ? glm::length(position - 2.0f * gProbe.lastCameraPos +
                          gProbe.lastCameraPos2)
            : 0.0f;
        if (gProbe.hasCameraPos2)
            gProbe.cameraPosAccel.add((double)posAccel);
        if (gProbe.hasCamera2) {
            const glm::vec3 d = euler - gProbe.lastCameraEuler;
            const glm::vec3 prev = gProbe.lastCameraEuler - gProbe.lastCameraEuler2;
            const float angAccel = std::sqrt(
                std::pow(shortestAngleDeg((float)d.x, (float)prev.x), 2.0f) +
                std::pow(shortestAngleDeg((float)d.y, (float)prev.y), 2.0f) +
                std::pow(shortestAngleDeg((float)d.z, (float)prev.z), 2.0f));
            gProbe.cameraAngAccel.add((double)angAccel);
        }
    }
    gProbe.lastCameraPos2 = gProbe.lastCameraPos;
    gProbe.hasCameraPos2 = gProbe.hasCamera;
    gProbe.lastCameraEuler2 = gProbe.lastCameraEuler;
    gProbe.hasCamera2 = gProbe.hasCamera;
    gProbe.lastCameraPos = position;
    gProbe.lastCameraEuler = euler;
    gProbe.hasCamera = true;
}

void sampleHud(float x, float y, float expectedX, float expectedY,
               bool forcedCenter)
{
    if (!active() || !forcedCenter) return;
    gProbe.hudError.add((double)std::sqrt(
        (x - expectedX) * (x - expectedX) +
        (y - expectedY) * (y - expectedY)));
}

void endFrame()
{
    if (!gProbe.running) return;
    const std::uint64_t now = nowMs();
    if (now >= gProbe.endMs) {
        emitVerdictAndStop("duration");
        return;
    }
    if (now >= gProbe.nextWindowMs) {
        emitWindow();
        gProbe.nextWindowMs = now + 1000;
    }
}

}  // namespace JitterProbe
