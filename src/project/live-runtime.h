// 09 12 2026
/* purpose
* Live runtime support registries: tree-discovered packages, content-addressed
* resources/assets, and dependency acquisition with hash verification.
* Does NOT build code, load packages, or render.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "project/blob-store.h"
#include "project/project-types.h"

namespace Project {

// ── Packages ────────────────────────────────────────────────────
struct PackageManifest {
    std::string name;
    std::uint32_t abi = 1;
    std::vector<std::string> sources;
    std::vector<std::string> behaviors;
    std::vector<std::string> assets;
    ContentId content;
};

class PackageRegistry {
public:
    static PackageRegistry& instance();

    // Discovers package.json files under `root` (relative glob given by caller
    // via the project tree in later phases). Stores the manifest blob.
    std::size_t scan(const std::filesystem::path& root, BlobStore& store);

    const std::vector<PackageManifest>& packages() const { return packages_; }
    const PackageManifest* find(const std::string& name) const;
    std::size_t count() const { return packages_.size(); }
    void clear() { packages_.clear(); }

private:
    std::vector<PackageManifest> packages_;
};

// ── Resources / assets ──────────────────────────────────────────
struct ResourceEntry {
    std::string path;
    std::string kind;  // extension without dot
    ContentId content;
};

class ResourceRegistry {
public:
    static ResourceRegistry& instance();

    std::size_t scan(const std::filesystem::path& root, BlobStore& store,
                     const std::vector<std::string>& extensions);
    const std::vector<ResourceEntry>& resources() const { return resources_; }
    const ResourceEntry* find(const std::string& path) const;
    std::size_t count() const { return resources_.size(); }
    void clear() { resources_.clear(); }

private:
    std::vector<ResourceEntry> resources_;
};

// ── Dependency acquisition ──────────────────────────────────────
struct DependencyRequest {
    std::string name;
    std::string version;
    std::string source;   // url or local path
    ContentId expected;   // required bytes; empty = no verification possible
};

enum class DependencyStatus {
    Verified,      // hash matched a local/known content id
    NotLocal,      // needs download; no network in this phase
    HashMismatch,  // bytes present but wrong
    Rejected,      // malformed request
};

struct DependencyResult {
    DependencyStatus status = DependencyStatus::Rejected;
    std::string detail;
    ContentId actual;
};

class DependencyResolver {
public:
    static DependencyResolver& instance();

    // Registers an already-present local artifact (name/version/path).
    void registerLocal(const std::string& name, const std::string& version,
                       const std::filesystem::path& path);

    // Verifies a request against local artifacts. Download is a later phase;
    // this returns NotLocal when the artifact is absent.
    DependencyResult resolve(const DependencyRequest& request) const;

    std::size_t localCount() const { return local_.size(); }

private:
    std::map<std::string, std::filesystem::path> local_;  // "name@version" -> path
};

// ── Subsystem replacement lifecycle ─────────────────────────────
enum class SubsystemState {
    Inactive,
    Initializing,
    Mirroring,
    Validating,
    SwitchPending,
    Active,
    Retired,
};

inline const char* subsystemStateName(SubsystemState state)
{
    switch (state) {
    case SubsystemState::Inactive: return "inactive";
    case SubsystemState::Initializing: return "initializing";
    case SubsystemState::Mirroring: return "mirroring";
    case SubsystemState::Validating: return "validating";
    case SubsystemState::SwitchPending: return "switch_pending";
    case SubsystemState::Active: return "active";
    case SubsystemState::Retired: return "retired";
    }
    return "unknown";
}

// A slot a large system (renderer, ragdoll solver, authority model) can be
// replaced in without restarting: initialize the candidate alongside, mirror
// state/resources, validate, switch at a safe tick, retire the old one later.
class SubsystemSlot {
public:
    explicit SubsystemSlot(std::string name);

    const std::string& name() const { return name_; }
    SubsystemState state() const { return state_; }
    std::uint64_t activationTick() const { return activationTick_; }
    const std::string& lastTransition() const { return lastTransition_; }

    bool beginInitialize();
    bool markMirrored();
    bool markValidated();
    bool requestSwitch();
    bool activate(std::uint64_t tick);
    bool retire();

private:
    bool transition(SubsystemState expected, SubsystemState next, const char* label);

    std::string name_;
    SubsystemState state_ = SubsystemState::Inactive;
    std::uint64_t activationTick_ = 0;
    std::string lastTransition_ = "inactive";
};

// ── Runtime component schemas (layout/version) ──────────────────
struct ComponentLayout {
    std::uint32_t typeId = 0;
    std::uint32_t version = 1;
    std::string name;
    std::uint32_t size = 0;
    std::uint32_t alignment = 1;
};

class ComponentSchemaRegistry {
public:
    static ComponentSchemaRegistry& instance();

    void registerLayout(ComponentLayout layout);
    const ComponentLayout* find(std::uint32_t typeId) const;
    std::size_t count() const { return layouts_.size(); }

private:
    std::map<std::uint32_t, ComponentLayout> layouts_;
};

} // namespace Project
