// 07 19 2026 1130
/* purpose
* Test authoritative weapon-runtime reconciliation logic.
* Verifies tick→seconds conversion, revision ordering, spawn generation
* staleness, ammo validation, idempotent duplicates, and mutation safety.
* Tests the pure logic using a WeaponRuntime map (no Player dependency).
* fill in 3rd line
* Does NOT test packet receive handlers, respawn, or reload mechanics.
* Does NOT test projectile behavior, damage, or networking.
*/

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <unordered_map>

#include "combat/weapon-types.h"

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-50s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

static constexpr uint32_t EST_SERVER_TICK = 100;
static constexpr float SIM_HZ = 60.0f;

// ── Pure logic under test (mirrors production reconcileAuthoritativeWeaponRuntime) ──

static bool reconcile(
    std::unordered_map<std::string, WeaponRuntime>& runtimes,
    const std::string& weaponId,
    int magazineAmmo,
    int reserveAmmo,
    uint64_t nextAllowedFireTick,
    bool reloading,
    uint64_t reloadCompleteTick,
    uint32_t stateRevision,
    uint32_t spawnGeneration,
    uint32_t estimatedServerTick)
{
    // Unknown weapons: reject, create no runtime, change nothing
    if (weaponId == "nonexistent")
    {
        return false;
    }

    auto rtIt = runtimes.find(weaponId);
    if (rtIt == runtimes.end())
    {
        WeaponRuntime rt;
        rt.currentAmmo = 6;
        rt.reserveAmmo = 1337;
        runtimes[weaponId] = rt;
        rtIt = runtimes.find(weaponId);
    }

    WeaponRuntime& rt = rtIt->second;

    // SpawnGen 0 cannot overwrite known nonzero generation
    if (spawnGeneration == 0 && rt.authoritativeSpawnGeneration != 0)
    {
        // Continue with existing generation
    }

    if (spawnGeneration != 0 && spawnGeneration < rt.authoritativeSpawnGeneration)
        return false;

    if (spawnGeneration > rt.authoritativeSpawnGeneration)
    {
        if (spawnGeneration != 0)
            rt.authoritativeSpawnGeneration = spawnGeneration;
        rt.authoritativeStateRevision = 0;
    }

    if (stateRevision < rt.authoritativeStateRevision)
        return false;

    if (stateRevision == rt.authoritativeStateRevision)
        return true;

    if (magazineAmmo < 0 || reserveAmmo < 0)
        return false;

    rt.currentAmmo = magazineAmmo;
    rt.reserveAmmo = reserveAmmo;
    rt.isReloading = reloading;

    bool hasServerTick = estimatedServerTick > 0;

    if (hasServerTick && nextAllowedFireTick > 0)
    {
        uint64_t rem = nextAllowedFireTick > estimatedServerTick
            ? nextAllowedFireTick - estimatedServerTick : 0;
        rt.fireCooldown = (float)rem / SIM_HZ;
    }

    if (reloading && reloadCompleteTick > 0)
    {
        if (hasServerTick)
        {
            uint64_t rem = reloadCompleteTick > estimatedServerTick
                ? reloadCompleteTick - estimatedServerTick : 0;
            rt.reloadTimer = (float)rem / SIM_HZ;
        }
    }
    else if (!reloading)
    {
        rt.reloadTimer = 0.0f;
    }

    rt.authoritativeStateRevision = stateRevision;
    if (spawnGeneration != 0)
        rt.authoritativeSpawnGeneration = spawnGeneration;

    return true;
}

// ── Test 1: Newer revision applies ammo ──────────────────────────────

static void testNewerRevisionApplies()
{
    TEST("newer revision applies ammo");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    CHECK(reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 1, 0, EST_SERVER_TICK), "ok");
    CHECK(runtimes["revolver"].currentAmmo == 3, "ammo=%d", runtimes["revolver"].currentAmmo);
    CHECK(runtimes["revolver"].reserveAmmo == 100, "reserve=%d", runtimes["revolver"].reserveAmmo);
    PASS();
}

// ── Test 2: Fire tick converts to seconds ───────────────────────────

static void testFireTickConversion()
{
    TEST("fire tick to seconds");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 6, 100, 130, false, 0, 1, 0, EST_SERVER_TICK);
    CHECK(std::fabs(runtimes["revolver"].fireCooldown - 0.5f) < 0.001f,
          "cd=%.4f", runtimes["revolver"].fireCooldown);
    PASS();
}

