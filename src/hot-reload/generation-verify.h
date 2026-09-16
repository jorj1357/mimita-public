// 09 15 2026
/* purpose
* Minimal generation verify gate: given a candidate generation manifest and the
* peer's local facts, decide whether the peer may report READY(G). Covers
* artifact hash, logical generation association, hot ABI compatibility, required
* generic capabilities, required schemas, and required dependencies, with an
* explicit failure reason. Does NOT build a package ecosystem and does NOT own
* acquisition, activation, or migration application.
*/
#pragma once

#include <cstdint>

namespace MimitaRuntime {

enum class VerifyFailure : std::uint32_t {
    None = 0,
    HashMismatch = 1,
    LogicalGenerationMismatch = 2,
    AbiMismatch = 3,
    MissingCapability = 4,
    SchemaMismatch = 5,
    DependencyMissing = 6,
    LoadFailed = 7,
    MigrationFailed = 8,
};

inline const char* verifyFailureName(VerifyFailure f)
{
    switch (f) {
    case VerifyFailure::None: return "none";
    case VerifyFailure::HashMismatch: return "hash-mismatch";
    case VerifyFailure::LogicalGenerationMismatch: return "logical-generation-mismatch";
    case VerifyFailure::AbiMismatch: return "abi-mismatch";
    case VerifyFailure::MissingCapability: return "missing-capability";
    case VerifyFailure::SchemaMismatch: return "schema-mismatch";
    case VerifyFailure::DependencyMissing: return "dependency-missing";
    case VerifyFailure::LoadFailed: return "load-failed";
    case VerifyFailure::MigrationFailed: return "migration-failed";
    }
    return "unknown";
}

static constexpr std::uint32_t kMaxVerifyRequirements = 8;

struct GenerationManifestV1 {
    std::uint64_t logicalGenerationId = 0;
    std::uint64_t logicalBehaviorHash = 0;
    std::uint64_t platformArtifactHash = 0;
    std::uint32_t platformArtifactSize = 0;
    std::uint32_t hotAbiVersion = 0;

    std::uint64_t requiredCapabilities[kMaxVerifyRequirements] = {};
    std::uint32_t requiredCapabilityCount = 0;
    std::uint64_t requiredSchemas[kMaxVerifyRequirements] = {};  // schema id
    std::uint32_t requiredSchemaVersions[kMaxVerifyRequirements] = {};  // target
    std::uint32_t requiredSchemaCount = 0;
    std::uint64_t requiredDependencies[kMaxVerifyRequirements] = {};
    std::uint32_t requiredDependencyCount = 0;
};

// Peer-local facts the gate checks against.
struct GenerationLocalFactsV1 {
    std::uint64_t artifactHash = 0;
    std::uint32_t artifactSize = 0;
    std::uint32_t coldAbiVersion = 0;
    // Availability probes (null = treat the category as trivially satisfied).
    bool (*hasCapability)(void* user, std::uint64_t capabilityId) = nullptr;
    bool (*hasSchema)(void* user, std::uint64_t schemaHash) = nullptr;
    bool (*hasDependency)(void* user, std::uint64_t dependencyId) = nullptr;
    void* user = nullptr;
};

inline VerifyFailure verifyGeneration(const GenerationManifestV1& manifest,
                                      const GenerationLocalFactsV1& facts)
{
    // Artifact identity.
    if (manifest.platformArtifactHash == 0 ||
        manifest.platformArtifactHash != facts.artifactHash)
        return VerifyFailure::HashMismatch;
    if (manifest.platformArtifactSize != 0 &&
        manifest.platformArtifactSize != facts.artifactSize)
        return VerifyFailure::HashMismatch;

    // Logical generation association (when the peer pins one).
    if (manifest.logicalGenerationId == 0)
        return VerifyFailure::LogicalGenerationMismatch;

    // Hot ABI compatibility.
    if (manifest.hotAbiVersion != 0 &&
        manifest.hotAbiVersion != facts.coldAbiVersion)
        return VerifyFailure::AbiMismatch;

    // Required generic capabilities.
    if (facts.hasCapability) {
        for (std::uint32_t i = 0; i < manifest.requiredCapabilityCount; ++i) {
            if (!facts.hasCapability(facts.user, manifest.requiredCapabilities[i]))
                return VerifyFailure::MissingCapability;
        }
    }

    // Required component/schema versions.
    if (facts.hasSchema) {
        for (std::uint32_t i = 0; i < manifest.requiredSchemaCount; ++i) {
            if (!facts.hasSchema(facts.user, manifest.requiredSchemas[i]))
                return VerifyFailure::SchemaMismatch;
        }
    }

    // Required package/resource dependencies.
    if (facts.hasDependency) {
        for (std::uint32_t i = 0; i < manifest.requiredDependencyCount; ++i) {
            if (!facts.hasDependency(facts.user, manifest.requiredDependencies[i]))
                return VerifyFailure::DependencyMissing;
        }
    }

    return VerifyFailure::None;
}

} // namespace MimitaRuntime
