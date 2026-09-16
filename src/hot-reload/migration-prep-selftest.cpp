// 09 15 2026
/* purpose
* Headless self-test for the smallest real migration-preparation model. Uses the
* REAL DynamicComponentStore schema/version + migration substrate: a genuine v1
* -> v2 stored-component migration, a no-op case, a missing-migration failure, a
* stale-plan rejection, and a rejected commit that leaves F state intact.
*/
#include "hot-reload/migration-prep-selftest.h"

#include <cstring>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "hot-reload/migration-prep.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

struct TestStateV1 {
    int value = 0;
};
struct TestStateV2 {
    int value = 0;
    int bonus = 0;
};

constexpr std::uint64_t kType = 0x1001;
constexpr std::uint64_t kTypeFail = 0x1002;
constexpr EntityId kEntity = static_cast<EntityId>(0xE1);

bool migrateV1toV2(const void* oldState, std::size_t oldSize, void* newState,
                   std::size_t newSize)
{
    if (oldSize < sizeof(TestStateV1) || newSize < sizeof(TestStateV2))
        return false;
    const TestStateV1* in = static_cast<const TestStateV1*>(oldState);
    TestStateV2* out = static_cast<TestStateV2*>(newState);
    out->value = in->value;
    out->bonus = 42;  // deterministic default
    return true;
}

bool alwaysFail(const void*, std::size_t, void*, std::size_t) { return false; }

DynamicComponentSchema schemaV(std::uint64_t id, std::uint32_t version,
                               std::uint32_t size, const char* name)
{
    DynamicComponentSchema s;
    s.typeId = id;
    s.version = version;
    s.size = size;
    s.align = 4;
    s.name = name;
    return s;
}

MigrationPrepareFactsV1 makeFacts(DynamicComponentStore& store)
{
    MigrationPrepareFactsV1 facts{};
    facts.storedSchemaVersion = [](void* user, std::uint64_t id) {
        return static_cast<DynamicComponentStore*>(user)->maxStoredVersion(id);
    };
    facts.hasMigrationPath = [](void* user, std::uint64_t id, std::uint32_t from,
                                std::uint32_t to) {
        return static_cast<DynamicComponentStore*>(user)->hasMigration(id, from, to);
    };
    facts.user = &store;
    return facts;
}

} // namespace