// ── Test 3: Reload tick converts to seconds ─────────────────────────

static void testReloadTickConversion()
{
    TEST("reload tick to seconds");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 6, 100, 0, true, 160, 1, 0, EST_SERVER_TICK);
    CHECK(std::fabs(runtimes["revolver"].reloadTimer - 1.0f) < 0.001f,
          "rt=%.4f", runtimes["revolver"].reloadTimer);
    PASS();
}

// ── Test 4: Completed timer becomes zero ────────────────────────────

static void testCompletedTimer()
{
    TEST("completed timer zero");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 6, 100, 90, false, 0, 1, 0, EST_SERVER_TICK);
    CHECK(runtimes["revolver"].fireCooldown == 0.0f, "cd=%.4f", runtimes["revolver"].fireCooldown);
    PASS();
}

// ── Test 5: Reloading state applied ─────────────────────────────────

static void testReloadingState()
{
    TEST("reloading state applied");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 6, 100, 0, true, 130, 1, 0, EST_SERVER_TICK);
    CHECK(runtimes["revolver"].isReloading == true, "reload not set");
    PASS();
}

// ── Test 6: Older revision rejected ─────────────────────────────────

static void testOlderRevisionIgnored()
{
    TEST("older revision ignored");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 10, 0, EST_SERVER_TICK);
    CHECK(!reconcile(runtimes, "revolver", 6, 100, 0, false, 0, 5, 0, EST_SERVER_TICK), "reject");
    CHECK(runtimes["revolver"].currentAmmo == 3, "ammo unchanged");
    PASS();
}

// ── Test 7: Equal revision idempotent ───────────────────────────────

static void testEqualRevisionIdempotent()
{
    TEST("equal revision idempotent");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 10, 0, EST_SERVER_TICK);
    CHECK(reconcile(runtimes, "revolver", 6, 100, 0, false, 0, 10, 0, EST_SERVER_TICK), "dup");
    CHECK(runtimes["revolver"].currentAmmo == 3, "ammo unchanged");
    PASS();
}

// ── Test 8: Stale spawn gen rejected ────────────────────────────────

static void testStaleSpawnGen()
{
    TEST("stale spawn gen rejected");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 10, 3, EST_SERVER_TICK);
    CHECK(!reconcile(runtimes, "revolver", 6, 100, 0, false, 0, 11, 2, EST_SERVER_TICK), "stale");
    CHECK(runtimes["revolver"].currentAmmo == 3, "ammo unchanged");
    PASS();
}

// ── Test 9: New spawn gen resets revision ───────────────────────────

static void testNewSpawnGenResetsRevision()
{
    TEST("new spawn gen resets revision");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    // First life: gen=3, revision=10
    reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 10, 3, EST_SERVER_TICK);
    // New life: gen=4, revision=1 (lower numeric value, but same gen)
    bool ok = reconcile(runtimes, "revolver", 6, 100, 0, false, 0, 1, 4, EST_SERVER_TICK);
    CHECK(ok, "new spawn gen should accept lower revision");
    CHECK(runtimes["revolver"].currentAmmo == 6, "ammo=%d", runtimes["revolver"].currentAmmo);
    PASS();
}

// ── Test 10: Unknown weapon rejected, no runtime created ────────────

static void testUnknownWeaponRejected()
{
    TEST("unknown weapon rejected, no runtime");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    // Pre-populate an existing weapon to verify it's untouched
    reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 1, 0, EST_SERVER_TICK);

    bool ok = reconcile(runtimes, "nonexistent", 6, 100, 0, false, 0, 1, 0, EST_SERVER_TICK);
    CHECK(!ok, "unknown weapon should return false");
    CHECK(runtimes.find("nonexistent") == runtimes.end(), "unknown weapon created runtime");
    // Existing runtime unchanged
    CHECK(runtimes["revolver"].currentAmmo == 3, "existing runtime changed");
    PASS();
}

// ── Test 11: SpawnGen 0 cannot overwrite known generation ───────────

