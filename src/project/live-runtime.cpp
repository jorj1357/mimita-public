// 09 12 2026
/* purpose
* Implements package discovery, resource scanning, and dependency hash checks.
* Does NOT download or build.
*/
#include "project/live-runtime.h"

#include <algorithm>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

namespace Project {

PackageRegistry& PackageRegistry::instance()
{
    static PackageRegistry registry;
    return registry;
}

std::size_t PackageRegistry::scan(const std::filesystem::path& root, BlobStore& store)
{
    packages_.clear();
    std::error_code error;
    if (!std::filesystem::exists(root, error))
        return 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
        if (error || !entry.is_regular_file())
            continue;
        if (entry.path().filename() != "package.json")
            continue;

        PackageManifest manifest;
        ContentId manifestId;
        if (!store.putFile(entry.path(), manifestId))
            continue;
        manifest.content = manifestId;

        try {
            std::ifstream in(entry.path());
            nlohmann::json json = nlohmann::json::parse(in);
            manifest.name = json.value("name", "");
            manifest.abi = json.value("abi", (std::uint32_t)1);
            if (json.contains("sources") && json["sources"].is_array())
                for (const auto& s : json["sources"])
                    manifest.sources.push_back(s.get<std::string>());
            if (json.contains("behaviors") && json["behaviors"].is_array())
                for (const auto& b : json["behaviors"])
                    manifest.behaviors.push_back(b.get<std::string>());
            if (json.contains("assets") && json["assets"].is_array())
                for (const auto& a : json["assets"])
                    manifest.assets.push_back(a.get<std::string>());
        } catch (...) {
            continue;
        }
        if (manifest.name.empty())
            continue;
        packages_.push_back(std::move(manifest));
    }

    std::sort(packages_.begin(), packages_.end(),
              [](const PackageManifest& a, const PackageManifest& b) {
                  return a.name < b.name;
              });
    return packages_.size();
}

const PackageManifest* PackageRegistry::find(const std::string& name) const
{
    for (const auto& package : packages_)
        if (package.name == name)
            return &package;
    return nullptr;
}

ResourceRegistry& ResourceRegistry::instance()
{
    static ResourceRegistry registry;
    return registry;
}

std::size_t ResourceRegistry::scan(const std::filesystem::path& root, BlobStore& store,
                                   const std::vector<std::string>& extensions)
{
    resources_.clear();
    std::set<std::string> wanted(extensions.begin(), extensions.end());
    std::error_code error;
    if (!std::filesystem::exists(root, error))
        return 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
        if (error || !entry.is_regular_file())
            continue;
        std::string ext = entry.path().extension().string();
        if (!ext.empty() && ext.front() == '.')
            ext.erase(ext.begin());
        if (!wanted.count(ext))
            continue;

        ResourceEntry resource;
        if (!store.putFile(entry.path(), resource.content))
            continue;
        resource.path = std::filesystem::relative(entry.path(), root, error).generic_string();
        resource.kind = ext;
        resources_.push_back(std::move(resource));
    }

    std::sort(resources_.begin(), resources_.end(),
              [](const ResourceEntry& a, const ResourceEntry& b) {
                  return a.path < b.path;
              });
    return resources_.size();
}

const ResourceEntry* ResourceRegistry::find(const std::string& path) const
{
    for (const auto& resource : resources_)
        if (resource.path == path)
            return &resource;
    return nullptr;
}

DependencyResolver& DependencyResolver::instance()
{
    static DependencyResolver resolver;
    return resolver;
}

void DependencyResolver::registerLocal(const std::string& name, const std::string& version,
                                       const std::filesystem::path& path)
{
    local_[name + "@" + version] = path;
}

DependencyResult DependencyResolver::resolve(const DependencyRequest& request) const
{
    DependencyResult result;
    if (request.name.empty()) {
        result.status = DependencyStatus::Rejected;
        result.detail = "empty dependency name";
        return result;
    }

    const std::string key = request.name + "@" + request.version;
    auto it = local_.find(key);
    std::filesystem::path artifact;
    if (it != local_.end()) {
        artifact = it->second;
    } else if (!request.source.empty() && std::filesystem::is_regular_file(request.source)) {
        artifact = request.source;
    } else {
        result.status = DependencyStatus::NotLocal;
        result.detail = "not present locally; download is a later phase";
        return result;
    }

    result.actual = ContentId::fromFile(artifact.string());
    if (!request.expected.valid()) {
        result.status = DependencyStatus::Verified;
        result.detail = "no expected hash supplied";
        return result;
    }
    if (result.actual.str() == request.expected.str()) {
        result.status = DependencyStatus::Verified;
        result.detail = "hash verified";
    } else {
        result.status = DependencyStatus::HashMismatch;
        result.detail = "hash mismatch";
    }
    return result;
}

SubsystemSlot::SubsystemSlot(std::string name)
    : name_(std::move(name))
{
}

bool SubsystemSlot::transition(SubsystemState expected, SubsystemState next, const char* label)
{
    if (state_ != expected)
        return false;
    state_ = next;
    lastTransition_ = label;
    return true;
}

bool SubsystemSlot::beginInitialize()
{
    return transition(SubsystemState::Inactive, SubsystemState::Initializing, "initializing");
}

bool SubsystemSlot::markMirrored()
{
    return transition(SubsystemState::Initializing, SubsystemState::Mirroring, "mirroring");
}

bool SubsystemSlot::markValidated()
{
    return transition(SubsystemState::Mirroring, SubsystemState::Validating, "validating");
}

bool SubsystemSlot::requestSwitch()
{
    return transition(SubsystemState::Validating, SubsystemState::SwitchPending, "switch_pending");
}

bool SubsystemSlot::activate(std::uint64_t tick)
{
    if (!transition(SubsystemState::SwitchPending, SubsystemState::Active, "active"))
        return false;
    activationTick_ = tick;
    return true;
}

bool SubsystemSlot::retire()
{
    if (state_ != SubsystemState::Active)
        return false;
    state_ = SubsystemState::Retired;
    lastTransition_ = "retired";
    return true;
}

ComponentSchemaRegistry& ComponentSchemaRegistry::instance()
{
    static ComponentSchemaRegistry registry;
    return registry;
}

void ComponentSchemaRegistry::registerLayout(ComponentLayout layout)
{
    if (layout.size == 0)
        layout.size = 1;
    if (layout.alignment == 0)
        layout.alignment = 1;
    layouts_[layout.typeId] = std::move(layout);
}

const ComponentLayout* ComponentSchemaRegistry::find(std::uint32_t typeId) const
{
    auto it = layouts_.find(typeId);
    return it == layouts_.end() ? nullptr : &it->second;
}

} // namespace Project
