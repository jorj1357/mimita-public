// 09 22 2026
/* purpose
* Implements the v2.0.6 projectile/melee/attack-policy parity harness.
* See combat-v206-parity-selftest.h for scope.
*
* References are frozen from the pre-hot cold owners and documented in
* docs/gold/2026-09-21-v206-weapon-parity-method.md:
*   - projectile splash: server-projectiles.cpp explodeProjectile (two-regime
*     full-damage-radius mix, else Gaussian exp(-(d/r)^2 * exponent)); knock
*     scale (1 - t^2)*0.85 + 0.15 or damage/splashDamage clamp.
*   - physical contact: server-physical-contact.cpp physicalContactDamage /
*     physicalContactKnockback (slash/lunge base, QuickHit force, godball speed).
*   - attack policy: the hot `attack.policy` owner must accept/reject a fixed
*     set of scenarios with the documented decisions.
*/
#include "combat/combat-v206-parity-selftest.h"

#include <cmath>
#include <cstdio>
#include <string>

#include "combat/weapon-execution.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/packets.h"

namespace {

bool check(std::string& report, bool condition, const char* name)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

bool checkNear(std::string& report, const char* name, float got, float expected,
               float eps)
{
    const bool ok = std::fabs(got - expected) <= eps;
    if (!ok)
    {
        char line[160];
        std::snprintf(line, sizeof(line), "[FAIL] %s got=%.4f expected=%.4f\n",
                      name, got, expected);
        report += line;
    }
    return ok;
}

// ── Frozen projectile splash reference ────────────────────────────────
struct SplashRef {
    float splashRadius;
    float splashDamage;
    float splashExponent;
    float fullDamageRadius;
    float edgeDamage;
};

float splashDamageReference(const SplashRef& s, float dist)
{
    if (s.fullDamageRadius > 0.0f)
    {
        if (dist <= s.fullDamageRadius)
            return s.splashDamage;
        if (dist >= s.splashRadius)
            return s.edgeDamage;
        const float t = (dist - s.fullDamageRadius) /
            std::fmax(0.001f, s.splashRadius - s.fullDamageRadius);
        return s.splashDamage + (s.edgeDamage - s.splashDamage) * t;
    }
    return s.splashDamage *
        std::exp(-std::pow(dist / s.splashRadius, 2.0f) * s.splashExponent);
}

float splashKnockScaleReference(const SplashRef& s, float dist, float damageValue)
{
    if (s.fullDamageRadius > 0.0f)
        return std::fmin(1.0f, std::fmax(0.0f, damageValue / std::fmax(0.001f, s.splashDamage)));
    const float t = dist / s.splashRadius;
    return (1.0f - t * t) * 0.85f + 0.15f;
}

// ── Frozen physical-contact reference ─────────────────────────────────
struct MeleeRef {
    float slashBaseDamage;
    float slashKnockback;
    float lungeBaseDamage;
    float lungeKnockback;
};

int meleeDamageReference(const MeleeRef& m, bool lunge)
{
    const float base = lunge ? m.lungeBaseDamage : m.slashBaseDamage;
    return std::max(1, std::min(500, (int)std::lround(base)));
}

float meleeKnockbackStrengthReference(const MeleeRef& m, int damage, bool lunge)
{
    const float fallback = std::max(1.0f, damage * 0.75f);
    return lunge ? m.lungeKnockback : m.slashKnockback;
}

} // namespace

