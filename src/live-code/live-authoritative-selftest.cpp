// 09 12 2026
/* purpose
* Implements the headless hot authoritative damage self-test.
* Proves the kernel -> hot behavior -> kernel apply plumbing, and that no cold
* gameplay clamp overrides the hot result.
* Does NOT own gameplay policy.
*/
#include "live-code/live-authoritative-selftest.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "live-code/live-journal.h"
#include "network/server-damage-policy.h"

#include <cmath>
#include <fstream>
#include <string>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

ServerDamagePolicyInput makeInput()
{
    ServerDamagePolicyInput input{};
    input.source = GAME_DAMAGE_SOURCE_EXPLOSION;
    input.attackerEntity = 0x111100000001ull;
    input.victimEntity = 0x222200000002ull;
    input.projectileEntity = 0x333300000003ull;
    input.weaponNetworkId = 0;
    input.victimIsNpc = 1;
    input.distance = 1.0f;
    input.tick = 100;
    return input;
}

} // namespace

bool runHotAuthoritativeSelfTest(std::string& report)
{
    bool ok = true;
    HotReloadSystem::instance().startup();
    LiveEventJournal::instance().init();
    ok &= check(HotReloadSystem::instance().loaded(), "GameAPI loaded", report);
    ok &= check(LiveBehavior::available(), "hot behavior event handler present", report);

    // The kernel dispatch must be handled by the hot behavior.
    DamagePolicyV1 probe{};
    probe.baseDamage = 5;
    probe.outDamage = 5;
    probe.source = GAME_DAMAGE_SOURCE_EXPLOSION;
    const bool handled = LiveBehavior::dispatchDamagePolicy(probe, 100);
    ok &= check(handled && probe.handled == 1, "kernel dispatches to hot behavior", report);

    // The kernel apply path must return exactly the hot behavior's result.
    glm::vec3 knockback{0.0f};
    const int resolved = serverResolveDamagePolicy(makeInput(), 5, knockback);
    ok &= check(resolved == probe.outDamage,
                "kernel applies the hot policy result", report);

    // No cold gameplay clamp may restore a small constant (the old 150/500).
    glm::vec3 bigKnockback{0.0f};
    const int largeResolved = serverResolveDamagePolicy(makeInput(), 999999, bigKnockback);
    ok &= check(largeResolved > 500,
                "large hot damage is not clamped to a gameplay constant", report);
    ok &= check(serverAuthoritativeDamageLimit() == 0,
                "dev-branch authoritative damage limit is unlimited", report);

    // Structured evidence must exist for the authoritative policy chain.
    {
        std::ifstream journal(LiveEventJournal::instance().path());
        std::string line;
        std::string content;
        while (std::getline(journal, line))
            content += line;
        ok &= check(content.find("\"type\":\"hot_damage_policy_result\"") != std::string::npos,
                    "authoritative policy journal evidence written", report);
    }

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
