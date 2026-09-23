// 09 23 2026
/* purpose
* ONE hot-owned lag-compensation policy body shared by the two EXE seams:
*   - target-tick selection (was the `net.rewind` event handler body), and
*   - pose selection at that tick (was the `history.select` handler body).
* Both stages are inline functions in this header so a single edit to this file
* changes the whole lag-compensation ordering live, without rebuilding the EXE.
* Context-free: plain numbers and samples only; no Player/Npc/pointer types.
* Does NOT own history storage, collision, sockets, or the seams themselves
* (runtime/policy module wires these functions to `net.rewind`/`history.select`).
*/
#pragma once

#include <cmath>
#include <cstdint>

#include "hot-reload/hot-history.h"
#include "hot-reload/hot-rewind.h"

namespace MimitaLagComp {

// Simulation rate used to convert seconds to ticks. One definition for both
// stages so target-tick and interpolation rounding cannot drift apart.
inline constexpr double kSimHz = 60.0;

// Stage 1: choose the rewind target tick from timing + history facts.
// Returns the target tick; sets `outHandled`, `outAllow`, `outClamped`,
// `outReject`. This is exactly the previous `net.rewind` policy body.
inline std::uint32_t selectTargetTick(const GameRewindPolicyV1& in,
                                      std::uint32_t* outHandled,
                                      std::uint32_t* outAllow,
                                      std::uint32_t* outClamped,
                                      std::uint32_t* outReject)
{
    const auto set = [](std::uint32_t* p, std::uint32_t v) { if (p) *p = v; };
    set(outHandled, 0u);
    set(outAllow, 0u);
    set(outClamped, 0u);
    set(outReject, 0u);

    // Generation mismatch: never evaluate across incompatible behavior
    // generations; reject conservatively.
    if (in.attackerGeneration != in.currentGeneration ||
        in.targetGeneration != in.currentGeneration) {
        set(outReject, 1u);
        set(outHandled, 1u);
        return in.commandTick;
    }

    const int64_t interpTicks =
        (int64_t)std::llround((double)in.interpolationDelaySeconds * kSimHz);
    const int64_t compTicks =
        (int64_t)std::llround((double)in.compensationSeconds * kSimHz);
    const int64_t pingTicks =
        (int64_t)std::llround((double)in.measuredLatencySeconds * kSimHz);

    int64_t rewind;
    if (in.commandTick != 0u) {
        rewind = (int64_t)in.commandTick - interpTicks - compTicks;
    } else if (in.acceptedClientTick != 0u && in.acceptedServerTick != 0u) {
        rewind = (int64_t)in.acceptedServerTick - interpTicks - compTicks - pingTicks;
    } else {
        rewind = (int64_t)in.currentTick - interpTicks - compTicks - pingTicks;
    }

    if (in.maxRewindTicks > 0u) {
        const int64_t floor = (int64_t)in.currentTick - (int64_t)in.maxRewindTicks;
        if (rewind < floor) {
            rewind = floor;
            set(outClamped, 1u);
        }
    }
    if (rewind < 0)
        rewind = 0;
    if (rewind > (int64_t)in.currentTick)
        rewind = (int64_t)in.currentTick;

    set(outAllow, 1u);
    set(outHandled, 1u);
    return (std::uint32_t)rewind;
}

// Stage 2: choose the pose at the target tick from the raw bracketing samples.
// Writes selection/fraction/pose into `p` and sets `p->handled`. This is exactly
// the previous `history.select` policy body.
inline void selectPose(MimitaNet::GameHistorySelectV1* p)
{
    if (!p)
        return;
    p->handled = 0u;
    p->fraction = 0.0f;

    if (p->exactTick && p->haveA) {
        p->selection = (std::uint32_t)MimitaNet::HistorySelectionV1::ExactTick;
        for (int i = 0; i < 3; ++i) {
            p->position[i] = p->a.position[i];
            p->velocity[i] = p->a.velocity[i];
        }
        p->yaw = p->a.yaw;
        p->handled = 1u;
        return;
    }

    if (p->haveA && p->haveB) {
        // Generation boundary: never blend incompatible behavior generations;
        // clamp to the newer authoritative sample.
        if (p->a.logicalGenerationId != p->b.logicalGenerationId) {
            p->selection =
                (std::uint32_t)MimitaNet::HistorySelectionV1::GenerationClamp;
            for (int i = 0; i < 3; ++i) {
                p->position[i] = p->b.position[i];
                p->velocity[i] = p->b.velocity[i];
            }
            p->yaw = p->b.yaw;
            p->handled = 1u;
            return;
        }
        p->selection = (std::uint32_t)MimitaNet::HistorySelectionV1::Interpolated;
        const float frac = p->b.tick > p->a.tick
            ? (float)(p->targetTick - p->a.tick) / (float)(p->b.tick - p->a.tick)
            : 0.0f;
        p->fraction = frac;
        for (int i = 0; i < 3; ++i) {
            p->position[i] =
                p->a.position[i] + (p->b.position[i] - p->a.position[i]) * frac;
            p->velocity[i] =
                p->a.velocity[i] + (p->b.velocity[i] - p->a.velocity[i]) * frac;
        }
        p->yaw = p->a.yaw + (p->b.yaw - p->a.yaw) * frac;
        p->handled = 1u;
        return;
    }

    if (p->haveA) {
        p->selection = (std::uint32_t)MimitaNet::HistorySelectionV1::NearestOlder;
        for (int i = 0; i < 3; ++i) {
            p->position[i] = p->a.position[i];
            p->velocity[i] = p->a.velocity[i];
        }
        p->yaw = p->a.yaw;
        p->handled = 1u;
        return;
    }
}

} // namespace MimitaLagComp
