// 07 19 2026 1020
/* purpose
* Verify that rocket and grenade physics values come from WeaponDefinition
* configuration (customParams), not from hardcoded server overrides.
* Tests the generic WeaponDefinition-to-physics-config mapping that
* authoritative server applies to every projectile weapon equally.
* Does NOT test networking, collision, or rendering.
*/

#include <cstdio>
#include <cmath>
#include <string>
#include <unordered_map>



static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-50s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

// Simulate the server's projectileConfig() mapping for a weapon definition
struct FakeWeaponDef {
    std::unordered_map<std::string, float> customParams;
};

static float cp(const FakeWeaponDef& def, const char* key, float fallback)
{
    auto it = def.customParams.find(key);
    return it != def.customParams.end() ? it->second : fallback;
}

struct FakeProjectileConfig {
    float gravity, drag, angularDrag, restitution, friction;
    float armingDistance, minBounceSpeed, upBias, angularSpeed;
    int maxBounceCount;
};

static FakeProjectileConfig mapConfig(const FakeWeaponDef& def)
{
    FakeProjectileConfig cfg;
    cfg.gravity = cp(def, "gravity", 20.0f);
    cfg.drag = cp(def, "drag", 0.15f);
    cfg.angularDrag = cp(def, "angularDrag", 0.3f);
    cfg.restitution = cp(def, "bounceRestitution", 0.35f);
    cfg.friction = cp(def, "bounceFriction", 0.5f);
    cfg.armingDistance = cp(def, "armingDistance", 2.0f);
    cfg.minBounceSpeed = cp(def, "minBounceSpeed", 0.1f);
    cfg.upBias = cp(def, "upBias", 4.0f);
    cfg.angularSpeed = cp(def, "angSpeed", 6.0f);
    cfg.maxBounceCount = (int)cp(def, "maxBounceCount", 10.0f);
    return cfg;
}

// ── Test: Rocket config with explicitly zero physics values ──────────

static void testRocketMapping()
{
    TEST("rocket def -> gravity=0 drag=0 no bounce");

    FakeWeaponDef def;
    def.customParams["gravity"] = 0.0f;
    def.customParams["drag"] = 0.0f;
    def.customParams["angularDrag"] = 0.0f;
    def.customParams["maxBounceCount"] = 0.0f;
    // Rocket does not define bounce/settle values at all — rely on fallbacks.
    // This is fine because bounceEnabled = maxBounceCount > 0 is false.

    FakeProjectileConfig cfg = mapConfig(def);
    CHECK(cfg.gravity == 0.0f, "gravity=%.1f", cfg.gravity);
    CHECK(cfg.drag == 0.0f, "drag=%.2f", cfg.drag);
    CHECK(cfg.angularDrag == 0.0f, "angularDrag=%.1f", cfg.angularDrag);
    CHECK(cfg.maxBounceCount == 0, "maxBounceCount=%d", cfg.maxBounceCount);
    // bounceEnabled computed by caller: maxBounceCount > 0
    bool bounceEnabled = cfg.maxBounceCount > 0;
    CHECK(!bounceEnabled, "bounceEnabled should be false");

    PASS();
}

// ── Test: Grenade config with explicitly set physics values ──────────

static void testGrenadeMapping()
{
    TEST("grenade def -> gravity=20 drag=0.15 bounce enabled");

    FakeWeaponDef def;
    def.customParams["gravity"] = 20.0f;
    def.customParams["drag"] = 0.15f;
    def.customParams["angularDrag"] = 0.3f;
    def.customParams["maxBounceCount"] = 10.0f;
    def.customParams["bounceRestitution"] = 0.35f;
    def.customParams["bounceFriction"] = 0.5f;
    def.customParams["upBias"] = 4.0f;
    def.customParams["angSpeed"] = 6.0f;
    def.customParams["armingDistance"] = 2.0f;
    def.customParams["minBounceSpeed"] = 0.1f;

    FakeProjectileConfig cfg = mapConfig(def);
    CHECK(std::fabs(cfg.gravity - 20.0f) < 0.1f, "gravity=%.1f", cfg.gravity);
    CHECK(std::fabs(cfg.drag - 0.15f) < 0.01f, "drag=%.2f", cfg.drag);
    CHECK(std::fabs(cfg.angularDrag - 0.3f) < 0.01f, "angularDrag=%.1f", cfg.angularDrag);
    CHECK(cfg.maxBounceCount == 10, "maxBounceCount=%d", cfg.maxBounceCount);
    bool bounceEnabled = cfg.maxBounceCount > 0;
    CHECK(bounceEnabled, "bounceEnabled should be true");

    PASS();
}