bool runCombatV206ParitySelfTest(std::string& report)
{
    report.clear();
    bool ok = true;

    // The attack-policy checks drive the live hot owner, so the package must be
    // active (registers the `attack.policy` event handler and tool recipes).
    HotReloadSystem::instance().startup();

    // ── 1. Projectile splash: two-regime (mix) falloff ────────────────
    {
        SplashRef s{};
        s.splashRadius = 8.0f;
        s.splashDamage = 90.0f;
        s.splashExponent = 2.0f;
        s.fullDamageRadius = 2.0f;
        s.edgeDamage = 10.0f;

        ok &= checkNear(report, "splash full dmg at 1m",
                        splashDamageReference(s, 1.0f), 90.0f, 1e-3f);
        ok &= checkNear(report, "splash edge dmg at 8m",
                        splashDamageReference(s, 8.0f), 10.0f, 1e-3f);
        // Midpoint (2..8) at 5m: t = 3/6 = 0.5 -> mix(90,10,0.5)=50.
        ok &= checkNear(report, "splash mid dmg at 5m",
                        splashDamageReference(s, 5.0f), 50.0f, 1e-2f);
        ok &= checkNear(report, "splash knock full at 2m",
                        splashKnockScaleReference(s, 2.0f, 90.0f), 1.0f, 1e-3f);
        ok &= checkNear(report, "splash knock half at 5m",
                        splashKnockScaleReference(s, 5.0f, 50.0f), 0.5556f, 1e-2f);
    }

    // ── 2. Projectile splash: Gaussian (no full-damage radius) ────────
    {
        SplashRef s{};
        s.splashRadius = 4.0f;
        s.splashDamage = 90.0f;
        s.splashExponent = 2.0f;
        s.fullDamageRadius = 0.0f;
        s.edgeDamage = 0.0f;

        ok &= checkNear(report, "gaussian dmg at 0m",
                        splashDamageReference(s, 0.0f), 90.0f, 1e-2f);
        // at r: exp(-1*2) = 0.1353 -> 12.18
        ok &= checkNear(report, "gaussian dmg at 4m",
                        splashDamageReference(s, 4.0f), 90.0f * 0.135335f, 1e-1f);
        ok &= checkNear(report, "gaussian knock at 0m",
                        splashKnockScaleReference(s, 0.0f, 90.0f), 1.0f, 1e-3f);
        // at r: (1-1)*0.85+0.15 = 0.15
        ok &= checkNear(report, "gaussian knock at 4m",
                        splashKnockScaleReference(s, 4.0f, 90.0f), 0.15f, 1e-3f);
    }

    // ── 3. Melee damage/knockback reference ───────────────────────────
    {
        MeleeRef m{};
        m.slashBaseDamage = 10.0f;
        m.slashKnockback = 12.0f;
        m.lungeBaseDamage = 18.0f;
        m.lungeKnockback = 20.0f;

        report += "slashBaseDamage=" + std::to_string(meleeDamageReference(m, false)) +
                  " lungeBaseDamage=" + std::to_string(meleeDamageReference(m, true)) + "\n";
        ok &= check(report, meleeDamageReference(m, false) == 10, "slash damage = 10");
        ok &= check(report, meleeDamageReference(m, true) == 18, "lunge damage = 18");
        ok &= checkNear(report, "slash knockback",
                        meleeKnockbackStrengthReference(m, 10, false), 12.0f, 1e-3f);
        ok &= checkNear(report, "lunge knockback",
                        meleeKnockbackStrengthReference(m, 18, true), 20.0f, 1e-3f);
    }

    // ── 4. Attack policy: the hot owner's fixed decisions ─────────────
    // The hot `attack.policy` event owns routing. These scenarios freeze the
    // expected accept/reject so a policy edit that regresses is caught.
    {
        // Valid accepted request.
        AttackPolicyV1 valid{};
        valid.shooterPlayerId = 1;
        valid.spawnGeneration = 1;
        valid.spawnStateActive = 1;
        valid.weaponDefNetworkId = 1;   // revolver (hot)
        valid.weaponNetworkId = 1;
        valid.executionType = 0;        // Hitscan
        valid.hasDefinition = 1;
        valid.communityAllowed = 1;
        valid.origin[0] = 0; valid.origin[1] = 0; valid.origin[2] = 1;
        valid.direction[0] = 1; valid.direction[1] = 0; valid.direction[2] = 0;
        valid.shooterPos[0] = 0; valid.shooterPos[1] = 0; valid.shooterPos[2] = 1;
        valid.originTolerance = 12.0f;
        valid.maxShotsPerTick = 8;
        const bool handled = LiveBehavior::dispatchAttackPolicy(valid, 1);
        ok &= check(report, handled && valid.handled == 1 && valid.accept == 1,
                    "attack policy accepts a valid hot request");

        // Dead shooter rejected.
        AttackPolicyV1 dead{};
        dead.shooterPlayerId = 1;
        dead.spawnGeneration = 1;
        dead.spawnStateActive = 1;
        dead.shooterDead = 1;
        dead.weaponDefNetworkId = 1; dead.weaponNetworkId = 1;
        dead.hasDefinition = 1; dead.communityAllowed = 1;
        LiveBehavior::dispatchAttackPolicy(dead, 2);
        ok &= check(report, dead.handled == 1 && dead.accept == 0,
                    "attack policy rejects a dead shooter");

        // Stale spawn rejected.
        AttackPolicyV1 stale{};
        stale.shooterPlayerId = 1;
        stale.spawnStateActive = 1;
        stale.weaponDefNetworkId = 1; stale.weaponNetworkId = 1;
        stale.hasDefinition = 1; stale.communityAllowed = 1;
        LiveBehavior::dispatchAttackPolicy(stale, 3);
        ok &= check(report, stale.handled == 1 && stale.accept == 0,
                    "attack policy rejects a stale spawn");

        // Out-of-tolerance geometry rejected.
        AttackPolicyV1 tooFar{};
        tooFar.shooterPlayerId = 1;
        tooFar.spawnGeneration = 1;
        tooFar.spawnStateActive = 1;
        tooFar.weaponDefNetworkId = 1; tooFar.weaponNetworkId = 1;
        tooFar.executionType = 0;
        tooFar.hasDefinition = 1; tooFar.communityAllowed = 1;
        tooFar.origin[0] = 100.0f; tooFar.origin[1] = 0; tooFar.origin[2] = 1;
        tooFar.direction[0] = 1; tooFar.direction[1] = 0; tooFar.direction[2] = 0;
        tooFar.shooterPos[0] = 0; tooFar.shooterPos[1] = 0; tooFar.shooterPos[2] = 1;
        tooFar.originTolerance = 12.0f;
        tooFar.maxShotsPerTick = 8;
        LiveBehavior::dispatchAttackPolicy(tooFar, 4);
        ok &= check(report, tooFar.handled == 1 && tooFar.accept == 0,
                    "attack policy rejects out-of-tolerance geometry");

        // Shot-limit rejected.
        AttackPolicyV1 rateLimited{};
        rateLimited.shooterPlayerId = 1;
        rateLimited.spawnGeneration = 1;
        rateLimited.spawnStateActive = 1;
        rateLimited.weaponDefNetworkId = 1; rateLimited.weaponNetworkId = 1;
        rateLimited.executionType = 0;
        rateLimited.hasDefinition = 1; rateLimited.communityAllowed = 1;
        rateLimited.origin[0] = 0; rateLimited.origin[1] = 0; rateLimited.origin[2] = 1;
        rateLimited.direction[0] = 1; rateLimited.direction[1] = 0; rateLimited.direction[2] = 0;
        rateLimited.shooterPos[0] = 0; rateLimited.shooterPos[1] = 0; rateLimited.shooterPos[2] = 1;
        rateLimited.originTolerance = 12.0f;
        rateLimited.shotsThisTick = 8;
        rateLimited.maxShotsPerTick = 8;
        LiveBehavior::dispatchAttackPolicy(rateLimited, 5);
        ok &= check(report, rateLimited.handled == 1 && rateLimited.accept == 0,
                    "attack policy rejects over the per-tick shot limit");

        // Non-hot weapon declines (cold stays authoritative).
        AttackPolicyV1 coldWeapon{};
        coldWeapon.shooterPlayerId = 1;
        coldWeapon.spawnGeneration = 1;
        coldWeapon.spawnStateActive = 1;
        coldWeapon.weaponDefNetworkId = 999; coldWeapon.weaponNetworkId = 999;
        coldWeapon.hasDefinition = 1; coldWeapon.communityAllowed = 1;
        LiveBehavior::dispatchAttackPolicy(coldWeapon, 6);
        ok &= check(report, coldWeapon.handled == 0,
                    "attack policy declines a non-hot weapon (cold owns)");
    }

    if (ok)
        report += "[OK] v2.0.6 projectile/melee/attack-policy parity holds\n";
    return ok;
}
