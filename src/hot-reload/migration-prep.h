// 09 15 2026
/* purpose
* Smallest real migration-preparation model for hot generations. Before a peer
* reports READY(G) it must know whether live state owned by the active generation
* F can be transformed for G. This answers NO_MIGRATION_REQUIRED / PREPARED(plan)
* / FAILED(reason) by comparing the candidate's declared schema versions against
* LIVE stored state and resolving whether a registered migration path exists.
*
* It reuses the existing DynamicComponentStore schema/version + migration
* substrate (no duplicate schema identity system, no migration language). The
* plan stores LOGICAL migration identities (schema id + from/to version), never
* function pointers, so it cannot dangle across a generation unload; commit
* resolves the path against the still-registered migrations atomically via
* applySchemaUpdate. Preparation never mutates active F state.
*/
#pragma once

#include <cstdint>

#include "hot-reload/generation-verify.h"

namespace MimitaRuntime {

enum class MigrationOutcome : std::uint32_t {
    NoMigrationRequired = 0,
    Prepared = 1,
    Failed = 2,
};

enum class MigrationFailure : std::uint32_t {
    None = 0,
    StaleSource = 1,       // from == to, or no active source to migrate from
    MissingMigration = 2,  // stored version differs but no migration path exists
};

inline const char* migrationFailureName(MigrationFailure f)
{
    switch (f) {
    case MigrationFailure::None: return "none";
    case MigrationFailure::StaleSource: return "stale-source";
    case MigrationFailure::MissingMigration: return "missing-migration";
    }
    return "unknown";
}

static constexpr std::uint32_t kMaxMigrationEntries = kMaxVerifyRequirements;

struct MigrationPlanEntryV1 {
    std::uint64_t schemaId = 0;
    std::uint32_t fromVersion = 0;
    std::uint32_t toVersion = 0;
};

struct MigrationPlanV1 {
    std::uint64_t fromGeneration = 0;
    std::uint64_t toGeneration = 0;
    std::uint32_t entryCount = 0;
    MigrationPlanEntryV1 entries[kMaxMigrationEntries] = {};
    bool valid = false;
    MigrationOutcome outcome = MigrationOutcome::NoMigrationRequired;
    MigrationFailure failure = MigrationFailure::None;
};

// Candidate schema set + live-state probes. Supplied by the caller; the model
// stores no callbacks or pointers.
struct MigrationPrepareFactsV1 {
    std::uint32_t candidateSchemaCount = 0;
    std::uint64_t candidateSchemaIds[kMaxVerifyRequirements] = {};
    std::uint32_t candidateSchemaVersions[kMaxVerifyRequirements] = {};
    // Highest live stored version for a schema id (0 = none stored).
    std::uint32_t (*storedSchemaVersion)(void* user, std::uint64_t schemaId) = nullptr;
    // Whether a migration path exists for this exact transition.
    bool (*hasMigrationPath)(void* user, std::uint64_t schemaId,
                             std::uint32_t fromVersion,
                             std::uint32_t toVersion) = nullptr;
    void* user = nullptr;
};

// Builds a plan bound to the exact F -> G transition. Never mutates live state.
inline MigrationPlanV1 prepareMigration(std::uint64_t fromGeneration,
                                        std::uint64_t toGeneration,
                                        const MigrationPrepareFactsV1& facts)
{
    MigrationPlanV1 plan{};
    plan.fromGeneration = fromGeneration;
    plan.toGeneration = toGeneration;

    if (fromGeneration == 0 || toGeneration == 0 || fromGeneration == toGeneration) {
        plan.outcome = MigrationOutcome::Failed;
        plan.failure = MigrationFailure::StaleSource;
        return plan;
    }

    const std::uint32_t count = facts.candidateSchemaCount < kMaxVerifyRequirements
        ? facts.candidateSchemaCount
        : kMaxVerifyRequirements;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t id = facts.candidateSchemaIds[i];
        const std::uint32_t target = facts.candidateSchemaVersions[i];
        if (id == 0 || target == 0)
            continue;
        std::uint32_t stored = 0;
        if (facts.storedSchemaVersion)
            stored = facts.storedSchemaVersion(facts.user, id);
        // No live state, or already at the target version: nothing to migrate.
        if (stored == 0 || stored == target)
            continue;
        if (!facts.hasMigrationPath ||
            !facts.hasMigrationPath(facts.user, id, stored, target)) {
            plan.outcome = MigrationOutcome::Failed;
            plan.failure = MigrationFailure::MissingMigration;
            return plan;
        }
        if (plan.entryCount >= kMaxMigrationEntries) {
            plan.outcome = MigrationOutcome::Failed;
            plan.failure = MigrationFailure::MissingMigration;
            return plan;
        }
        MigrationPlanEntryV1& e = plan.entries[plan.entryCount++];
        e.schemaId = id;
        e.fromVersion = stored;
        e.toVersion = target;
    }

    plan.valid = true;
    plan.outcome = plan.entryCount > 0 ? MigrationOutcome::Prepared
                                       : MigrationOutcome::NoMigrationRequired;
    return plan;
}

// A plan may only be committed against the exact F -> G it was prepared for.
// Any other active generation means the plan is stale and must be rebuilt.
inline bool migrationPlanMatches(const MigrationPlanV1& plan,
                                 std::uint64_t activeGeneration,
                                 std::uint64_t targetGeneration)
{
    return plan.valid &&
        plan.fromGeneration == activeGeneration &&
        plan.toGeneration == targetGeneration;
}

} // namespace MimitaRuntime