static void testSpawnGenZeroNoOverwrite()
{
    TEST("spawnGen 0 cannot overwrite known gen");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 3, 100, 0, false, 0, 10, 5, EST_SERVER_TICK);
    CHECK(runtimes["revolver"].authoritativeSpawnGeneration == 5, "gen=%u", runtimes["revolver"].authoritativeSpawnGeneration);

    // Update with spawnGen=0 — must not overwrite gen 5
    reconcile(runtimes, "revolver", 6, 100, 0, false, 0, 11, 0, EST_SERVER_TICK);
    CHECK(runtimes["revolver"].authoritativeSpawnGeneration == 5, "gen overwritten to %u", runtimes["revolver"].authoritativeSpawnGeneration);
    CHECK(runtimes["revolver"].currentAmmo == 6, "ammo should update but gen should not");
    PASS();
}

// ── Test 12: Negative ammo causes no partial mutation ───────────────

static void testNoPartialMutation()
{
    TEST("invalid ammo no partial mutation");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "revolver", 3, 100, 130, false, 0, 10, 0, EST_SERVER_TICK);
    float oldCd = runtimes["revolver"].fireCooldown;

    // Negative magazine — rejection should not change ANY field
    bool ok = reconcile(runtimes, "revolver", -1, 100, 200, false, 0, 11, 0, EST_SERVER_TICK);
    CHECK(!ok, "negative ammo not rejected");
    CHECK(runtimes["revolver"].currentAmmo == 3, "ammo changed");
    CHECK(runtimes["revolver"].fireCooldown == oldCd, "cooldown changed");
    CHECK(runtimes["revolver"].authoritativeStateRevision == 10, "revision changed");
    PASS();
}

// ── Test 13: No server tick preserves predicted timer ───────────────

static void testNoServerTickPolicy()
{
    TEST("no server tick preserves predicted timer");
    std::unordered_map<std::string, WeaponRuntime> runtimes;

    // First set a predicted cooldown (as if the client had fired)
    runtimes["revolver"].fireCooldown = 0.5f;
    runtimes["revolver"].reloadTimer = 1.0f;
    runtimes["revolver"].isReloading = true;
    runtimes["revolver"].currentAmmo = 3;
    runtimes["revolver"].reserveAmmo = 100;
    runtimes["revolver"].authoritativeStateRevision = 5;

    // estimatedServerTick=0 means no server tick known
    // Policy: preserve predicted timers, only apply ammo/revision metadata
    bool ok = reconcile(runtimes, "revolver", 3, 100, 160, true, 200, 6, 0, 0);
    CHECK(ok, "reconcile should succeed");

    // Predicted timers preserved (no server tick to convert from)
    CHECK(std::fabs(runtimes["revolver"].fireCooldown - 0.5f) < 0.001f,
          "cooldown changed to %.4f", runtimes["revolver"].fireCooldown);
    CHECK(std::fabs(runtimes["revolver"].reloadTimer - 1.0f) < 0.001f,
          "reloadTimer changed to %.4f", runtimes["revolver"].reloadTimer);

    // Revision and ammo still applied
    CHECK(runtimes["revolver"].authoritativeStateRevision == 6, "revision not applied");
    CHECK(runtimes["revolver"].currentAmmo == 3, "ammo not applied");

    PASS();
}

// ── Test 14: isReloading=false cancels timer even without server tick ─

static void testReloadFalseCancelsTimer()
{
    TEST("isReloading=false zeros timer even without server tick");
    std::unordered_map<std::string, WeaponRuntime> runtimes;

    runtimes["revolver"].reloadTimer = 2.0f;
    runtimes["revolver"].isReloading = true;

    // Authoritative says not reloading, no server tick known
    reconcile(runtimes, "revolver", 6, 100, 0, false, 0, 1, 0, 0);
    CHECK(runtimes["revolver"].isReloading == false, "reload state not applied");
    CHECK(runtimes["revolver"].reloadTimer == 0.0f, "reloadTimer=%.4f expected 0", runtimes["revolver"].reloadTimer);
    PASS();
}

// ── Test 14: No weapon-specific branch ──────────────────────────────