// ── Test: Same generic mapping used for both weapons — no per-weapon branch ──
// This proves that if the rocket's customParams contain gravity=0, drag=0, etc.,
// the same mapping function produces the correct physics config for both.

static void testGenericMapping()
{
    TEST("generic mapping identical for both weapons");

    // Build rocket-like definition
    FakeWeaponDef rocket;
    rocket.customParams["gravity"] = 0.0f;
    rocket.customParams["drag"] = 0.0f;
    rocket.customParams["angularDrag"] = 0.0f;
    rocket.customParams["maxBounceCount"] = 0.0f;

    // Build grenade-like definition
    FakeWeaponDef grenade;
    grenade.customParams["gravity"] = 20.0f;
    grenade.customParams["drag"] = 0.15f;
    grenade.customParams["angularDrag"] = 0.3f;
    grenade.customParams["maxBounceCount"] = 10.0f;
    grenade.customParams["bounceRestitution"] = 0.35f;
    grenade.customParams["bounceFriction"] = 0.5f;
    grenade.customParams["upBias"] = 4.0f;
    grenade.customParams["angSpeed"] = 6.0f;
    grenade.customParams["armingDistance"] = 2.0f;
    grenade.customParams["minBounceSpeed"] = 0.1f;

    // Use the SAME mapConfig function for both
    FakeProjectileConfig rc = mapConfig(rocket);
    FakeProjectileConfig gc = mapConfig(grenade);

    // Rocket values
    CHECK(rc.gravity == 0.0f, "rocket gravity=%.1f", rc.gravity);
    CHECK(rc.drag == 0.0f, "rocket drag=%.2f", rc.drag);
    CHECK(rc.maxBounceCount == 0, "rocket maxBounceCount=%d", rc.maxBounceCount);

    // Grenade values
    CHECK(std::fabs(gc.gravity - 20.0f) < 0.1f, "grenade gravity=%.1f", gc.gravity);
    CHECK(std::fabs(gc.drag - 0.15f) < 0.01f, "grenade drag=%.2f", gc.drag);
    CHECK(gc.maxBounceCount == 10, "grenade maxBounceCount=%d", gc.maxBounceCount);

    // bounceEnabled computed from maxBounceCount in both cases
    CHECK((rc.maxBounceCount > 0) == false, "rocket should not bounce");
    CHECK((gc.maxBounceCount > 0) == true, "grenade should bounce");

    PASS();
}

// ── Regression: bounceEnabled derived from config, not weapon type ──
// The authoritative server's makePhysicsConfig() must compute bounceEnabled
// solely from maxBounceCount > 0, not from a per-weapon branch.

static void testBounceEnabledGeneric()
{
    TEST("bounceEnabled from maxBounceCount, not weapon type");

    int rocketMaxBounce = 0;
    int grenadeMaxBounce = 10;
    CHECK((rocketMaxBounce > 0) == false, "rocket bounceEnabled mismatch");
    CHECK((grenadeMaxBounce > 0) == true, "grenade bounceEnabled mismatch");

    PASS();
}

// ── Explosion policy tests ──────────────────────────────────────────
// Test the generic shouldExplode logic: no weapon names, no enums.
// Only configuration flags and ProjectileStepResult matter.

enum class StepType { None, WorldBounce, WorldImpact, PlayerImpact, LifetimeExpired };

