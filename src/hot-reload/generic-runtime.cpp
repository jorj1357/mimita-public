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

bool GenericRuntime::activate(const GamePackageDescriptorV1* package, std::string& error,
                              std::uint32_t generation)
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
    std::vector<ModeEntry> modes;
    std::unordered_map<std::uint64_t, std::size_t> modeIndex;
    std::vector<std::uint64_t> modeDomains;
    std::vector<SchemaEntry> schemas;
    std::vector<DynamicComponentSchema> dynamicSchemas;
    std::vector<CapabilityEntry> providers;
    std::vector<CapabilityRequirement> requirements;

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
            e.domainId = ev.domainId;
            e.dispatch = ev.dispatch;
            e.name = ev.name ? ev.name : "";
            events.push_back(std::move(e));
        }
    }

    if (package->modes) {
        for (std::uint32_t i = 0; i < package->modeCount; ++i) {
            const GameModeDescriptorV1& m = package->modes[i];
            if (m.id == 0 || m.domainId == 0) {
                error = "invalid mode descriptor";
                return false;
            }
            if (modeIndex.find(m.id) != modeIndex.end()) {
                error = "duplicate mode id=" + std::to_string(m.id);
                return false;
            }
            ModeEntry e;
            e.id = m.id;
            e.domainId = m.domainId;
            e.matchSchemaId = m.matchSchemaId;
            e.matchSchemaHash = m.matchSchemaHash;
            e.displayName = m.displayName ? m.displayName : "";
            modeIndex[m.id] = modes.size();
            if (std::find(modeDomains.begin(), modeDomains.end(), m.domainId) ==
                modeDomains.end())
                modeDomains.push_back(m.domainId);
            modes.push_back(std::move(e));
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

            // Stage; the dynamic store is updated atomically at commit so a
            // rejected candidate leaves the previous schemas and bytes intact.
            DynamicComponentSchema schema;
            schema.typeId = cs.id;
            schema.schemaHash = cs.schemaHash;
            schema.version = cs.version ? cs.version : 1;
            schema.size = cs.size;
            schema.align = cs.align ? cs.align : 1;
            schema.copyPolicy = cs.copyPolicy;
            schema.networkPolicy = cs.networkPolicy;
            schema.name = cs.name ? cs.name : "";
            dynamicSchemas.push_back(std::move(schema));
        }
    }

    if (package->capabilityProviders) {
        for (std::uint32_t i = 0; i < package->capabilityProviderCount; ++i) {
            const GameCapabilityDescriptorV1& cap = package->capabilityProviders[i];
            if (cap.id == 0)
                continue;
            for (const CapabilityEntry& existing : providers) {
                if (existing.id == cap.id) {
                    error = "duplicate capability provider id=" + std::to_string(cap.id);
                    return false;
                }
            }
            CapabilityEntry e;
            e.id = cap.id;
            e.signatureId = cap.signatureId;
            e.schemaHash = cap.schemaHash;
            e.callable = cap.callable;
            e.providerPackage = package->packageId;
            e.providerGeneration = generation;
            e.name = cap.name ? cap.name : "";
            providers.push_back(std::move(e));
        }
    }
    if (package->capabilityRequirements) {
        for (std::uint32_t i = 0; i < package->capabilityRequirementCount; ++i) {
            const GameCapabilityRequirementV1& r = package->capabilityRequirements[i];
            CapabilityRequirement req;
            req.id = r.id;
            req.signatureId = r.signatureId;
            req.schemaHash = r.schemaHash;
            requirements.push_back(req);
        }
    }

    // Resolve capability requirements generically: satisfied by a staged package
    // provider or by a kernel primitive, with a compatible signature. The kernel
    // only knows ids + signatures here; it never switches on a concept name.
    for (const CapabilityRequirement& req : requirements) {
        const CapabilityEntry* provider = nullptr;
        for (const CapabilityEntry& p : providers) {
            if (p.id == req.id) { provider = &p; break; }
        }
        if (!provider) {
            for (const CapabilityEntry& p : kernelCapabilities_) {
                if (p.id == req.id) { provider = &p; break; }
            }
        }
        if (!provider) {
            error = "missing capability provider id=" + std::to_string(req.id);
            return false;
        }
        if (req.signatureId != 0 && provider->signatureId != 0 &&
            req.signatureId != provider->signatureId) {
            error = "capability signature mismatch id=" + std::to_string(req.id);
            return false;
        }
        if (req.schemaHash != 0 && provider->schemaHash != 0 &&
            req.schemaHash != provider->schemaHash) {
            error = "capability schema mismatch id=" + std::to_string(req.id);
            return false;
        }
    }

    // Distinct domains declared by this package's systems.
    std::vector<std::uint64_t> domains;
    for (const SystemEntry& s : systems) {
        if (std::find(domains.begin(), domains.end(), s.domainId) == domains.end())
            domains.push_back(s.domainId);
    }

    // Route migrations. A migration whose type id is one of this package's
    // dynamic component schemas is owned by the dynamic store (64-bit keyed);
    // everything else stays with the project typed-state registry. Registering
    // migrations is additive and harmless if the migration then fails.
    if (package->migrations) {
        for (std::uint32_t i = 0; i < package->migrationCount; ++i) {
            const GameMigrationDescriptorV1& m = package->migrations[i];
            if (!m.migrate || m.toVersion <= m.fromVersion)
                continue;
            bool isDynamic = false;
            for (const DynamicComponentSchema& s : dynamicSchemas) {
                if (s.typeId == m.typeId) { isDynamic = true; break; }
            }
            if (isDynamic) {
                DynamicComponentStore::instance().registerMigration(
                    m.typeId, m.fromVersion, m.toVersion,
                    reinterpret_cast<DynamicMigrationFn>(m.migrate));
            } else {
                Project::StateSchemaRegistry::instance().registerMigration(
                    static_cast<std::uint32_t>(m.typeId), m.fromVersion, m.toVersion,
                    reinterpret_cast<Project::MigrationFn>(m.migrate));
            }
        }
    }

    // Apply the staged dynamic schemas. If any stored blob cannot be migrated
    // to its new version, the candidate is rejected here, before any runtime
    // registration is committed, so old code and old state both survive.
    if (!DynamicComponentStore::instance().applySchemaUpdate(dynamicSchemas, error))
        return false;

    // Commit atomically.
    systems_ = std::move(systems);
    commands_ = std::move(commands);
    commandIndex_ = std::move(commandIndex);
    events_ = std::move(events);
    modes_ = std::move(modes);
    modeIndex_ = std::move(modeIndex);
    modeDomains_ = std::move(modeDomains);
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

    // Register generic capability ids so a hot package can both provide and
    // require capabilities by id without extending the fixed enum.
    const std::string subject = packageName_.empty() ? "package" : packageName_;
    for (const CapabilityEntry& c : capabilityProviders_)
        Project::CapabilityRegistry::instance().provideId(subject, c.id);
    for (const CapabilityRequirement& required : capabilityRequirements_)
        Project::CapabilityRegistry::instance().requestId(subject, required.id);

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
    modes_.clear();
    modeIndex_.clear();
    modeDomains_.clear();
    activeModeDomain_ = 0;
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
        // Mode-scoped handlers run only while their mode domain is active.
        if (e.domainId != 0 && e.domainId != activeModeDomain_)
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
        if (std::find(modeDomains_.begin(), modeDomains_.end(), domain) !=
            modeDomains_.end())
            continue;  // mode domains run only while their mode is active
        if (runDomain(domain, tick, dt, host))
            ran = true;
    }
    return ran;
}

