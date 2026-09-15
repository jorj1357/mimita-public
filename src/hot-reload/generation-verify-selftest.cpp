// 09 15 2026
/* purpose
* Implements the headless generation verify-gate self-test: valid generation
* passes; bad hash, bad ABI, missing capability, schema mismatch, and missing
* dependency each fail with an explicit reason. Does NOT own rendering or the
* transport.
*/
#include "hot-reload/generation-verify-selftest.h"

#include <string>

#include "hot-reload/generation-verify.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "ecs/dynamic-components.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

// Simple local facts: capability id 111 exists, schema hash 222 exists,
// dependency 333 exists.
struct LocalRegistry {
    bool hasCapability(std::uint64_t id) const { return id == 111; }
    bool hasSchema(std::uint64_t hash) const { return hash == 222; }
    bool hasDependency(std::uint64_t id) const { return id == 333; }
};
bool capProbe(void* u, std::uint64_t id) { return static_cast<LocalRegistry*>(u)->hasCapability(id); }
bool schemaProbe(void* u, std::uint64_t h) { return static_cast<LocalRegistry*>(u)->hasSchema(h); }
bool depProbe(void* u, std::uint64_t id) { return static_cast<LocalRegistry*>(u)->hasDependency(id); }

GenerationManifestV1 makeValid()
{
    GenerationManifestV1 m{};
    m.logicalGenerationId = 0xABC;
    m.logicalBehaviorHash = 0xDEF;
    m.platformArtifactHash = 0x1234;
    m.platformArtifactSize = 3000;
    m.hotAbiVersion = 8;
    m.requiredCapabilities[0] = 111;
    m.requiredCapabilityCount = 1;
    m.requiredSchemas[0] = 222;
    m.requiredSchemaCount = 1;
    m.requiredDependencies[0] = 333;
    m.requiredDependencyCount = 1;
    return m;
}

GenerationLocalFactsV1 makeFacts(LocalRegistry& reg)
{
    GenerationLocalFactsV1 f{};
    f.artifactHash = 0x1234;
    f.artifactSize = 3000;
    f.coldAbiVersion = 8;
    f.hasCapability = &capProbe;
    f.hasSchema = &schemaProbe;
    f.hasDependency = &depProbe;
    f.user = &reg;
    return f;
}

} // namespace

bool runGenerationVerifySelfTest(std::string& report)
{
    bool ok = true;
    LocalRegistry reg;

    ok &= check(verifyGeneration(makeValid(), makeFacts(reg)) == VerifyFailure::None,
                "valid generation passes verify", report);

    {
        GenerationManifestV1 m = makeValid();
        GenerationLocalFactsV1 f = makeFacts(reg);
        f.artifactHash = 0x9999;  // client hash differs
        ok &= check(verifyGeneration(m, f) == VerifyFailure::HashMismatch,
                    "artifact hash mismatch -> HashMismatch", report);
    }

    {
        GenerationManifestV1 m = makeValid();
        m.hotAbiVersion = 9;
        ok &= check(verifyGeneration(m, makeFacts(reg)) == VerifyFailure::AbiMismatch,
                    "hot ABI mismatch -> AbiMismatch", report);
    }

    {
        GenerationManifestV1 m = makeValid();
        m.requiredCapabilities[0] = 999;  // unknown capability
        ok &= check(verifyGeneration(m, makeFacts(reg)) == VerifyFailure::MissingCapability,
                    "missing capability -> MissingCapability", report);
    }

    {
        GenerationManifestV1 m = makeValid();
        m.requiredSchemas[0] = 888;  // unknown schema
        ok &= check(verifyGeneration(m, makeFacts(reg)) == VerifyFailure::SchemaMismatch,
                    "schema mismatch -> SchemaMismatch", report);
    }

    {
        GenerationManifestV1 m = makeValid();
        m.requiredDependencies[0] = 777;  // missing dependency
        ok &= check(verifyGeneration(m, makeFacts(reg)) == VerifyFailure::DependencyMissing,
                    "missing dependency -> DependencyMissing", report);
    }

    {
        GenerationManifestV1 m = makeValid();
        m.logicalGenerationId = 0;
        ok &= check(verifyGeneration(m, makeFacts(reg)) ==
                        VerifyFailure::LogicalGenerationMismatch,
                    "missing logical generation -> LogicalGenerationMismatch", report);
    }

    ok &= check(std::string(verifyFailureName(VerifyFailure::AbiMismatch)) ==
                    "abi-mismatch",
                "failure reasons are named for debugging", report);

    // ── Real manifest population from live package registration ────────
    HotReloadSystem::instance().startup();
    GenericRuntime& rt = GenericRuntime::instance();
    GenerationManifestV1 real{};
    real.logicalGenerationId = 1;
    real.platformArtifactHash = 0x1234;
    real.hotAbiVersion = MIMITA_GAME_API_VERSION;
    for (std::size_t i = 0;
         i < rt.capabilityRequirementCount() &&
         real.requiredCapabilityCount < kMaxVerifyRequirements;
         ++i) {
        std::uint64_t id = 0;
        if (rt.capabilityRequirementAt(i, &id))
            real.requiredCapabilities[real.requiredCapabilityCount++] = id;
    }
    for (std::size_t i = 0;
         i < rt.schemaCount() && real.requiredSchemaCount < kMaxVerifyRequirements;
         ++i) {
        std::uint64_t id = 0;
        if (rt.schemaAt(i, &id))
            real.requiredSchemas[real.requiredSchemaCount++] = id;
    }

    GenerationLocalFactsV1 live{};
    live.artifactHash = 0x1234;
    live.coldAbiVersion = MIMITA_GAME_API_VERSION;
    live.hasCapability = [](void*, std::uint64_t id) {
        return GenericRuntime::instance().hasCapability(id);
    };
    live.hasSchema = [](void*, std::uint64_t id) {
        return DynamicComponentStore::instance().schema(id) != nullptr;
    };

    ok &= check(real.requiredCapabilityCount > 0,
                "manifest capabilities populated from real package requirements",
                report);
    ok &= check(real.requiredSchemaCount > 0,
                "manifest schemas populated from real package registration", report);
    ok &= check(verifyGeneration(real, live) == VerifyFailure::None,
                "runtime-derived manifest verifies against the live registry",
                report);
    if (real.requiredCapabilityCount < kMaxVerifyRequirements) {
        real.requiredCapabilities[real.requiredCapabilityCount++] = 0xDEADBEEF;
        ok &= check(verifyGeneration(real, live) == VerifyFailure::MissingCapability,
                    "unknown capability requirement fails verify", report);
    }

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