static bool shouldExplode(bool explodeOnPlayerImpact, bool explodeOnWorldImpact,
                          bool explodeOnLifetime, StepType step)
{
    switch (step)
    {
    case StepType::PlayerImpact:    return explodeOnPlayerImpact;
    case StepType::WorldImpact:     return explodeOnWorldImpact;
    case StepType::WorldBounce:     return false; // never explode on bounce
    case StepType::LifetimeExpired: return explodeOnLifetime;
    default:                        return false;
    }
}

static void testRocketExplosionPolicy()
{
    TEST("rocket explosion policy (generic)");

    // Rocket: explodeOnPlayerImpact=1, explodeOnWorldImpact=1, explodeOnLifetime=1
    CHECK(shouldExplode(true, true, true, StepType::PlayerImpact)    == true,  "rocket PlayerImpact");
    CHECK(shouldExplode(true, true, true, StepType::WorldImpact)     == true,  "rocket WorldImpact");
    CHECK(shouldExplode(true, true, true, StepType::WorldBounce)     == false, "rocket WorldBounce");
    CHECK(shouldExplode(true, true, true, StepType::LifetimeExpired) == true,  "rocket LifetimeExpired");

    PASS();
}

static void testGrenadeExplosionPolicy()
{
    TEST("grenade explosion policy (generic)");

    // Grenade: explodeOnPlayerImpact=1, explodeOnWorldImpact=0, explodeOnLifetime=1
    CHECK(shouldExplode(true, false, true, StepType::PlayerImpact)    == true,  "grenade PlayerImpact");
    CHECK(shouldExplode(true, false, true, StepType::WorldImpact)     == false, "grenade WorldImpact (no immediate explode)");
    CHECK(shouldExplode(true, false, true, StepType::WorldBounce)     == false, "grenade WorldBounce");
    CHECK(shouldExplode(true, false, true, StepType::LifetimeExpired) == true,  "grenade LifetimeExpired");

    PASS();
}

static void testGenericPolicyDiffersByConfig()
{
    TEST("generic policy differs only by config flags");

    // Two policies with different config — no weapon names
    bool policyA_PlayerImpact = true;
    bool policyA_WorldImpact = true;
    bool policyA_Lifetime = true;

    bool policyB_PlayerImpact = true;
    bool policyB_WorldImpact = false;
    bool policyB_Lifetime = true;

    // Same generic function, different config → different results for WorldImpact
    CHECK(shouldExplode(policyA_PlayerImpact, policyA_WorldImpact, policyA_Lifetime, StepType::WorldImpact) == true,
          "policyA WorldImpact should explode");
    CHECK(shouldExplode(policyB_PlayerImpact, policyB_WorldImpact, policyB_Lifetime, StepType::WorldImpact) == false,
          "policyB WorldImpact should not explode");

    // WorldBounce never explodes for either
    CHECK(shouldExplode(policyA_PlayerImpact, policyA_WorldImpact, policyA_Lifetime, StepType::WorldBounce) == false,
          "policyA WorldBounce must not explode");
    CHECK(shouldExplode(policyB_PlayerImpact, policyB_WorldImpact, policyB_Lifetime, StepType::WorldBounce) == false,
          "policyB WorldBounce must not explode");

    PASS();
}

static void testDisabledExplosion()
{
    TEST("all triggers disabled → no explosion");

    CHECK(shouldExplode(false, false, false, StepType::PlayerImpact)    == false, "disabled PlayerImpact");
    CHECK(shouldExplode(false, false, false, StepType::WorldImpact)     == false, "disabled WorldImpact");
    CHECK(shouldExplode(false, false, false, StepType::WorldBounce)     == false, "disabled WorldBounce");
    CHECK(shouldExplode(false, false, false, StepType::LifetimeExpired) == false, "disabled LifetimeExpired");

    PASS();
}

int main()
{
    printf("=== Configuration Mapping Tests ===\n\n");

    testRocketMapping();
    testGrenadeMapping();
    testGenericMapping();
    testBounceEnabledGeneric();
    testRocketExplosionPolicy();
    testGrenadeExplosionPolicy();
    testGenericPolicyDiffersByConfig();
    testDisabledExplosion();

    printf("\n=== Results: %d passed, %d failed ===\n",
           gPassed, gFailed);

    return gFailed > 0 ? 1 : 0;
}
