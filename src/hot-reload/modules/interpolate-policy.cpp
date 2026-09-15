// 09 15 2026
/* purpose
* net.interpolate: the ONE hot interpolation policy. Given samples + timing
* facts it owns: generation/lifecycle discontinuity, packet-gap response,
* buffer-dry (extrapolate vs hold), the extrapolation cap, delay retargeting,
* and the interpolation alpha. The cold sample-buffer mechanism still stores
* samples and performs the numeric mix/extrapolation with these decisions.
* Context-free: plain numbers only.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-interpolation.h"
#include "hot-reload/hot-package.h"

#include <cmath>

namespace {

// Hot-editable interpolation policy values.
constexpr std::uint32_t kInterpolate = 0u;
constexpr std::uint32_t kExtrapolate = 1u;
constexpr std::uint32_t kHold = 2u;
constexpr std::uint32_t kSnap = 3u;
constexpr std::uint32_t kReset = 4u;
constexpr double kMaxExtrapolationMs = 250.0;
constexpr std::uint32_t kLargeGapTicks = 30u;

void MIMITA_GAME_CALL onInterpolate(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameInterpolateV1*>(event->payload) : nullptr;
    if (!p)
        return;

    p->hardSnap = 0u;
    p->shouldResetBuffer = 0u;
    p->mode = kInterpolate;
    p->outAlpha = p->alpha;
    p->outDelaySeconds = p->delaySeconds;
    p->outExtrapolationMs = kMaxExtrapolationMs;

    // Adaptive interpolation-delay policy: cold supplies measurements; the hot
    // policy owns the desired delay (bounds, jitter/loss response, convergence
    // rate). This is the real owner of outDelaySeconds (not a pass-through).
    if (p->delayQuery != 0u) {
        float desired = p->baseDelaySeconds;
        if (p->minDelaySeconds > desired)
            desired = p->minDelaySeconds;
        const float jitterDelay =
            (p->estimatedJitterMs * p->jitterMultiplier) / 1000.0f;
        if (jitterDelay > desired)
            desired = jitterDelay;
        const float lossDelay =
            p->minDelaySeconds + p->recentLossFraction * p->lossDelayBudgetSeconds;
        if (lossDelay > desired)
            desired = lossDelay;
        if (desired > p->maxDelaySeconds)
            desired = p->maxDelaySeconds;

        const float cur = p->currentAdaptiveDelaySeconds;
        if (cur <= 0.0f) {
            p->outDelaySeconds = desired;
            p->handled = 1u;
            return;
        }
        const float rate = desired > cur ? p->increaseRateMsPerSecond
                                         : p->decreaseRateMsPerSecond;
        const float maxStep = (rate / 1000.0f) * p->deltaSeconds;
        const float delta = desired - cur;
        if (std::fabs(delta) <= maxStep)
            p->outDelaySeconds = desired;
        else
            p->outDelaySeconds = cur + (delta > 0.0f ? maxStep : -maxStep);
        p->handled = 1u;
        return;
    }

    // Generation / lifecycle discontinuity: never blend as normal motion.
    const bool genMismatch = p->aGeneration != p->bGeneration ||
                             p->bGeneration != p->currentGeneration;
    if (genMismatch || p->lifecycleChanged != 0u) {
        p->mode = kSnap;
        p->hardSnap = 1u;
        p->handled = 1u;
        return;
    }

    // Large packet gap: snap rather than bridge a hole with a near-1 alpha.
    if (p->packetGapTicks >= kLargeGapTicks) {
        p->mode = kSnap;
        p->hardSnap = 1u;
        p->handled = 1u;
        return;
    }

    // Buffer dry: hot decides extrapolate vs hold (hold is the conservative
    // default when extrapolation is not permitted).
    if (p->bufferDry != 0u) {
        p->mode = p->allowExtrapolation != 0u ? kExtrapolate : kHold;
        p->handled = 1u;
        return;
    }

    // Normal interpolation: policy owns the alpha range.
    if (p->outAlpha < 0.0)
        p->outAlpha = 0.0;
    if (p->outAlpha > 1.0)
        p->outAlpha = 1.0;

    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_interpolateRegistration{
    {GAME_EVENT_INTERPOLATE, 0, 0, onInterpolate, "net.interpolate"}};

#endif
