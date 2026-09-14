// 09 13 2026
/* purpose
* Kernel-side generic runtime registry: owns the active package descriptor and
* dispatches its systems, events, commands, component schemas, capabilities, and
* resources by id/hash. One generic mechanism so new concepts register at
* runtime instead of adding a subsystem-specific slot to the EXE.
* Does NOT own entity state, rendering, networking, or gameplay policy.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "hot-reload/game-api.h"

namespace MimitaRuntime {

class GenericRuntime {
public:
    static GenericRuntime& instance();

    // Registers the active package generation. On failure the previous
    // registration is retained and `error` explains the conflict.
    bool activate(const GamePackageDescriptorV1* package, std::string& error);
    void deactivate();

    // Runs every system registered in `domainId`, ordered by (priority, id).
    bool runDomain(std::uint64_t domainId, std::uint64_t tick, float dt, void* host);

    // Runs every non-reserved domain that any registered system declares, once
    // per fixed tick. Reserved domains (gameplay/render) are timed by the kernel.
    bool runRegisteredDomains(std::uint64_t tick, float dt, void* host);

    // Immediate event dispatch to the registered event type handler.
    bool dispatchEvent(const GameEventV1& event, void* host);

    // Build + dispatch an event by id (generic emit).
    bool emit(std::uint64_t typeId, std::uint64_t sourceEntity,
              std::uint64_t targetEntity, const void* payload, std::uint32_t payloadSize,
              std::uint64_t tick, void* host);

    // Resolved capability provider callable (nullptr when none).
    void* capability(std::uint64_t id) const;
    bool hasCapability(std::uint64_t id) const;
    bool kernelProvidesCapability(std::uint64_t id) const;

    bool hasCommand(const std::string& name) const;
    bool runCommand(const std::string& name, const char* args, void* host) const;

    // ── Hot movement override (free-fly/noclip) ─────────────────────
    // Hot gameplay systems request a full local-player transform for this tick;
    // the kernel applies it and skips the built-in physics step.
    void beginMovementTick();
    void requestMovementOverride(std::uint32_t flags, const float position[3],
                                 const float velocity[3], float yaw);
    bool consumeMovementOverride(float outPosition[3], float outVelocity[3],
                                 float& outYaw);
    bool movementOverrideActive() const { return movementOverrideActive_; }

    // Kernel-owned shared state at the start of permanentStorage (nullptr before
    // the runtime allocates it).
    GameSharedStateV1* sharedState();

    bool active() const { return activePackage_ != nullptr; }
    std::uint64_t packageId() const { return packageId_; }
    std::uint64_t logicalHash() const { return logicalHash_; }
    // Deterministic hash of the registered type-id set (systems, events, schemas,
    // capabilities, commands) for the generation manifest / cross-peer agreement.
    std::uint64_t manifestHash() const;
    std::size_t systemCount() const { return systems_.size(); }
    std::size_t commandCount() const { return commands_.size(); }
    std::size_t schemaCount() const { return schemas_.size(); }
    std::size_t capabilityProviderCount() const { return capabilityProviders_.size(); }
    std::size_t capabilityRequirementCount() const { return capabilityRequirements_.size(); }
    std::string describe() const;

private:
    GenericRuntime() = default;

    struct SystemEntry {
        std::uint64_t id = 0;
        std::uint64_t domainId = 0;
        std::uint32_t priority = 0;
        GameSystemInvokeFn invoke = nullptr;
        std::string name;
    };
    struct CommandEntry {
        std::string name;
        std::string usage;
        GameCommandInvokeFn invoke = nullptr;
    };
    struct EventEntry {
        std::uint64_t id = 0;
        std::uint64_t schemaHash = 0;
        GameEventDispatchFn dispatch = nullptr;
        std::string name;
    };
    struct SchemaEntry {
        std::uint64_t id = 0;
        std::uint32_t size = 0;
        std::uint32_t align = 1;
        std::uint32_t copyPolicy = 0;
        std::string name;
    };
    struct CapabilityEntry {
        std::uint64_t id = 0;
        void* callable = nullptr;
        std::string name;
    };

    const GamePackageDescriptorV1* activePackage_ = nullptr;
    std::vector<SystemEntry> systems_;
    std::vector<CommandEntry> commands_;
    std::unordered_map<std::string, std::size_t> commandIndex_;
    std::vector<EventEntry> events_;
    std::vector<SchemaEntry> schemas_;
    std::vector<CapabilityEntry> capabilityProviders_;
    std::vector<std::uint64_t> capabilityRequirements_;
    std::vector<std::uint64_t> domains_;
    std::uint64_t packageId_ = 0;
    std::uint64_t logicalHash_ = 0;
    std::string packageName_;
    bool movementOverrideActive_ = false;
    float movementPosition_[3]{0.0f, 0.0f, 0.0f};
    float movementVelocity_[3]{0.0f, 0.0f, 0.0f};
    float movementYaw_ = 0.0f;
};

} // namespace MimitaRuntime
