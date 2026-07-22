// 07 21 2026, 21 45
/* purpose
* Tests generic deterministic hitscan tracing for revolver and shotgun-style definitions.
* Verifies pellet count, seed stability, world blocking, headshot damage, aggregation, and bounds.
* Runs randomized direction generation cases to catch non-finite spread or cap regressions.
* Does NOT open sockets, render effects, mutate ammo, or start the game executable.
* Does NOT test projectile, physical-contact, or client/server process orchestration.
* Does NOT duplicate server packet validation or network transport handling.
*/

#include "combat/weapon-execution.h"

#include <cmath>
#include <cstdio>
#include <string>

static int gFailures = 0;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

static bool nearVec(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(a - b) < 0.0001f;
}

static WeaponDefinition makeDef(const char* id, int pellets, float spread, float damage)
{
    WeaponDefinition def;
    def.id = id;
    def.behaviorType = WeaponBehaviorType::Hitscan;
    def.executionType = WeaponExecutionType::Hitscan;
    def.damage = damage;
    def.headshotMultiplier = 2.0f;
    def.pelletCount = pellets;
    def.spread = spread;
    return def;
}

static WeaponExecution::PlayerTarget target(uint32_t id, glm::vec3 pos, float radius = 0.65f)
{
    WeaponExecution::PlayerTarget t;
    t.playerId = id;
    t.spawnGeneration = 7;
    t.position = pos;
    t.radius = radius;
    t.height = 3.5f;
    return t;
}

static void testRevolverTrace()
{
    WeaponDefinition def = makeDef("revolver", 1, 0.0f, 50.0f);
    WeaponExecution::HitscanTraceConfig cfg;
    cfg.maxRange = 50.0f;
    cfg.damage = def.damage;
    cfg.headshotMultiplier = def.headshotMultiplier;
    cfg.pelletCount = def.pelletCount;
    cfg.spreadDegrees = def.spread;
    cfg.worldBlockDistance = cfg.maxRange;
    auto result = WeaponExecution::traceHitscan(
        def, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), cfg,
        {target(2, glm::vec3(10.0f, 0.0f, 0.0f))});
    check(result.pelletCount == 1, "revolver emits one pellet");
    check(result.aggregates.size() == 1, "revolver hits one target");
    check(result.aggregates[0].damage == 50, "revolver aggregate damage is server-computed");
}

static void testHeadshotAndWorldBlock()
{
    WeaponDefinition def = makeDef("revolver", 1, 0.0f, 50.0f);
    WeaponExecution::HitscanTraceConfig cfg;
    cfg.maxRange = 50.0f;
    cfg.damage = def.damage;
    cfg.headshotMultiplier = def.headshotMultiplier;
    cfg.pelletCount = 1;
    cfg.worldBlockDistance = cfg.maxRange;
    auto head = WeaponExecution::traceHitscan(
        def, glm::vec3(0.0f, 0.0f, 1.2f), glm::vec3(1.0f, 0.0f, 0.0f), cfg,
        {target(3, glm::vec3(10.0f, 0.0f, 0.0f))});
    check(head.aggregates.size() == 1, "headshot target hit");
    check(head.aggregates[0].headshot, "headshot flag set");
    check(head.aggregates[0].damage == 100, "headshot multiplier applied");

    cfg.worldBlockDistance = 5.0f;
    auto blocked = WeaponExecution::traceHitscan(
        def, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), cfg,
        {target(4, glm::vec3(10.0f, 0.0f, 0.0f))});
    check(blocked.aggregates.empty(), "world hit blocks farther player");
}

static void testShotgunSpreadAggregation()
{
    WeaponDefinition def = makeDef("shotgun", 15, 12.0f, 12.0f);
    WeaponExecution::HitscanTraceConfig cfg;
    cfg.maxRange = 40.0f;
    cfg.damage = def.damage;
    cfg.headshotMultiplier = def.headshotMultiplier;
    cfg.pelletCount = def.pelletCount;
    cfg.spreadDegrees = def.spread;
    cfg.worldBlockDistance = cfg.maxRange;
    cfg.deterministicSeed = 1234;
    auto result = WeaponExecution::traceHitscan(
        def, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), cfg,
        {target(5, glm::vec3(10.0f, 0.0f, 0.0f), 3.0f)});
    check(result.pelletCount == 15, "shotgun emits configured pellet count");
    check(result.aggregates.size() == 1, "shotgun aggregates target damage once");
    check(result.aggregates[0].pelletHits > 1, "shotgun records multiple pellet hits");
    check(result.aggregates[0].damage <= 15 * 12 * 2, "shotgun damage remains bounded");
}

static void testDeterministicSeeds()
{
    WeaponDefinition def = makeDef("shotgun", 15, 8.0f, 12.0f);
    glm::vec3 a[MAX_PELLETS_PER_BLAST]{};
    glm::vec3 b[MAX_PELLETS_PER_BLAST]{};
    glm::vec3 c[MAX_PELLETS_PER_BLAST]{};
    int ac = WeaponExecution::buildPelletDirections(def, glm::vec3(1.0f, 0.0f, 0.0f), 99, a, MAX_PELLETS_PER_BLAST);
    int bc = WeaponExecution::buildPelletDirections(def, glm::vec3(1.0f, 0.0f, 0.0f), 99, b, MAX_PELLETS_PER_BLAST);
    int cc = WeaponExecution::buildPelletDirections(def, glm::vec3(1.0f, 0.0f, 0.0f), 100, c, MAX_PELLETS_PER_BLAST);
    check(ac == 15 && bc == 15 && cc == 15, "deterministic spread keeps pellet count");
    bool same = true;
    bool different = false;
    for (int i = 0; i < ac; ++i)
    {
        same = same && nearVec(a[i], b[i]);
        different = different || !nearVec(a[i], c[i]);
    }
    check(same, "same seed reproduces exact pellet directions");
    check(different, "different seed changes at least one pellet direction");
}

static void testRandomizedDirections()
{
    uint32_t seed = 0x4a7c15u;
    int cases = 2000;
    for (int i = 0; i < cases; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        WeaponDefinition def = makeDef("random", 1 + (int)(seed % 24), (seed % 2400) / 100.0f, 10.0f);
        glm::vec3 dirs[MAX_PELLETS_PER_BLAST]{};
        int count = WeaponExecution::buildPelletDirections(
            def, glm::vec3(1.0f, (float)((seed >> 8) & 15) * 0.01f, 0.1f),
            seed, dirs, MAX_PELLETS_PER_BLAST);
        check(count >= 1 && count <= MAX_PELLETS_PER_BLAST, "random pellet count capped");
        for (int p = 0; p < count; ++p)
        {
            check(std::isfinite(dirs[p].x) && std::isfinite(dirs[p].y) &&
                  std::isfinite(dirs[p].z), "random pellet direction finite");
            check(std::fabs(glm::length(dirs[p]) - 1.0f) < 0.001f,
                  "random pellet direction normalized");
        }
    }
    std::printf("[generic-hitscan-test] randomized seed=0x%08x cases=%d\n", seed, cases);
}

int main()
{
    testRevolverTrace();
    testHeadshotAndWorldBlock();
    testShotgunSpreadAggregation();
    testDeterministicSeeds();
    testRandomizedDirections();
    if (gFailures)
    {
        std::printf("[generic-hitscan-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[generic-hitscan-test] PASS\n");
    return 0;
}
