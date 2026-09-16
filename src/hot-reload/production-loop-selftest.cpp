// 09 15 2026
/* purpose
* Production-loop F -> G self-test. Composes the REAL pieces without shortcuts:
* HotReloadSystem startup (loads the actual hot package), a real generation
* manifest, verifyGeneration on real local facts, prepareMigration against live
* stored state, installCandidateArtifact (the same loader path), the real switch
* transaction at an explicit tick, and generation publication. Then asserts that
* persistent world/entity state survived the generation change unchanged — the
* core product invariant "same world, new code".
*/
#include "hot-reload/production-loop-selftest.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "hot-reload/artifact-cache.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generation-verify.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/migration-prep.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::vector<unsigned char> readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
}

} // namespace

bool runProductionLoopSelfTest(std::string& report)
{
    bool ok = true;

    HotReloadSystem& hrs = HotReloadSystem::instance();
    hrs.startup();
    const std::uint32_t F = hrs.status().activeGeneration;
    ok &= check(F != 0, "F is active (real hot package loaded)", report);

    // ── Persistent world state that must survive F -> G ───────────────
    DynamicComponentStore& store = DynamicComponentStore::instance();
    store.clear();
    const std::uint64_t typeId = 0x9001;  // a project component, not in the package
    DynamicComponentSchema s;
    s.typeId = typeId;
    s.version = 1;
    s.size = sizeof(int);
    s.align = 4;
    store.registerSchema(s);
    const EntityId entity = static_cast<EntityId>(0x5001);
    int value = 42;
    store.write(entity, typeId, &value, sizeof(value));

    // ── Real candidate artifact bytes (same file a local build stages) ─
    const std::vector<unsigned char> bytes = readFile("build/mimita-game.dll");
    if (bytes.empty()) {
        report += "[info] build/mimita-game.dll missing; production loop needs a "
                  "built hot package\n";
        return ok;
    }
    const std::uint64_t hash =
        hashArtifactBytes(bytes.data(), bytes.size());
    const std::uint32_t G = F + 1;

    // ── Server-side manifest + client-side verify against real facts ───
    GenerationManifestV1 manifest{};
    manifest.logicalGenerationId = G;
    manifest.platformArtifactHash = hash;
    manifest.platformArtifactSize = (std::uint32_t)bytes.size();
    manifest.hotAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;

    GenerationLocalFactsV1 facts{};
    facts.artifactHash = hash;
    facts.artifactSize = (std::uint32_t)bytes.size();
    facts.coldAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
    ok &= check(verifyGeneration(manifest, facts) == VerifyFailure::None,
                "candidate manifest verifies against real local facts", report);

    // ── Migration preparation against live state (no-op here) ──────────
    MigrationPrepareFactsV1 mf{};
    mf.candidateSchemaCount = manifest.requiredSchemaCount;
    for (std::uint32_t i = 0; i < manifest.requiredSchemaCount; ++i) {
        mf.candidateSchemaIds[i] = manifest.requiredSchemas[i];
        mf.candidateSchemaVersions[i] = manifest.requiredSchemaVersions[i];
    }
    mf.storedSchemaVersion = [](void*, std::uint64_t id) {
        return DynamicComponentStore::instance().maxStoredVersion(id);
    };
    mf.hasMigrationPath = [](void*, std::uint64_t id, std::uint32_t from,
                             std::uint32_t to) {
        return DynamicComponentStore::instance().hasMigration(id, from, to);
    };
    const MigrationPlanV1 plan = prepareMigration(F, G, mf);
    ok &= check(plan.outcome != MigrationOutcome::Failed,
                "migration preparation succeeds (no-op)", report);
    hrs.setCandidateMigrationPlan(plan);

    // ── Real install through the same loader path ──────────────────────
    std::string err;
    ok &= check(hrs.installCandidateArtifact(bytes, G, hash, err) &&
                    hrs.hasInstalledCandidate() &&
                    hrs.installedCandidateGeneration() == G,
                "artifact installed as a real inactive candidate", report);

    // ── Real coordinated switch + activation transaction ───────────────
    hrs.requestSwitchAtTick(1000);
    bool activated = false;
    for (std::uint32_t t = 1000; t < 1010 && !activated; ++t)
        activated = hrs.pollAndAdvance(t);
    const std::uint32_t activeAfter = hrs.status().activeGeneration;
    ok &= check(activated && activeAfter == G &&
                    hrs.lastSwitchRejection() == SwitchRejection::None,
                "F -> G activated through the real switch transaction", report);

    // ── Session/world continuity: identity + state survive ─────────────
    int out = 0;
    const bool survived = store.read(entity, typeId, &out, sizeof(out)) && out == 42;
    ok &= check(survived,
                "persistent entity/component survived the generation change",
                report);
    report += "  [info] F=" + std::to_string(F) + " G=" + std::to_string(G) +
              " activeAfter=" + std::to_string(activeAfter) + "\n";

    return ok;
}
