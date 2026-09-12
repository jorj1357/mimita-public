// 09 12 2026
/* purpose
* Define the capability model for behaviors/packages. A behavior declares the
* capabilities it needs; the kernel grants only allowed ones.
* Hash != signature != capability: bytes, trust, and permitted actions are
* distinct. Native C++ is the bootstrap; the model does not assume it.
* Does NOT enforce anything itself; it owns the vocabulary and grants.
*/
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Project {

enum class Capability : std::uint64_t {
    None = 0,
    ReadComponent = 1ull << 0,
    WriteComponent = 1ull << 1,
    EmitEvent = 1ull << 2,
    QueryPhysics = 1ull << 3,
    SpawnEntity = 1ull << 4,
    DestroyEntity = 1ull << 5,
    RenderCommand = 1ull << 6,
    NetworkMessage = 1ull << 7,
    Filesystem = 1ull << 8,
    Internet = 1ull << 9,
    Compute = 1ull << 10,
    Time = 1ull << 11,
};

using CapabilityMask = std::uint64_t;

inline CapabilityMask capabilityBit(Capability capability)
{
    return static_cast<CapabilityMask>(capability);
}

inline bool hasCapability(CapabilityMask mask, Capability capability)
{
    return (mask & capabilityBit(capability)) != 0;
}

inline const char* capabilityName(Capability capability)
{
    switch (capability) {
    case Capability::ReadComponent: return "read_component";
    case Capability::WriteComponent: return "write_component";
    case Capability::EmitEvent: return "emit_event";
    case Capability::QueryPhysics: return "query_physics";
    case Capability::SpawnEntity: return "spawn_entity";
    case Capability::DestroyEntity: return "destroy_entity";
    case Capability::RenderCommand: return "render_command";
    case Capability::NetworkMessage: return "network_message";
    case Capability::Filesystem: return "filesystem";
    case Capability::Internet: return "internet";
    case Capability::Compute: return "compute";
    case Capability::Time: return "time";
    case Capability::None: break;
    }
    return "none";
}

class CapabilityRegistry {
public:
    static CapabilityRegistry& instance();

    void grant(const std::string& subject, CapabilityMask mask);
    void setRequested(const std::string& subject, CapabilityMask mask);

    CapabilityMask granted(const std::string& subject) const;
    CapabilityMask requested(const std::string& subject) const;
    // Requested bits that were not granted.
    CapabilityMask denied(const std::string& subject) const;
    bool allows(const std::string& subject, Capability capability) const;

    std::size_t subjectCount() const { return grants_.size(); }

private:
    CapabilityRegistry() = default;
    std::map<std::string, CapabilityMask> grants_;
    std::map<std::string, CapabilityMask> requests_;
};

} // namespace Project
