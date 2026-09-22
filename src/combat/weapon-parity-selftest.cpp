// 09 22 2026
/* purpose
* Implements the v2.0.6 weapon parity harness seed.
* See weapon-parity-selftest.h for scope.
*/
#include "combat/weapon-parity-selftest.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "combat/hitscan-model.h"
#include "combat/pellet-pattern.h"
#include "combat/weapon-data.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "network/network-weapons.h"
#include "network/server-weapon-tuning.h"

namespace {

// Frozen v2.0.6 reference (docs/gold/2026-09-21-v206-weapon-parity-method.md).
struct ReferenceWeapon {
    const char* id;
    float damage;
    float fireDelay;
    float reloadTime;
    int magazineSize;
    int pelletCount;
    float spread;
};

const std::vector<ReferenceWeapon>& references()
{
    static const std::vector<ReferenceWeapon> refs = {
        {"revolver", 50.0f, 0.08f, 1.0f, 6, 1, 0.0f},
        {"shotgun", 12.0f, 0.25f, 1.5f, 2, 15, 3.0f},
    };
    return refs;
}

WeaponDefinition makeDefinition(float damage, float headshotMultiplier,
                                int pelletCount, float spread)
{
    WeaponDefinition def;
    def.damage = damage;
    def.headshotMultiplier = headshotMultiplier;
    def.pelletCount = pelletCount;
    def.spread = spread;
    return def;
}

bool checkInt(std::string& report, const char* what, int got, int expected)
{
    if (got == expected)
        return true;
    char line[160];
    std::snprintf(line, sizeof(line), "[FAIL] %s got=%d expected=%d\n", what, got,
                  expected);
    report += line;
    return false;
}

} // namespace

bool runWeaponParitySelfTest(std::string& report)
{
    report.clear();
    bool ok = true;

    // 1. Reference numbers are recorded and non-degenerate.
    for (const ReferenceWeapon& ref : references())
    {
        if (ref.damage <= 0.0f || ref.fireDelay <= 0.0f || ref.magazineSize <= 0)
        {
            report += "[FAIL] invalid frozen reference entry\n";
            ok = false;
        }
    }

    // 2. Revolver: no falloff, torso = base, head = x2, leg = x limb.
    {
        WeaponDefinition def = makeDefinition(50.0f, 2.0f, 1, 0.0f);
        ok &= checkInt(report, "revolver torso@10m",
                       WeaponExecution::computeHitscanDamage(def, "torso", 10.0f, 1.0f),
                       50);
        ok &= checkInt(report, "revolver head@10m",
                       WeaponExecution::computeHitscanDamage(def, "head", 10.0f, 1.0f),
                       100);
        // 50 * 0.75 = 37.5 -> 38 (lrint rounds half away from zero)
        ok &= checkInt(report, "revolver leg@10m",
                       WeaponExecution::computeHitscanDamage(def, "leg", 10.0f, 1.0f),
                       38);
    }

    // 3. Shotgun: falloff start 30, min fraction 0.02, exponent 2.
    {
        WeaponDefinition def = makeDefinition(12.0f, 2.0f, 15, 10.0f);
        def.customParams["distanceFalloffStart"] = 30.0f;
        def.customParams["minDamageFraction"] = 0.02f;
        def.customParams["falloffExponent"] = 2.0f;
        def.customParams["limbDamageMultiplier"] = 0.75f;

        HitscanDamageParams p;
        p.baseDamage = 12.0f;
        p.headshotMultiplier = 2.0f;
        p.limbDamageMultiplier = 0.75f;
        p.distanceFalloffStart = 30.0f;
        p.minDamageFraction = 0.02f;
        p.falloffExponent = 2.0f;

        // Cold wrapper and the shared model must agree at every sample: this is
        // the one-owner guarantee (cold adopts the shared model).
        const float distances[] = {0.0f, 5.0f, 15.0f, 30.0f, 45.0f, 90.0f};
        for (float distance : distances)
        {
            const int coldTorso = WeaponExecution::computeHitscanDamage(
                def, "torso", distance, 1.0f);
            const int sharedTorso = computeHitscanDamage(p, false, false, distance);
            ok &= checkInt(report, "shotgun cold==shared torso", coldTorso, sharedTorso);

            const int coldHead = WeaponExecution::computeHitscanDamage(
                def, "head", distance, 1.0f);
            const int sharedHead = computeHitscanDamage(p, true, false, distance);
            ok &= checkInt(report, "shotgun cold==shared head", coldHead, sharedHead);
        }

        // Damage must monotonically fall with distance (falloff actually runs).
        int previous = computeHitscanDamage(p, false, false, 0.0f);
        for (float distance : {5.0f, 15.0f, 30.0f, 45.0f, 90.0f})
        {
            const int current = computeHitscanDamage(p, false, false, distance);
            if (current > previous)
            {
                report += "[FAIL] shotgun damage increased with distance\n";
                ok = false;
            }
            previous = current;
        }
    }

    // 4. Frozen pellet grid: shotgun = 15 deterministic pellets, identical
    //    across calls and independent of any seed (the spread owner is seedless).
    {
        glm::vec3 dirsA[MAX_PELLETS_PER_BLAST]{};
        glm::vec3 dirsB[MAX_PELLETS_PER_BLAST]{};
        const glm::vec3 aim = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
        const int countA =
            buildFixedPelletDirections(aim, 15, 10.0f, dirsA, MAX_PELLETS_PER_BLAST);
        const int countB =
            buildFixedPelletDirections(aim, 15, 10.0f, dirsB, MAX_PELLETS_PER_BLAST);
        ok &= checkInt(report, "shotgun pellet count", countA, 15);
        ok &= checkInt(report, "shotgun pellet count (repeat)", countB, 15);
        for (int i = 0; i < countA; ++i)
        {
            if (glm::length(dirsA[i] - dirsB[i]) > 1e-6f)
            {
                report += "[FAIL] pellet grid not deterministic\n";
                ok = false;
                break;
            }
        }
    }

    // 5. behaviorSource parity: the hot `weapon.tuning` lookup must mirror the
    //    registry (which is JSON-authoritative), so editing weapons.json
    //    hot-retunes every hot tool.
    {
        WeaponData::registerBuiltinWeapons();
        for (const ReferenceWeapon& ref : references())
        {
            const std::uint32_t networkId = MimitaNet::weaponDefNetworkIdFor(ref.id);
            const WeaponDefinition* def = WeaponRegistry::instance().get(ref.id);
            GameWeaponTuningV1 tuning{};
            const bool found = networkId != 0 && def != nullptr &&
                               MimitaNet::serverWeaponTuning(networkId, &tuning);
            if (!found || tuning.found == 0)
            {
                char line[128];
                std::snprintf(line, sizeof(line),
                              "[FAIL] weapon.tuning not found for %s\n", ref.id);
                report += line;
                ok = false;
                continue;
            }
            if (std::fabs(tuning.damage - def->damage) > 1e-4f ||
                std::fabs(tuning.fireDelay - def->fireDelay) > 1e-4f ||
                std::fabs(tuning.reloadTime - def->reloadTime) > 1e-4f ||
                tuning.magazineSize != def->magazineSize ||
                tuning.pelletCount != def->pelletCount ||
                std::fabs(tuning.spread - def->spread) > 1e-4f)
            {
                char line[160];
                std::snprintf(line, sizeof(line),
                              "[FAIL] weapon.tuning diverges from registry for %s\n",
                              ref.id);
                report += line;
                ok = false;
            }
        }
    }

    if (ok)
        report += "[OK] v2.0.6 weapon reference frozen; damage + grid parity holds\n";
    return ok;
}