bool GenericRuntime::hasMode(std::uint64_t modeId) const
{
    return modeIndex_.find(modeId) != modeIndex_.end();
}

std::uint64_t GenericRuntime::modeDomain(std::uint64_t modeId) const
{
    auto it = modeIndex_.find(modeId);
    return it == modeIndex_.end() ? 0 : modes_[it->second].domainId;
}

const char* GenericRuntime::modeDisplayName(std::uint64_t modeId) const
{
    auto it = modeIndex_.find(modeId);
    return it == modeIndex_.end() ? "" : modes_[it->second].displayName.c_str();
}

std::uint64_t GenericRuntime::modeIdAt(std::size_t index) const
{
    return index < modes_.size() ? modes_[index].id : 0;
}

void GenericRuntime::setActiveModeDomain(std::uint64_t domainId)
{
    activeModeDomain_ = domainId;
}

bool GenericRuntime::runActiveModeDomain(std::uint64_t tick, float dt, void* host)
{
    if (activeModeDomain_ == 0)
        return false;
    return runDomain(activeModeDomain_, tick, dt, host);
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
    for (const CapabilityEntry& p : kernelCapabilities_) {
        if (p.id == id)
            return p.callable;
    }
    for (const CapabilityEntry& p : capabilityProviders_) {
        if (p.id == id)
            return p.callable;
    }
    return nullptr;
}

