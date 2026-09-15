// 09 15 2026
/* purpose
* Implements the headless hot interpolation-policy self-test: alpha clamp,
* generation-boundary hard snap, and determinism. No Player/Npc/snapshot
* pointers; the payload is generic.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/interpolation-policy-selftest.h"

#include <string>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-interpolation.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

GameInterpolateV1 makeBase()
{
    GameInterpolateV1 p{};
    p.aTick = 100;
    p.bTick = 102;
    p.renderTick = 101.0;
    p.alpha = 0.5;
    p.aGeneration = 3;
    p.bGeneration = 3;
    p.currentGeneration = 3;
    p.handled = 0;
    return p;
}

GameInterpolateV1 run(GameInterpolateV1 p)
{
    LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_INTERPOLATE, &p, sizeof(p), 0, 0,
                                          0);
    return p;
}

} // namespace

bool runInterpolationPolicySelfTest(std::string& report)
{
    bool ok = true;
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    GameInterpolateV1 normal = run(makeBase());
    ok &= check(normal.handled == 1u && normal.hardSnap == 0u &&
                    normal.outAlpha == 0.5,
                "two normal samples -> interpolation alpha preserved", report);

    GameInterpolateV1 over = run([&] {
        GameInterpolateV1 p = makeBase();
        p.alpha = 1.7;
        return p;
    }());
    ok &= check(over.handled == 1u && over.outAlpha == 1.0,
                "alpha is clamped by the hot policy", report);

    GameInterpolateV1 under = run([&] {
        GameInterpolateV1 p = makeBase();
        p.alpha = -0.4;
        return p;
    }());
    ok &= check(under.handled == 1u && under.outAlpha == 0.0,
                "negative alpha is clamped by the hot policy", report);

    GameInterpolateV1 gen = run([&] {
        GameInterpolateV1 p = makeBase();
        p.bGeneration = 4;  // generation boundary between samples
        return p;
    }());
    ok &= check(gen.handled == 1u && gen.hardSnap == 1u,
                "generation boundary -> hard snap (no cross-generation blend)",
                report);

    GameInterpolateV1 cur = run([&] {
        GameInterpolateV1 p = makeBase();
        p.currentGeneration = 4;  // current differs from samples
        return p;
    }());
    ok &= check(cur.handled == 1u && cur.hardSnap == 1u,
                "current-generation mismatch -> hard snap", report);

    GameInterpolateV1 dryHold = run([&] {
        GameInterpolateV1 p = makeBase();
        p.bufferDry = 1u;
        p.allowExtrapolation = 0u;
        return p;
    }());
    ok &= check(dryHold.handled == 1u && dryHold.mode == 2u,
                "buffer dry + extrapolation denied -> hold", report);

    GameInterpolateV1 dryExtrap = run([&] {
        GameInterpolateV1 p = makeBase();
        p.bufferDry = 1u;
        p.allowExtrapolation = 1u;
        return p;
    }());
    ok &= check(dryExtrap.handled == 1u && dryExtrap.mode == 1u &&
                    dryExtrap.outExtrapolationMs > 0.0,
                "buffer dry + extrapolation allowed -> extrapolate with cap", report);

    GameInterpolateV1 gap = run([&] {
        GameInterpolateV1 p = makeBase();
        p.packetGapTicks = 40u;
        return p;
    }());
    ok &= check(gap.handled == 1u && gap.mode == 3u && gap.hardSnap == 1u,
                "large packet gap -> snap", report);

    auto delayQuery = [&](float current, float jitterMs, float lossFrac,
                          float minD, float maxD, float upRate, float downRate) {
        GameInterpolateV1 p{};
        p.delayQuery = 1u;
        p.currentAdaptiveDelaySeconds = current;
        p.baseDelaySeconds = 0.1f;
        p.estimatedJitterMs = jitterMs;
        p.recentLossFraction = lossFrac;
        p.minDelaySeconds = minD;
        p.maxDelaySeconds = maxD;
        p.increaseRateMsPerSecond = upRate;
        p.decreaseRateMsPerSecond = downRate;
        p.jitterMultiplier = 0.5f;
        p.lossDelayBudgetSeconds = 0.2f;
        p.deltaSeconds = 1.0f / 60.0f;
        return run(p);
    };

    GameInterpolateV1 stable = delayQuery(0.0f, 0.0f, 0.0f, 0.05f, 0.3f, 10.0f, 10.0f);
    ok &= check(stable.handled == 1u && stable.outDelaySeconds > 0.0 &&
                    stable.outDelaySeconds <= 0.3001,
                "adaptive delay: healthy stream stays within bounds", report);

    GameInterpolateV1 jitter = delayQuery(0.1f, 400.0f, 0.0f, 0.05f, 0.3f, 200.0f, 50.0f);
    ok &= check(jitter.handled == 1u && jitter.outDelaySeconds > 0.1f &&
                    jitter.outDelaySeconds <= 0.3001,
                "adaptive delay: jitter requests higher delay within max", report);

    GameInterpolateV1 loss = delayQuery(0.1f, 0.0f, 1.0f, 0.05f, 0.3f, 200.0f, 50.0f);
    ok &= check(loss.handled == 1u && loss.outDelaySeconds > 0.1f,
                "adaptive delay: buffer starvation/loss raises delay", report);

    GameInterpolateV1 recover = delayQuery(0.3f, 0.0f, 0.0f, 0.05f, 0.3f, 200.0f, 50.0f);
    ok &= check(recover.handled == 1u && recover.outDelaySeconds <= 0.3f,
                "adaptive delay: recovered stream permits reduction (not above current)",
                report);

    GameInterpolateV1 lo = delayQuery(0.0f, 0.0f, 0.0f, 0.05f, 0.05f, 10.0f, 10.0f);
    ok &= check(lo.handled == 1u && lo.outDelaySeconds >= 0.0499f,
                "adaptive delay: min bound respected", report);

    GameInterpolateV1 d1 = run(makeBase());
    GameInterpolateV1 d2 = run(makeBase());
    ok &= check(d1.outAlpha == d2.outAlpha && d1.hardSnap == d2.hardSnap &&
                    d1.mode == d2.mode,
                "interpolation classification is deterministic", report);

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
