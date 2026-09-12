// 09 12 2026
/* purpose
* Implements the phases 4-6 primitives self-test.
* Does NOT own the systems it checks.
*/
#include "project/phase456-selftest.h"

#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "project/capabilities.h"
#include "project/live-runtime.h"
#include "project/project-types.h"
#include "project/project-watcher.h"
#include "project/world-hash.h"
#include "sim/domain-scheduler.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

void writeFile(const std::filesystem::path& path, const std::string& content)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

} // namespace

bool runPhase456SelfTest(std::string& report)
{
    bool ok = true;
    std::error_code error;

    // ── Multi-domain scheduler ─────────────────────────────────────
    {
        Sim::DomainScheduler scheduler;
        scheduler.clear();
        Sim::SimulationDomain& gameplay = scheduler.add("gameplay", 60.0);
        Sim::SimulationDomain& solver = scheduler.add("solver", 600.0);
        (void)gameplay;
        solver.maxCatchup = 20;  // allow the 600 Hz domain to catch up a full 1/60s
        int steps = 0;
        scheduler.advance(1.0 / 60.0, [&](Sim::SimulationDomain&) { ++steps; });
        ok &= check(steps >= 10, "multi-domain scheduler runs independent rates", report);
        ok &= check(scheduler.find("gameplay") && scheduler.find("solver") &&
                        solver.tickRateHz == 600.0,
                    "domain registration and lookup", report);
    }

    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) / "mimita_phase456_selftest";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "src", error);

    // ── Packages ───────────────────────────────────────────────────
    {
        writeFile(root / "packages" / "rockets" / "package.json",
                  "{\"name\":\"rockets\",\"abi\":1,\"sources\":[\"src/rocket.cpp\"],"
                  "\"behaviors\":[\"explosion\"],\"assets\":[\"assets/rocket.png\"]}");
        Project::BlobStore store(root / ".mimita" / "store");
        Project::PackageRegistry& packages = Project::PackageRegistry::instance();
        ok &= check(packages.scan(root, store) == 1 &&
                        packages.find("rockets") != nullptr &&
                        packages.find("rockets")->behaviors.size() == 1,
                    "tree package discovery", report);
    }

    // ── Resources ──────────────────────────────────────────────────
    {
        writeFile(root / "assets" / "rocket.png", "png-bytes");
        writeFile(root / "assets" / "shot.wav", "wav-bytes");
        Project::BlobStore store(root / ".mimita" / "store");
        Project::ResourceRegistry& resources = Project::ResourceRegistry::instance();
        const std::size_t count = resources.scan(root / "assets", store, {"png", "wav"});
        ok &= check(count == 2 && resources.find("rocket.png") != nullptr,
                    "content-addressed resource scan", report);
    }

    // ── Capabilities ───────────────────────────────────────────────
    {
        Project::CapabilityRegistry& capabilities = Project::CapabilityRegistry::instance();
        const Project::CapabilityMask requested =
            Project::capabilityBit(Project::Capability::ReadComponent) |
            Project::capabilityBit(Project::Capability::EmitEvent) |
            Project::capabilityBit(Project::Capability::Internet);
        capabilities.setRequested("rockets", requested);
        capabilities.grant("rockets",
                           Project::capabilityBit(Project::Capability::ReadComponent) |
                               Project::capabilityBit(Project::Capability::EmitEvent));
        ok &= check(capabilities.allows("rockets", Project::Capability::ReadComponent) &&
                        capabilities.allows("rockets", Project::Capability::EmitEvent) &&
                        !capabilities.allows("rockets", Project::Capability::Internet) &&
                        capabilities.denied("rockets") ==
                            Project::capabilityBit(Project::Capability::Internet),
                    "capability grant/deny model", report);
    }

    // ── Dependency verification ────────────────────────────────────
    {
        writeFile(root / "deps" / "libx-1.0.tar", "dependency-bytes");
        Project::DependencyResolver& resolver = Project::DependencyResolver::instance();
        resolver.registerLocal("libx", "1.0", root / "deps" / "libx-1.0.tar");

        Project::DependencyRequest verified;
        verified.name = "libx";
        verified.version = "1.0";
        verified.expected = Project::ContentId::fromFile((root / "deps" / "libx-1.0.tar").string());
        const Project::DependencyResult good = resolver.resolve(verified);

        Project::DependencyRequest mismatch = verified;
        mismatch.expected.digest = std::string(64, '0');
        const Project::DependencyResult bad = resolver.resolve(mismatch);

        Project::DependencyRequest missing;
        missing.name = "liby";
        missing.version = "1.0";
        const Project::DependencyResult absent = resolver.resolve(missing);

        ok &= check(good.status == Project::DependencyStatus::Verified &&
                        bad.status == Project::DependencyStatus::HashMismatch &&
                        absent.status == Project::DependencyStatus::NotLocal,
                    "dependency hash verification and trust states", report);
    }

    // ── Subsystem replacement lifecycle ────────────────────────────
    {
        Project::SubsystemSlot slot("renderer");
        const bool lifecycle =
            slot.beginInitialize() && slot.markMirrored() && slot.markValidated() &&
            slot.requestSwitch() && slot.activate(1234) && slot.retire();
        ok &= check(lifecycle && slot.activationTick() == 1234 &&
                        slot.state() == Project::SubsystemState::Retired,
                    "subsystem initialize/mirror/validate/switch/retire", report);
    }

    // ── Component schemas ──────────────────────────────────────────
    {
        Project::ComponentLayout layout;
        layout.typeId = 42;
        layout.version = 2;
        layout.name = "Metric";
        layout.size = 64;
        Project::ComponentSchemaRegistry::instance().registerLayout(layout);
        const Project::ComponentLayout* found =
            Project::ComponentSchemaRegistry::instance().find(42);
        ok &= check(found && found->size == 64 && found->version == 2,
                    "runtime component schema registry", report);
    }

    // ── World hashing determinism ──────────────────────────────────
    {
        EntityRegistry& registry = EntityRegistry::instance();
        registry.destroyAll();
        const EntityId player = registry.create(EntityRealm::Server, EntityDomain::Player, 1);
        TransformComponent transform;
        transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
        registry.add<TransformComponent>(player, transform);
        HealthComponent health;
        health.current = 100;
        health.max = 100;
        registry.add<HealthComponent>(player, health);

        const std::string first = Project::WorldHash::hashEntities(registry);
        const std::string again = Project::WorldHash::hashEntities(registry);
        health.current = 90;
        registry.add<HealthComponent>(player, health);
        const std::string changed = Project::WorldHash::hashEntities(registry);
        ok &= check(first == again && first != changed,
                    "world hash deterministic and sensitive to state", report);
        registry.destroyAll();
    }

    // ── Filesystem watcher ─────────────────────────────────────────
    {
        Project::ProjectWatcher watcher;
        const bool started = watcher.start(root);
        bool observed = false;
        if (started) {
            // Let the worker issue its first read before we write.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            writeFile(root / "src" / "watched.cpp", "int watched() { return 1; }\n");
            for (int attempt = 0; attempt < 60 && !observed; ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                observed = watcher.poll();
            }
            watcher.stop();
        }
        ok &= check(started && observed, "recursive filesystem watcher observes a change", report);
    }

    std::filesystem::remove_all(root, error);
    return ok;
}
