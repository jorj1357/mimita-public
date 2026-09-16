// 09 15 2026
/* purpose
* Headless self-test for the explicit switch-boundary transaction: the prepared
* F -> G migration plan must be validated against the REAL active generation, and
* state migration + generation publication must be atomic. Uses the REAL
* DynamicComponentStore to commit a v1 -> v2 state migration at the boundary and
* proves last-good on stale/missing/invalid plans.
*/
#include "hot-reload/switch-transaction-selftest.h"

#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "hot-reload/migration-prep.h"
#include "hot-reload/switch-transaction.h"

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

constexpr std::uint64_t kType = 0x2001;
constexpr EntityId kEntity = static_cast<EntityId>(0xE2);

bool migrateV1toV2(const void* oldState, std::size_t oldSize, void* newState,
                   std::size_t newSize)
{
    if (oldSize < sizeof(TestStateV1) || newSize < sizeof(TestStateV2))
        return false;
    const TestStateV1* in = static_cast<const TestStateV1*>(oldState);
    TestStateV2* out = static_cast<TestStateV2*>(newState);
    out->value = in->value;
    out->bonus = 42;
    return true;
}

DynamicComponentSchema schemaV(std::uint64_t id, std::uint32_t version,
                               std::uint32_t size)
{
    DynamicComponentSchema s;
    s.typeId = id;
    s.version = version;
    s.size = size;
    s.align = 4;
    return s;
}

MigrationPlanV1 prepareFor(DynamicComponentStore& store, std::uint32_t targetVersion,
                           std::uint64_t from, std::uint64_t to)
{
    MigrationPrepareFactsV1 facts{};
    facts.candidateSchemaCount = 1;
    facts.candidateSchemaIds[0] = kType;
    facts.candidateSchemaVersions[0] = targetVersion;
    facts.storedSchemaVersion = [](void* user, std::uint64_t id) {
        return static_cast<DynamicComponentStore*>(user)->maxStoredVersion(id);
    };
    facts.hasMigrationPath = [](void* user, std::uint64_t id, std::uint32_t f,
                                std::uint32_t t) {
        return static_cast<DynamicComponentStore*>(user)->hasMigration(id, f, t);
    };
    facts.user = &store;
    return prepareMigration(from, to, facts);
}

} // namespace

