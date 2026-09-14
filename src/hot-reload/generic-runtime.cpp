// 09 13 2026
/* purpose
* Implements the kernel-side generic runtime registry.
* Does NOT own entity state, rendering, networking, or gameplay policy.
*/
#include "hot-reload/generic-runtime.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

#include "ecs/dynamic-components.h"
#include "hot-reload/hot-reload-system.h"

#include "project/capabilities.h"
#include "project/live-runtime.h"
#include "project/state-schema.h"

namespace MimitaRuntime {

GenericRuntime& GenericRuntime::instance()
{
    static GenericRuntime runtime;
    return runtime;
}

bool GenericRuntime::activate(const GamePackageDescriptorV1* package, std::string& error)
{
    if (!package) {
        // Null descriptor is allowed: it clears the generic registration while
        // keeping legacy module tables active.
        deactivate();
        return true;
    }
    if (package->abiVersion != MIMITA_PACKAGE_ABI_VERSION) {
        error = "package abi mismatch";
        return false;
    }
    if (package->structSize < sizeof(GamePackageDescriptorV1)) {
        error = "package descriptor too small";
        return false;
    }

    // Stage the registration first so a failure leaves the active package intact.
    std::vector<SystemEntry> systems;
    std::vector<CommandEntry> commands;
    std::unordered_map<std::string, std::size_t> commandIndex;
    std::vector<EventEntry> events;
    std::vector<SchemaEntry> schemas;
    std::vector<CapabilityEntry> providers;
    std::vector<std::uint64_t> requirements;

    if (package->systems) {
        for (std::uint32_t i = 0; i < package->systemCount; ++i) {
            const GameSystemDescriptorV1& s = package->systems[i];
            if (!s.invoke || s.id == 0) {
                error = "invalid system descriptor";
                return false;
            }
            SystemEntry e;
            e.id = s.id;
            e.domainId = s.domainId;
            e.priority = s.priority;
            e.invoke = s.invoke;
            e.name = s.name ? s.name : "";
            systems.push_back(std::move(e));
        }
    }
    std::sort(systems.begin(), systems.end(),
              [](const SystemEntry& a, const SystemEntry& b) {
                  if (a.domainId != b.domainId) return a.domainId < b.domainId;
                  if (a.priority != b.priority) return a.priority < b.priority;
                  return a.id < b.id;
              });

    if (package->commands) {
        for (std::uint32_t i = 0; i < package->commandCount; ++i) {
            const GameCommandDescriptorV1& c = package->commands[i];
            if (!c.name || !c.invoke) {
                error = "invalid command descriptor";
                return false;
            }
            CommandEntry e;
            e.name = c.name;
            e.usage = c.usage ? c.usage : "";
            e.invoke = c.invoke;
            if (commandIndex.find(e.name) != commandIndex.end()) {
                error = "duplicate command '" + e.name + "'";
                return false;
            }
            commandIndex[e.name] = commands.size();
            commands.push_back(std::move(e));
        }
    }

    if (package->eventTypes) {
        for (std::uint32_t i = 0; i < package->eventTypeCount; ++i) {
            const GameEventTypeDescriptorV1& ev = package->eventTypes[i];
            if (ev.id == 0)
                continue;
            EventEntry e;
            e.id = ev.id;
            e.schemaHash = ev.schemaHash;
            e.dispatch = ev.dispatch;
            e.name = ev.name ? ev.name : "";
            events.push_back(std::move(e));
        }
    }

    if (package->componentSchemas) {
        for (std::uint32_t i = 0; i < package->componentSchemaCount; ++i) {
            const GameComponentSchemaDescriptorV1& cs = package->componentSchemas[i];
            if (cs.id == 0)
                continue;
            SchemaEntry e;
            e.id = cs.id;
            e.size = cs.size;
            e.align = cs.align ? cs.align : 1;
            e.copyPolicy = cs.copyPolicy;
            e.name = cs.name ? cs.name : "";
            schemas.push_back(std::move(e));

            DynamicComponentSchema schema;
            schema.typeId = cs.id;
            schema.schemaHash = cs.schemaHash;
            schema.size = cs.size;
            schema.align = cs.align ? cs.align : 1;
            schema.copyPolicy = cs.copyPolicy;
            schema.networkPolicy = cs.networkPolicy;
            schema.name = cs.name ? cs.name : "";
            DynamicComponentStore::instance().registerSchema(schema);
        }
    }

    if (package->capabilityProviders) {
        for (std::uint32_t i = 0; i < package->capabilityProviderCount; ++i) {
            const GameCapabilityDescriptorV1& cap = package->capabilityProviders[i];
            CapabilityEntry e;
            e.id = cap.id;
            e.callable = cap.callable;
            e.name = cap.name ? cap.name : "";
            providers.push_back(std::move(e));
        }
    }
    if (package->capabilityRequirements) {
        for (std::uint32_t i = 0; i < package->capabilityRequirementCount; ++i)
            requirements.push_back(package->capabilityRequirements[i]);
    }

    // Resolve capability requirements: satisfied by a package provider or by a
    // kernel primitive capability. An unresolved requirement fails the candidate.
    for (std::uint64_t req : requirements) {
        bool provided = kernelProvidesCapability(req);
        if (!provided) {
            for (const CapabilityEntry& p : providers) {
                if (p.id == req) { provided = true; break; }
            }
        }
        if (!provided) {
            error = "missing capability provider id=" + std::to_string(req);
            return false;
        }
    }

    // Distinct domains declared by this package's systems.
    std::vector<std::uint64_t> domains;
    for (const SystemEntry& s : systems) {
        if (std::find(domains.begin(), domains.end(), s.domainId) == domains.end())
            domains.push_back(s.domainId);
    }

    // Commit atomically.
    systems_ = std::move(systems);
    commands_ = std::move(commands);
    commandIndex_ = std::move(commandIndex);
    events_ = std::move(events);
    schemas_ = std::move(schemas);
    capabilityProviders_ = std::move(providers);
    capabilityRequirements_ = std::move(requirements);
    domains_ = std::move(domains);
    packageId_ = package->packageId;
    logicalHash_ = package->logicalHash;
    packageName_ = package->name ? package->name : "";
    activePackage_ = package;

    // Wire the declared package metadata into the existing project-layer
    // registries so schemas and migrations are visible to tooling and are
    // available before new code observes state. This is the generic path that
    // replaces hand-registered, subsystem-specific state.
    for (const SchemaEntry& s : schemas_) {
        Project::ComponentLayout layout;
        layout.typeId = static_cast<std::uint32_t>(s.id);
        layout.version = 1;
        layout.name = s.name;
        layout.size = s.size;
        layout.alignment = s.align ? s.align : 1;
        Project::ComponentSchemaRegistry::instance().registerLayout(layout);
    }
    if (package->migrations) {
        for (std::uint32_t i = 0; i < package->migrationCount; ++i) {
            const GameMigrationDescriptorV1& m = package->migrations[i];
            if (!m.migrate || m.toVersion <= m.fromVersion)
                continue;
            Project::StateSchemaRegistry::instance().registerMigration(
                static_cast<std::uint32_t>(m.typeId), m.fromVersion, m.toVersion,
                reinterpret_cast<Project::MigrationFn>(m.migrate));
        }
    }

    // Register generic capability ids so a hot package can both provide and
    // require capabilities by id without extending the fixed enum.
    const std::string subject = packageName_.empty() ? "package" : packageName_;
    for (const CapabilityEntry& c : capabilityProviders_)
        Project::CapabilityRegistry::instance().provideId(subject, c.id);
    for (std::uint64_t required : capabilityRequirements_)
        Project::CapabilityRegistry::instance().requestId(subject, required);

    // One-time registration observability (never per frame).
    std::printf("[GENERIC_RUNTIME] %s\n", describe().c_str());
    for (const SystemEntry& s : systems_)
        std::printf("[SYSTEM_REGISTERED] system=%s id=%llu domain=%llu priority=%u\n",
                    s.name.c_str(), (unsigned long long)s.id,
                    (unsigned long long)s.domainId, s.priority);
    for (const EventEntry& e : events_)
        std::printf("[EVENT_REGISTERED] event=%s id=%llu\n",
                    e.name.c_str(), (unsigned long long)e.id);
    for (const CapabilityEntry& c : capabilityProviders_)
        std::printf("[CAPABILITY_RESOLVED] provider=%s id=%llu\n",
                    c.name.c_str(), (unsigned long long)c.id);
    std::fflush(stdout);
    return true;
}

void GenericRuntime::deactivate()
{
    std::printf("[GENERIC_RUNTIME] deactivated package=%s\n",
                packageName_.empty() ? "(none)" : packageName_.c_str());
    systems_.clear();
    commands_.clear();
    commandIndex_.clear();
    events_.clear();
    schemas_.clear();
    capabilityProviders_.clear();
    capabilityRequirements_.clear();
    domains_.clear();
    packageId_ = 0;
    logicalHash_ = 0;
    packageName_.clear();
    activePackage_ = nullptr;
}

bool GenericRuntime::runDomain(std::uint64_t domainId, std::uint64_t tick, float dt,
                               void* host)
{
    bool ran = false;
    for (const SystemEntry& s : systems_) {
        if (s.domainId != domainId)
            continue;
        s.invoke(host, tick, dt);
        ran = true;
    }
    return ran;
}

bool GenericRuntime::dispatchEvent(const GameEventV1& event, void* host)
{
    for (const EventEntry& e : events_) {
        if (e.id != event.typeId || !e.dispatch)
            continue;
        // Schema identity is advisory: 0 on either side means unspecified.
        if (event.schemaHash != 0 && e.schemaHash != 0 &&
            event.schemaHash != e.schemaHash)
            continue;
        e.dispatch(host, &event);
        return true;
    }
    return false;
}

bool GenericRuntime::runRegisteredDomains(std::uint64_t tick, float dt, void* host)
{
    bool ran = false;
    for (std::uint64_t domain : domains_) {
        if (domain == GAME_DOMAIN_GAMEPLAY || domain == GAME_DOMAIN_RENDER)
            continue;  // timed by the kernel
        if (runDomain(domain, tick, dt, host))
            ran = true;
    }
    return ran;
}

bool GenericRuntime::emit(std::uint64_t typeId, std::uint64_t sourceEntity,
                          std::uint64_t targetEntity, const void* payload,
                          std::uint32_t payloadSize, std::uint64_t tick, void* host)
{
    GameEventV1 event{};
    event.typeId = typeId;  // full 64-bit generic id, never truncated
    event.payloadVersion = 1;
    event.payloadSize = payloadSize;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.tick = tick;
    event.payload = const_cast<void*>(payload);
    return dispatchEvent(event, host);
}

void* GenericRuntime::capability(std::uint64_t id) const
{
    for (const CapabilityEntry& p : capabilityProviders_) {
        if (p.id == id)
            return p.callable;
    }
    return nullptr;
}

bool GenericRuntime::hasCapability(std::uint64_t id) const
{
    return capability(id) != nullptr || kernelProvidesCapability(id);
}

bool GenericRuntime::kernelProvidesCapability(std::uint64_t id) const
{
    static const std::uint64_t kKernelCapabilities[] = {
        gameHash("component.read"),
        gameHash("component.write"),
        gameHash("entity.find"),
        gameHash("physics.query"),
        gameHash("log.write"),
        gameHash("time.now"),
        gameHash("dynamic.component.read"),
        gameHash("dynamic.component.write"),
    };
    for (std::uint64_t cap : kKernelCapabilities) {
        if (cap == id)
            return true;
    }
    return false;
}

bool GenericRuntime::hasCommand(const std::string& name) const
{
    return commandIndex_.find(name) != commandIndex_.end();
}

bool GenericRuntime::runCommand(const std::string& name, const char* args, void* host) const
{
    auto it = commandIndex_.find(name);
    if (it == commandIndex_.end())
        return false;
    const CommandEntry& c = commands_[it->second];
    if (!c.invoke)
        return false;
    c.invoke(host, args ? args : "");
    return true;
}

std::uint64_t GenericRuntime::manifestHash() const
{
    std::vector<std::uint64_t> ids;
    ids.reserve(systems_.size() + events_.size() + schemas_.size() +
                capabilityProviders_.size() + capabilityRequirements_.size() +
                commands_.size() + 1);
    for (const SystemEntry& s : systems_) ids.push_back(s.id);
    for (const EventEntry& e : events_) ids.push_back(e.id);
    for (const SchemaEntry& s : schemas_) ids.push_back(s.id);
    for (const CapabilityEntry& c : capabilityProviders_) ids.push_back(c.id);
    for (std::uint64_t r : capabilityRequirements_) ids.push_back(r);
    for (const CommandEntry& c : commands_) ids.push_back(gameHash(c.name.c_str()));
    ids.push_back(packageId_);
    std::sort(ids.begin(), ids.end());
    std::uint64_t h = 1469598103934665603ull;
    for (std::uint64_t v : ids) {
        h ^= v;
        h *= 1099511628211ull;
    }
    return h;
}

void GenericRuntime::beginMovementTick()
{
    movementOverrideActive_ = false;
}

void GenericRuntime::requestMovementOverride(std::uint32_t flags,
                                             const float position[3],
                                             const float velocity[3], float yaw)
{
    movementOverrideActive_ = (flags & 1u) != 0;
    for (int i = 0; i < 3; ++i) {
        movementPosition_[i] = position ? position[i] : 0.0f;
        movementVelocity_[i] = velocity ? velocity[i] : 0.0f;
    }
    movementYaw_ = yaw;
}

bool GenericRuntime::consumeMovementOverride(float outPosition[3],
                                             float outVelocity[3], float& outYaw)
{
    if (!movementOverrideActive_)
        return false;
    for (int i = 0; i < 3; ++i) {
        outPosition[i] = movementPosition_[i];
        outVelocity[i] = movementVelocity_[i];
    }
    outYaw = movementYaw_;
    return true;
}

GameSharedStateV1* GenericRuntime::sharedState()
{
    GameMemory& memory = HotReloadSystem::instance().gameMemory();
    if (!memory.permanentStorage ||
        memory.permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    return reinterpret_cast<GameSharedStateV1*>(memory.permanentStorage);
}

std::string GenericRuntime::describe() const
{
    std::ostringstream o;
    o << "package=" << (packageName_.empty() ? "(none)" : packageName_)
      << " id=" << packageId_ << " logical=" << logicalHash_
      << " systems=" << systems_.size() << " commands=" << commands_.size()
      << " schemas=" << schemas_.size()
      << " providers=" << capabilityProviders_.size()
      << " requirements=" << capabilityRequirements_.size();
    return o.str();
}

} // namespace MimitaRuntime