bool GenericRuntime::hasCapability(std::uint64_t id) const
{
    return capability(id) != nullptr;
}

bool GenericRuntime::kernelProvidesCapability(std::uint64_t id) const
{
    for (const CapabilityEntry& p : kernelCapabilities_) {
        if (p.id == id)
            return true;
    }
    return false;
}

void GenericRuntime::registerKernelCapability(std::uint64_t id,
                                              std::uint64_t signatureId,
                                              std::uint64_t schemaHash,
                                              void* callable,
                                              const char* name)
{
    if (id == 0 || !callable)
        return;
    for (CapabilityEntry& existing : kernelCapabilities_) {
        if (existing.id == id) {
            // Idempotent re-registration (e.g. selftest re-init).
            existing.signatureId = signatureId;
            existing.schemaHash = schemaHash;
            existing.callable = callable;
            existing.name = name ? name : "";
            return;
        }
    }
    CapabilityEntry e;
    e.id = id;
    e.signatureId = signatureId;
    e.schemaHash = schemaHash;
    e.callable = callable;
    e.providerPackage = 0;
    e.providerGeneration = 0;
    e.name = name ? name : "";
    kernelCapabilities_.push_back(std::move(e));
}

bool GenericRuntime::capabilityInfo(std::uint64_t id, std::uint64_t* outSignatureId,
                                    std::uint64_t* outSchemaHash,
                                    std::uint64_t* outProviderPackage,
                                    std::uint32_t* outProviderGeneration) const
{
    for (const CapabilityEntry& p : kernelCapabilities_) {
        if (p.id != id) continue;
        if (outSignatureId) *outSignatureId = p.signatureId;
        if (outSchemaHash) *outSchemaHash = p.schemaHash;
        if (outProviderPackage) *outProviderPackage = p.providerPackage;
        if (outProviderGeneration) *outProviderGeneration = p.providerGeneration;
        return true;
    }
    for (const CapabilityEntry& p : capabilityProviders_) {
        if (p.id != id) continue;
        if (outSignatureId) *outSignatureId = p.signatureId;
        if (outSchemaHash) *outSchemaHash = p.schemaHash;
        if (outProviderPackage) *outProviderPackage = p.providerPackage;
        if (outProviderGeneration) *outProviderGeneration = p.providerGeneration;
        return true;
    }
    return false;
}

std::uint32_t GenericRuntime::capabilityProviderGeneration(std::uint64_t id) const
{
    std::uint32_t generation = 0;
    capabilityInfo(id, nullptr, nullptr, nullptr, &generation);
    return generation;
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
    for (const CapabilityRequirement& r : capabilityRequirements_) ids.push_back(r.id);
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