bool runSwitchTransactionSelfTest(std::string& report)
{
    bool ok = true;

    // ── Pure switch validation matrix ─────────────────────────────────
    {
        MigrationPlanV1 good{};
        good.fromGeneration = 5;
        good.toGeneration = 6;
        good.valid = true;
        good.outcome = MigrationOutcome::NoMigrationRequired;

        SwitchTransactionFactsV1 f{};
        f.activeGeneration = 0;
        f.candidateGeneration = 6;
        f.plan = nullptr;
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::None,
                    "initial load needs no plan", report);

        f.activeGeneration = 5;
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::PlanMissing,
                    "missing plan at a real switch is rejected", report);

        f.plan = &good;
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::None,
                    "no-op plan for the exact F -> G is accepted", report);

        f.candidateGeneration = 0;
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::NoCandidate,
                    "no candidate -> NoCandidate", report);

        f.candidateGeneration = 6;
        f.activeGeneration = 9;  // source changed to X before commit
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::PlanStale,
                    "stale source generation -> PlanStale", report);

        f.activeGeneration = 5;
        MigrationPlanV1 bad = good;
        bad.outcome = MigrationOutcome::Failed;
        bad.valid = false;
        f.plan = &bad;
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::PlanInvalid,
                    "failed preparation -> PlanInvalid", report);
    }

    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();

    // ── Atomic state migration + generation publication (T-1/T/T+1) ───
    {
        store.registerSchema(schemaV(kType, 1, sizeof(TestStateV1)));
        TestStateV1 v1{};
        v1.value = 123;
        store.write(kEntity, kType, &v1, sizeof(v1));
        store.registerMigration(kType, 1, 2, &migrateV1toV2);

        std::uint64_t activeGeneration = 5;   // F
        const std::uint64_t candidateGeneration = 6;  // G

        // T-1: F active, old state visible.
        TestStateV1 before{};
        ok &= check(activeGeneration == 5 &&
                        store.versionOf(kEntity, kType) == 1 &&
                        store.read(kEntity, kType, &before, sizeof(before)) &&
                        before.value == 123,
                    "T-1: F active with F-schema state", report);

        // Boundary: validate the prepared plan, then commit state + publish.
        const MigrationPlanV1 plan = prepareFor(store, 2, activeGeneration,
                                                candidateGeneration);
        SwitchTransactionFactsV1 f{};
        f.activeGeneration = activeGeneration;
        f.candidateGeneration = candidateGeneration;
        f.plan = &plan;
        const SwitchRejection rejection = validateSwitchTransaction(f);
        bool committed = false;
        if (rejection == SwitchRejection::None) {
            std::vector<DynamicComponentSchema> next{
                schemaV(kType, 2, sizeof(TestStateV2))};
            std::string error;
            committed = store.applySchemaUpdate(next, error);
            if (committed)
                activeGeneration = candidateGeneration;  // publish G
        }

        // T+1: G active, migrated state visible on the same EntityId.
        TestStateV2 after{};
        ok &= check(rejection == SwitchRejection::None && committed &&
                        activeGeneration == candidateGeneration &&
                        store.versionOf(kEntity, kType) == 2 &&
                        store.read(kEntity, kType, &after, sizeof(after)) &&
                        after.value == 123 && after.bonus == 42,
                    "T: atomic migration + publication, T+1: G state on same id",
                    report);
    }

    // ── Stale plan on the production path: no mutation ────────────────
    {
        store.clear();
        store.registerSchema(schemaV(kType, 1, sizeof(TestStateV1)));
        TestStateV1 s{};
        s.value = 9;
        store.write(kEntity, kType, &s, sizeof(s));
        store.registerMigration(kType, 1, 2, &migrateV1toV2);

        const MigrationPlanV1 plan = prepareFor(store, 2, 5, 6);  // F=5 -> G=6
        SwitchTransactionFactsV1 f{};
        f.activeGeneration = 7;  // active is now X, not F
        f.candidateGeneration = 6;
        f.plan = &plan;
        const SwitchRejection rejection = validateSwitchTransaction(f);
        ok &= check(rejection == SwitchRejection::PlanStale &&
                        store.versionOf(kEntity, kType) == 1,
                    "stale plan rejected with no state mutation", report);
    }

    // ── Missing plan with a required migration: no publication ────────
    {
        SwitchTransactionFactsV1 f{};
        f.activeGeneration = 5;
        f.candidateGeneration = 6;
        f.plan = nullptr;
        ok &= check(validateSwitchTransaction(f) == SwitchRejection::PlanMissing,
                    "plan absent at a required switch -> rejected", report);
    }

    // ── No-migration switch path still works ──────────────────────────
    {
        store.clear();
        store.registerSchema(schemaV(kType, 1, sizeof(TestStateV1)));
        TestStateV1 s{};
        s.value = 4;
        store.write(kEntity, kType, &s, sizeof(s));
        const MigrationPlanV1 plan = prepareFor(store, 1, 5, 6);
        SwitchTransactionFactsV1 f{};
        f.activeGeneration = 5;
        f.candidateGeneration = 6;
        f.plan = &plan;
        const bool accepted = validateSwitchTransaction(f) == SwitchRejection::None &&
            plan.outcome == MigrationOutcome::NoMigrationRequired;
        std::vector<DynamicComponentSchema> next{
            schemaV(kType, 1, sizeof(TestStateV1))};
        std::string error;
        const bool committed = store.applySchemaUpdate(next, error);
        TestStateV1 after{};
        ok &= check(accepted && committed &&
                        store.read(kEntity, kType, &after, sizeof(after)) &&
                        after.value == 4,
                    "no-op migration switch activates normally", report);
    }

    store.clear();
    return ok;
}