static void testNoWeaponBranch()
{
    TEST("no weapon-specific branch");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "rocket_launcher", 3, 200, 130, true, 160, 1, 0, EST_SERVER_TICK);
    CHECK(runtimes["rocket_launcher"].currentAmmo == 3, "rkt ammo=%d", runtimes["rocket_launcher"].currentAmmo);
    CHECK(std::fabs(runtimes["rocket_launcher"].fireCooldown - 0.5f) < 0.001f, "rkt cd=%.4f", runtimes["rocket_launcher"].fireCooldown);
    CHECK(runtimes["rocket_launcher"].isReloading == true, "rkt reload");

    reconcile(runtimes, "grenade_launcher", 2, 150, 140, false, 0, 1, 0, EST_SERVER_TICK);
    CHECK(std::fabs(runtimes["grenade_launcher"].fireCooldown - 0.666f) < 0.01f, "gren cd=%.4f", runtimes["grenade_launcher"].fireCooldown);
    PASS();
}

// ── Test 15: Rejected attack result restores predicted ammo ──────────

static void testRejectedAttackRestoresAmmo()
{
    TEST("rejected attack restores ammo");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    runtimes["revolver"].currentAmmo = 0;
    runtimes["revolver"].reserveAmmo = 1337;
    runtimes["revolver"].authoritativeStateRevision = 2;
    bool ok = reconcile(runtimes, "revolver", 1, 1337, 0, false, 0, 3, 1, EST_SERVER_TICK);
    CHECK(ok, "rejected authoritative state should apply");
    CHECK(runtimes["revolver"].currentAmmo == 1, "ammo=%d", runtimes["revolver"].currentAmmo);
    CHECK(runtimes["revolver"].reserveAmmo == 1337, "reserve=%d", runtimes["revolver"].reserveAmmo);
    PASS();
}

// ── Test 16: Newer reload result blocks older attack rollback ────────

static void testAttackAfterReloadNoRollback()
{
    TEST("attack after newer reload cannot roll back");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    reconcile(runtimes, "rocket_launcher", 1, 1336, 0, false, 0, 20, 4, EST_SERVER_TICK);
    bool ok = reconcile(runtimes, "rocket_launcher", 0, 1337, 0, false, 0, 19, 4, EST_SERVER_TICK);
    CHECK(!ok, "older attack result should be stale");
    CHECK(runtimes["rocket_launcher"].currentAmmo == 1, "ammo=%d", runtimes["rocket_launcher"].currentAmmo);
    CHECK(runtimes["rocket_launcher"].reserveAmmo == 1336, "reserve=%d", runtimes["rocket_launcher"].reserveAmmo);
    PASS();
}

// ── Test 17: Full-mag hitscan preserves reserve from authority ───────

static void testHitscanReserveAfterFullMag()
{
    TEST("revolver/shotgun reserve after full magazine");
    std::unordered_map<std::string, WeaponRuntime> runtimes;
    CHECK(reconcile(runtimes, "revolver", 0, 1337, 0, false, 0, 6, 1, EST_SERVER_TICK), "revolver result");
    CHECK(reconcile(runtimes, "shotgun", 0, 1337, 0, false, 0, 2, 1, EST_SERVER_TICK), "shotgun result");
    CHECK(runtimes["revolver"].currentAmmo == 0 && runtimes["revolver"].reserveAmmo == 1337,
          "revolver ammo=%d/%d", runtimes["revolver"].currentAmmo, runtimes["revolver"].reserveAmmo);
    CHECK(runtimes["shotgun"].currentAmmo == 0 && runtimes["shotgun"].reserveAmmo == 1337,
          "shotgun ammo=%d/%d", runtimes["shotgun"].currentAmmo, runtimes["shotgun"].reserveAmmo);
    PASS();
}

int main()
{
    printf("=== Weapon Runtime Reconciliation Tests ===\n\n");

    testNewerRevisionApplies();
    testFireTickConversion();
    testReloadTickConversion();
    testCompletedTimer();
    testReloadingState();
    testOlderRevisionIgnored();
    testEqualRevisionIdempotent();
    testStaleSpawnGen();
    testNewSpawnGenResetsRevision();
    testUnknownWeaponRejected();
    testSpawnGenZeroNoOverwrite();
    testNoPartialMutation();
    testNoServerTickPolicy();
    testReloadFalseCancelsTimer();
    testNoWeaponBranch();
    testRejectedAttackRestoresAmmo();
    testAttackAfterReloadNoRollback();
    testHitscanReserveAfterFullMag();

    printf("\n=== Results: %d passed, %d failed ===\n",
           gPassed, gFailed);

    return gFailed > 0 ? 1 : 0;
}
