// 09 15 2026
/* purpose
* Implements the headless hot reconciliation-policy self-test: thresholds
* (ignore/smooth/snap), major snap, and generation-mismatch hard reset. No
* Player, client, or snapshot pointers; the payload is generic.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/reconciliation-policy-selftest.h"

#include <string>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-reconciliation.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

GameReconcileV1 makeBase()
{
    GameReconcileV1 r{};
    r.smallDistance = 0.5f;
    r.mediumDistance = 5.0f;
    r.majorDistance = 100.0f;
    r.predictedGeneration = 7;
    r.authoritativeGeneration = 7;
    r.handled = 0;
    return r;
}

GameReconcileV1 run(GameReconcileV1 r)
{
    LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_RECONCILE, &r, sizeof(r), 0, 0,
                                          0);
    return r;
}

} // namespace

bool runReconciliationPolicySelfTest(std::string& report)
{
    bool ok = true;
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    GameReconcileV1 tiny = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 0.1f;
        return r;
    }());
    ok &= check(tiny.handled == 1u && tiny.correctionMode == 0u &&
                    tiny.shouldCorrect == 0u,
                "tiny error -> no correction", report);

    GameReconcileV1 med = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 2.0f;
        return r;
    }());
    ok &= check(med.handled == 1u && med.correctionMode == 1u &&
                    med.shouldCorrect == 1u,
                "medium error -> smooth correction", report);

    GameReconcileV1 big = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 50.0f;
        return r;
    }());
    ok &= check(big.handled == 1u && big.correctionMode == 2u,
                "large error -> medium correction", report);

    GameReconcileV1 huge = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 500.0f;
        return r;
    }());
    ok &= check(huge.handled == 1u && huge.correctionMode == 3u &&
                    huge.shouldCorrect == 1u,
                "very large error -> snap correction", report);

    GameReconcileV1 gen = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 0.01f;  // would normally be ignored
        r.authoritativeGeneration = 8;
        return r;
    }());
    ok &= check(gen.handled == 1u && gen.hardReset == 1u &&
                    gen.correctionMode == 4u,
                "generation mismatch -> explicit hard reset (not silent)", report);

    GameReconcileV1 d1 = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 2.0f;
        return r;
    }());
    GameReconcileV1 d2 = run([&] {
        GameReconcileV1 r = makeBase();
        r.positionError = 2.0f;
        return r;
    }());
    ok &= check(d1.correctionMode == d2.correctionMode &&
                    d1.shouldCorrect == d2.shouldCorrect,
                "reconciliation classification is deterministic", report);

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
