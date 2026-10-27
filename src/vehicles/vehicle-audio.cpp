// 2026-10-27

#include "vehicles/vehicle-audio.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "audio/audio.h"
#include "debug/structured-log.h"

namespace {

constexpr float kMinPeriodSeconds = 1.0f / 60.0f;
constexpr float kMaxPeriodSeconds = 0.8f;
constexpr float kMinPeriodHighRpm = 0.02f;

} // namespace

void stopVehicleEngineAudio(uint32_t vehicleId)
{
    AudioManager::instance().stopOwner(
        audioOwnerKey(vehicleId, "engine"));
}

void updateVehicleEngineAudio(VehicleState& state, const VehicleSpec& spec,
                              const glm::vec3& position, uint64_t tick)
{
    if (!state.engineOn)
    {
        if (state.audioEngineWasOn)
        {
            stopVehicleEngineAudio(state.id);
            state.audioEngineWasOn = false;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.audio.engine", std::to_string(state.id),
                "engine audio stopped", tick,
                {{"vehicleId", state.id}, {"engineOn", false}},
                __FILE__, __LINE__, __func__);
        }
        state.nextEnginePulseTick = 0;
        return;
    }

    if (!state.audioEngineWasOn)
    {
        state.audioEngineWasOn = true;
        state.nextEnginePulseTick = tick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.audio.engine", std::to_string(state.id),
            "engine audio started", tick,
            {{"vehicleId", state.id},
             {"engineOn", true},
             {"sound", spec.engineSound}},
            __FILE__, __LINE__, __func__);
    }

    const float redline = std::max(spec.engine.redlineRpm, 1.0f);
    const float t = std::clamp(state.engineRpm / redline, 0.0f, 1.0f);
    const float periodSeconds =
        std::max(kMinPeriodSeconds,
                 (1.0f - t) * kMaxPeriodSeconds + t * kMinPeriodHighRpm);
    const uint64_t periodTicks = std::max<uint64_t>(
        1, static_cast<uint64_t>(std::llround(periodSeconds * 60.0f)));

    if (tick < state.nextEnginePulseTick)
        return;
    state.nextEnginePulseTick = tick + periodTicks;

    AudioEvent event;
    event.name = spec.engineSound;
    event.category = AudioCategory::Ambient;
    event.world = true;
    event.position = position;
    event.volume = 0.28f + state.throttle * 0.42f;
    event.pitch = 0.85f + t * 0.35f;
    event.maxDistance = 45.0f;
    event.ownerId = audioOwnerKey(state.id, "engine");
    event.startSeconds = 0.0f;
    event.endSeconds = periodSeconds;
    event.retrigger = true;
    AudioManager::instance().play(event);

    ++state.enginePulsesSinceLog;
    if (tick - state.lastAudioLogTick >= 60)
    {
        state.lastAudioLogTick = tick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Verbose,
            "vehicle.audio.pulse", std::to_string(state.id),
            "engine pulse schedule sample", tick,
            {{"vehicleId", state.id},
             {"rpm", state.engineRpm},
             {"periodSeconds", periodSeconds},
             {"periodTicks", periodTicks},
             {"pulsesThisSecond", state.enginePulsesSinceLog}},
            __FILE__, __LINE__, __func__);
        state.enginePulsesSinceLog = 0;
    }
}
