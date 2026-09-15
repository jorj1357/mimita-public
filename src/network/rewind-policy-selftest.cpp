// 09 15 2026
/* purpose
* Implements the headless hot rewind-policy self-test: rewind target from the
* command tick, latency/interpolation-delay compensation, max-rewind clamp, and
* generation-mismatch rejection. No Player/Npc/weapon pointers; generic payload.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/rewind-policy-selftest.h"

#include <string>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/hot-rewind.h"
#include "live-code/live-behavior.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

GameRewindPolicyV1 makeBase()
{
    GameRewindPolicyV1 p{};
    p.currentTick = 1000;
    p.commandTick = 990;
    p.measuredLatencySeconds = 0.0f;
    p.interpolationDelaySeconds = 0.0f;
    p.compensationSeconds = 0.0f;
    p.maxRewindTicks = 40;
    p.attackerGeneration = 1;
    p.targetGeneration = 1;
    p.currentGeneration = 1;
    p.handled = 0;
    return p;
}

GameRewindPolicyV1 run(GameRewindPolicyV1 p)
{
    LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_REWIND, &p, sizeof(p), 0, 0, 0);
    return p;
}

} // namespace

bool runRewindPolicySelfTest(std::string& report)
{
    bool ok = true;
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    GameRewindPolicyV1 inside = run(makeBase());
    ok &= check(inside.handled == 1u && inside.allow == 1u &&
                    inside.targetTick == 990u,
                "command tick inside history -> rewind to command tick", report);

    GameRewindPolicyV1 lat = run([&] {
        GameRewindPolicyV1 p = makeBase();
        p.commandTick = 0u;
        p.acceptedClientTick = 5u;
        p.acceptedServerTick = 1000u;
        p.measuredLatencySeconds = 0.1f;  // 6 ticks at 60 Hz
        return p;
    }());
    ok &= check(lat.handled == 1u && lat.targetTick == 994u,
                "latency compensation rewinds by ping ticks", report);

    GameRewindPolicyV1 interp = run([&] {
        GameRewindPolicyV1 p = makeBase();
        p.interpolationDelaySeconds = 0.05f;  // 3 ticks
        return p;
    }());
    ok &= check(interp.handled == 1u && interp.targetTick == 987u,
                "interpolation-delay compensation rewinds by interp ticks", report);

    GameRewindPolicyV1 clamp = run([&] {
        GameRewindPolicyV1 p = makeBase();
        p.commandTick = 100u;  // far older than maxRewind (40)
        return p;
    }());
    ok &= check(clamp.handled == 1u && clamp.clamped == 1u &&
                    clamp.targetTick == 960u,
                "max-rewind clamp applies", report);

    GameRewindPolicyV1 gen = run([&] {
        GameRewindPolicyV1 p = makeBase();
        p.targetGeneration = 2;  // mismatch
        return p;
    }());
    ok &= check(gen.handled == 1u && gen.reject == 1u && gen.allow == 0u,
                "generation mismatch -> conservative reject", report);

    GameRewindPolicyV1 d1 = run(makeBase());
    GameRewindPolicyV1 d2 = run(makeBase());
    ok &= check(d1.targetTick == d2.targetTick && d1.clamped == d2.clamped,
                "rewind target is deterministic", report);

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
