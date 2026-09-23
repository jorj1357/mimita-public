// 09 14 2026
/* purpose
* DLL-side aggregation for the generic package descriptor. Each hot source file
* self-registers the systems/events/schemas/commands/capabilities it provides,
* so adding a new file under src/hot-reload/modules/ creates a new runtime
* concept without editing an aggregator or the EXE. This is the general
* discovery mechanism behind the "no new slot per subsystem" rule.
* Does NOT own entity state, rendering, or gameplay policy.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>
#include <string>
#include <vector>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-packet-codec.h"

// Hot-side (DLL-internal) composition tables. A tool or projectile is an
// entity/composition; these tables let any module add a behavior for a runtime
// key without a kernel enum, switch, or ABI change. The kernel emits one
// generic fact per tool-use / projectile-impact; a single router handler in the
// DLL dispatches to the registered behavior.
using HotToolUseFn = void (MIMITA_GAME_CALL *)(const ToolUsePolicyV1* use,
                                               GameplayContextV1* context);
using HotProjectileImpactFn = void (MIMITA_GAME_CALL *)(
    const ProjectileImpactPolicyV1* impact, GameplayContextV1* context);

class HotPackageBuilder {
public:
    static HotPackageBuilder& instance()
    {
        static HotPackageBuilder builder;
        return builder;
    }

    void addToolBehavior(std::uint64_t toolId, HotToolUseFn fn)
    {
        toolBehaviors_.push_back({toolId, fn});
    }
    // Shared behavior family: many tool definitions may name one behaviorId.
    // The router prefers this table over the per-tool table.
    void addBehavior(std::uint64_t behaviorId, HotToolUseFn fn)
    {
        behaviors_.push_back({behaviorId, fn});
    }
    void addProjectileBehavior(std::uint64_t typeId, HotProjectileImpactFn fn)
    {
        projectileBehaviors_.push_back({typeId, fn});
    }
    HotToolUseFn findToolBehavior(std::uint64_t toolId) const
    {
        for (const auto& entry : toolBehaviors_)
            if (entry.id == toolId)
                return entry.fn;
        return nullptr;
    }
    HotToolUseFn findBehavior(std::uint64_t behaviorId) const
    {
        for (const auto& entry : behaviors_)
            if (entry.id == behaviorId)
                return entry.fn;
        return nullptr;
    }
    HotProjectileImpactFn findProjectileBehavior(std::uint64_t typeId) const
    {
        for (const auto& entry : projectileBehaviors_)
            if (entry.id == typeId)
                return entry.fn;
        return nullptr;
    }

    void addPacketCodec(const MimitaNet::GamePacketCodecDescriptorV1& c)
    {
        packetCodecs_.push_back(c);
    }
    // Version-range lookup: prefer a codec that can decode `version`
    // ([minSupportedVersion, schemaVersion]); otherwise return the newest codec
    // for the schema so the dispatcher can report too-old/too-new explicitly.
    const MimitaNet::GamePacketCodecDescriptorV1* findPacketCodec(
        std::uint64_t schemaId, std::uint32_t version) const
    {
        const MimitaNet::GamePacketCodecDescriptorV1* newest = nullptr;
        const MimitaNet::GamePacketCodecDescriptorV1* inRange = nullptr;
        for (const auto& c : packetCodecs_) {
            if (c.schemaId != schemaId)
                continue;
            if (!newest || c.schemaVersion > newest->schemaVersion)
                newest = &c;
            if (version >= c.minSupportedVersion && version <= c.schemaVersion) {
                if (!inRange || c.schemaVersion > inRange->schemaVersion)
                    inRange = &c;
            }
        }
        return inRange ? inRange : newest;
    }
    std::size_t packetCodecCount() const { return packetCodecs_.size(); }

    void addSystem(const GameSystemDescriptorV1& s) { systems_.push_back(s); }
    void addEventType(const GameEventTypeDescriptorV1& e) { events_.push_back(e); }
    void addSchema(const GameComponentSchemaDescriptorV1& s) { schemas_.push_back(s); }
    void addCapabilityProvider(const GameCapabilityDescriptorV1& c) { providers_.push_back(c); }
    void addCapabilityRequirement(std::uint64_t id, std::uint64_t signatureId = 0,
                                  std::uint64_t schemaHash = 0)
    {
        requirements_.push_back(GameCapabilityRequirementV1{id, signatureId, schemaHash});
    }
    void addCommand(const GameCommandDescriptorV1& c) { commands_.push_back(c); }
    void addResource(const GameResourceDescriptorV1& r) { resources_.push_back(r); }
    void addMigration(const GameMigrationDescriptorV1& m) { migrations_.push_back(m); }
    void addMode(const GameModeDescriptorV1& m) { modes_.push_back(m); }

    std::size_t systemCount() const { return systems_.size(); }

    // The descriptor points at stable per-instance vectors. It is valid for the
    // lifetime of the loaded DLL generation, which is all the kernel requires.
    const GamePackageDescriptorV1* descriptor()
    {
        descriptor_ = GamePackageDescriptorV1{};
        descriptor_.structSize = sizeof(GamePackageDescriptorV1);
        descriptor_.abiVersion = MIMITA_PACKAGE_ABI_VERSION;
        descriptor_.packageId = packageId_;
        descriptor_.logicalHash = logicalHash_;
        descriptor_.name = packageName_.c_str();
        descriptor_.systems = systems_.empty() ? nullptr : systems_.data();
        descriptor_.systemCount = (std::uint32_t)systems_.size();
        descriptor_.eventTypes = events_.empty() ? nullptr : events_.data();
        descriptor_.eventTypeCount = (std::uint32_t)events_.size();
        descriptor_.componentSchemas = schemas_.empty() ? nullptr : schemas_.data();
        descriptor_.componentSchemaCount = (std::uint32_t)schemas_.size();
        descriptor_.capabilityProviders = providers_.empty() ? nullptr : providers_.data();
        descriptor_.capabilityProviderCount = (std::uint32_t)providers_.size();
        descriptor_.capabilityRequirements =
            requirements_.empty() ? nullptr : requirements_.data();
        descriptor_.capabilityRequirementCount = (std::uint32_t)requirements_.size();
        descriptor_.commands = commands_.empty() ? nullptr : commands_.data();
        descriptor_.commandCount = (std::uint32_t)commands_.size();
        descriptor_.resources = resources_.empty() ? nullptr : resources_.data();
        descriptor_.resourceCount = (std::uint32_t)resources_.size();
        descriptor_.migrations = migrations_.empty() ? nullptr : migrations_.data();
        descriptor_.migrationCount = (std::uint32_t)migrations_.size();
        descriptor_.modes = modes_.empty() ? nullptr : modes_.data();
        descriptor_.modeCount = (std::uint32_t)modes_.size();
        return &descriptor_;
    }

    void setPackage(std::uint64_t id, std::uint64_t logicalHash, const char* name)
    {
        packageId_ = id;
        logicalHash_ = logicalHash;
        packageName_ = name ? name : "";
    }

private:
    HotPackageBuilder() = default;

    std::vector<GameSystemDescriptorV1> systems_;
    std::vector<MimitaNet::GamePacketCodecDescriptorV1> packetCodecs_;
    std::vector<GameEventTypeDescriptorV1> events_;
    std::vector<GameComponentSchemaDescriptorV1> schemas_;
    std::vector<GameCapabilityDescriptorV1> providers_;
    std::vector<GameCapabilityRequirementV1> requirements_;
    std::vector<GameCommandDescriptorV1> commands_;
    std::vector<GameResourceDescriptorV1> resources_;
    std::vector<GameMigrationDescriptorV1> migrations_;
    std::vector<GameModeDescriptorV1> modes_;
    struct ToolBehaviorEntry { std::uint64_t id; HotToolUseFn fn; };
    struct ProjectileBehaviorEntry { std::uint64_t id; HotProjectileImpactFn fn; };
    std::vector<ToolBehaviorEntry> toolBehaviors_;
    std::vector<ToolBehaviorEntry> behaviors_;
    std::vector<ProjectileBehaviorEntry> projectileBehaviors_;
    GamePackageDescriptorV1 descriptor_{};
    std::uint64_t packageId_ = gameHash("mimita.hot.package");
    std::uint64_t logicalHash_ = gameHash("mimita.hot.package.v1");
    std::string packageName_ = "mimita.hot.package";
};

// Self-registration helper. Usage in any hot source file:
//   MIMITA_REGISTER_SYSTEM({gameHash("my.system"), GAME_DOMAIN_GAMEPLAY, 10, 0,
//                           myTick, "my.system"});
namespace MimitaHotPackage {
struct SystemRegistrar {
    explicit SystemRegistrar(const GameSystemDescriptorV1& s)
    {
        HotPackageBuilder::instance().addSystem(s);
    }
};
struct EventRegistrar {
    explicit EventRegistrar(const GameEventTypeDescriptorV1& e)
    {
        HotPackageBuilder::instance().addEventType(e);
    }
};
struct SchemaRegistrar {
    explicit SchemaRegistrar(const GameComponentSchemaDescriptorV1& s)
    {
        HotPackageBuilder::instance().addSchema(s);
    }
};
// Declares an activation-time migration for a dynamic component schema
// (v1 -> v2). `migrate` is a DynamicMigrationFn-compatible function pointer.
struct MigrationRegistrar {
    MigrationRegistrar(std::uint64_t typeId, std::uint32_t fromVersion,
                       std::uint32_t toVersion, void* migrate)
    {
        HotPackageBuilder::instance().addMigration(
            GameMigrationDescriptorV1{typeId, fromVersion, toVersion, migrate});
    }
};
struct CapabilityRegistrar {
    explicit CapabilityRegistrar(const GameCapabilityDescriptorV1& c)
    {
        HotPackageBuilder::instance().addCapabilityProvider(c);
    }
};
// Declares a capability requirement (id + expected signature) without adding a
// provider. A package can require a capability another package/kernel provides.
struct CapabilityRequirementRegistrar {
    explicit CapabilityRequirementRegistrar(std::uint64_t id,
                                            std::uint64_t signatureId = 0,
                                            std::uint64_t schemaHash = 0)
    {
        HotPackageBuilder::instance().addCapabilityRequirement(id, signatureId, schemaHash);
    }
};
struct CommandRegistrar {
    explicit CommandRegistrar(const GameCommandDescriptorV1& c)
    {
        HotPackageBuilder::instance().addCommand(c);
    }
};
struct ModeRegistrar {
    explicit ModeRegistrar(const GameModeDescriptorV1& m)
    {
        HotPackageBuilder::instance().addMode(m);
    }
};
struct ToolBehaviorRegistrar {
    ToolBehaviorRegistrar(std::uint64_t toolId, HotToolUseFn fn)
    {
        HotPackageBuilder::instance().addToolBehavior(toolId, fn);
    }
};
// Registers a shared behavior family keyed by behaviorId (see
// hot-tool-visual.h TOOL_BEHAVIOR_*). A tool definition names this id.
struct BehaviorIdRegistrar {
    BehaviorIdRegistrar(std::uint64_t behaviorId, HotToolUseFn fn)
    {
        HotPackageBuilder::instance().addBehavior(behaviorId, fn);
    }
};
struct ProjectileBehaviorRegistrar {
    ProjectileBehaviorRegistrar(std::uint64_t typeId, HotProjectileImpactFn fn)
    {
        HotPackageBuilder::instance().addProjectileBehavior(typeId, fn);
    }
};
// Registers a packet codec for (schemaId, schemaVersion). Adding a new packet
// schema is a hot source edit: register a codec here and provide the
// net.packet-codecs lookup capability; no EXE call site changes.
struct PacketCodecRegistrar {
    explicit PacketCodecRegistrar(const MimitaNet::GamePacketCodecDescriptorV1& c)
    {
        HotPackageBuilder::instance().addPacketCodec(c);
    }
};
} // namespace MimitaHotPackage

#endif // MIMITA_GAME_DLL
