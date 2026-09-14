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

class HotPackageBuilder {
public:
    static HotPackageBuilder& instance()
    {
        static HotPackageBuilder builder;
        return builder;
    }

    void addSystem(const GameSystemDescriptorV1& s) { systems_.push_back(s); }
    void addEventType(const GameEventTypeDescriptorV1& e) { events_.push_back(e); }
    void addSchema(const GameComponentSchemaDescriptorV1& s) { schemas_.push_back(s); }
    void addCapabilityProvider(const GameCapabilityDescriptorV1& c) { providers_.push_back(c); }
    void addCapabilityRequirement(std::uint64_t id) { requirements_.push_back(id); }
    void addCommand(const GameCommandDescriptorV1& c) { commands_.push_back(c); }
    void addResource(const GameResourceDescriptorV1& r) { resources_.push_back(r); }
    void addMigration(const GameMigrationDescriptorV1& m) { migrations_.push_back(m); }

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
    std::vector<GameEventTypeDescriptorV1> events_;
    std::vector<GameComponentSchemaDescriptorV1> schemas_;
    std::vector<GameCapabilityDescriptorV1> providers_;
    std::vector<std::uint64_t> requirements_;
    std::vector<GameCommandDescriptorV1> commands_;
    std::vector<GameResourceDescriptorV1> resources_;
    std::vector<GameMigrationDescriptorV1> migrations_;
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
struct CapabilityRegistrar {
    explicit CapabilityRegistrar(const GameCapabilityDescriptorV1& c)
    {
        HotPackageBuilder::instance().addCapabilityProvider(c);
    }
};
struct CommandRegistrar {
    explicit CommandRegistrar(const GameCommandDescriptorV1& c)
    {
        HotPackageBuilder::instance().addCommand(c);
    }
};
} // namespace MimitaHotPackage

#endif // MIMITA_GAME_DLL