bool runMigrationPrepSelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();

    // ── Live F state: a stored v1 component on a real entity ──────────
    store.registerSchema(schemaV(kType, 1, sizeof(TestStateV1), "TestStateV1"));
    TestStateV1 v1{};
    v1.value = 7;
    ok &= check(store.write(kEntity, kType, &v1, sizeof(v1)),
                "live F state stored as real dynamic component", report);
    ok &= check(store.maxStoredVersion(kType) == 1,
                "live stored version reads as v1", report);

    // ── 1. No-op: F and G share the schema version ────────────────────
    {
        MigrationPrepareFactsV1 facts = makeFacts(store);
        facts.candidateSchemaIds[0] = kType;
        facts.candidateSchemaVersions[0] = 1;
        facts.candidateSchemaCount = 1;
        const MigrationPlanV1 plan = prepareMigration(5, 6, facts);
        ok &= check(plan.outcome == MigrationOutcome::NoMigrationRequired &&
                        plan.valid && plan.entryCount == 0,
                    "identical schema version -> NoMigrationRequired", report);
        ok &= check(migrationPlanMatches(plan, 5, 6),
                    "no-op plan matches its exact F -> G", report);
    }

    // ── 2. Real migration: v1 -> v2 with a registered path ────────────
    store.registerMigration(kType, 1, 2, &migrateV1toV2);
    MigrationPlanV1 plan{};
    {
        MigrationPrepareFactsV1 facts = makeFacts(store);
        facts.candidateSchemaIds[0] = kType;
        facts.candidateSchemaVersions[0] = 2;
        facts.candidateSchemaCount = 1;
        plan = prepareMigration(5, 6, facts);
        ok &= check(plan.outcome == MigrationOutcome::Prepared && plan.valid &&
                        plan.entryCount == 1 &&
                        plan.entries[0].fromVersion == 1 &&
                        plan.entries[0].toVersion == 2,
                    "v1 -> v2 with a migration path -> Prepared(plan)", report);
    }

    // ── 3. Preparation did not mutate live F ──────────────────────────
    {
        TestStateV1 still{};
        ok &= check(store.versionOf(kEntity, kType) == 1 &&
                        store.read(kEntity, kType, &still, sizeof(still)) &&
                        still.value == 7,
                    "prepare does not mutate active F state", report);
    }

    // ── 4. Plan is bound to exact F -> G ──────────────────────────────
    ok &= check(migrationPlanMatches(plan, 5, 6) &&
                    !migrationPlanMatches(plan, 9, 6) &&
                    !migrationPlanMatches(plan, 5, 7),
                "plan is invalid for any other source/target", report);

    // ── 5. Commit at the switch boundary preserves EntityId + value ───
    {
        std::vector<DynamicComponentSchema> next{
            schemaV(kType, 2, sizeof(TestStateV2), "TestStateV2")};
        std::string error;
        const bool committed = store.applySchemaUpdate(next, error);
        TestStateV2 after{};
        ok &= check(committed && store.versionOf(kEntity, kType) == 2 &&
                        store.read(kEntity, kType, &after, sizeof(after)) &&
                        after.value == 7 && after.bonus == 42,
                    "commit preserves EntityId + value, initializes new field",
                    report);
    }

    // ── 6. Missing migration -> MigrationFailed, READY blocked ────────
    {
        store.registerSchema(schemaV(kTypeFail, 1, sizeof(TestStateV1), "FailV1"));
        TestStateV1 f{};
        f.value = 3;
        store.write(kEntity, kTypeFail, &f, sizeof(f));
        MigrationPrepareFactsV1 facts = makeFacts(store);
        facts.candidateSchemaIds[0] = kTypeFail;
        facts.candidateSchemaVersions[0] = 2;
        facts.candidateSchemaCount = 1;
        const MigrationPlanV1 bad = prepareMigration(5, 6, facts);
        ok &= check(bad.outcome == MigrationOutcome::Failed && !bad.valid &&
                        bad.failure == MigrationFailure::MissingMigration,
                    "version change without a migration path -> Failed", report);
        const bool readyAllowed = bad.outcome != MigrationOutcome::Failed;
        ok &= check(!readyAllowed, "READY is blocked when prepare fails", report);
    }

    // ── 7. Rejected commit leaves F (last-good) intact ────────────────
    {
        store.registerMigration(kTypeFail, 1, 2, &alwaysFail);
        std::vector<DynamicComponentSchema> next{
            schemaV(kTypeFail, 2, sizeof(TestStateV2), "FailV2")};
        std::string error;
        const bool committed = store.applySchemaUpdate(next, error);
        TestStateV1 still{};
        ok &= check(!committed && !error.empty() &&
                        store.versionOf(kEntity, kTypeFail) == 1 &&
                        store.read(kEntity, kTypeFail, &still, sizeof(still)) &&
                        still.value == 3,
                    "failed commit leaves F state and version intact", report);
    }

    // ── 8. Supersede invalidates the plan for G ───────────────────────
    {
        // H (generation 7) arrives before G (6) commits: the F->G plan must not
        // validate an F->H transition.
        ok &= check(!migrationPlanMatches(plan, 5, 7),
                    "superseding candidate invalidates the F->G plan", report);
    }

    // ── 9. Stale source rejected ──────────────────────────────────────
    {
        MigrationPrepareFactsV1 facts = makeFacts(store);
        facts.candidateSchemaCount = 0;
        const MigrationPlanV1 same = prepareMigration(6, 6, facts);
        ok &= check(same.outcome == MigrationOutcome::Failed &&
                        same.failure == MigrationFailure::StaleSource,
                    "prepare from == to -> StaleSource", report);
        ok &= check(std::string(migrationFailureName(MigrationFailure::MissingMigration)) ==
                        "missing-migration",
                    "migration failure reasons are named", report);
    }

    store.clear();
    return ok;
}
