// 2026-11-14
#include "game/entity-domain-bridge-module.h"

namespace mimita::game {
namespace {
const mimita::host::EntityDomainCapabilityV1* gDomain = nullptr;
void* gHostData = nullptr;
}

void initializeHostEntityDomain(const mimita::host::HostServicesV1* services) {
    gDomain = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::EntityDomainCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::EntityDomain),
            mimita::host::kEntityDomainCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kEntityDomainCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::EntityDomainCapabilityV1)) {
        gDomain = capability;
    }
}

bool readPlayerCore(mimita::host::EntityHandle handle,
                    mimita::host::state::player_core::Record* out) {
    return gDomain && gDomain->read_player_core &&
           gDomain->read_player_core(gHostData, handle, out) ==
               mimita::host::HostResult::Ok;
}

bool writePlayerCore(mimita::host::EntityHandle handle,
                     const mimita::host::state::player_core::Record* in) {
    return gDomain && gDomain->write_player_core &&
           gDomain->write_player_core(gHostData, handle, in) ==
               mimita::host::HostResult::Ok;
}

bool readPlayerSession(mimita::host::EntityHandle handle,
                       mimita::host::state::player_session::Record* out) {
    return gDomain && gDomain->read_player_session &&
           gDomain->read_player_session(gHostData, handle, out) ==
               mimita::host::HostResult::Ok;
}

bool writePlayerSession(
    mimita::host::EntityHandle handle,
    const mimita::host::state::player_session::Record* in) {
    return gDomain && gDomain->write_player_session &&
           gDomain->write_player_session(gHostData, handle, in) ==
               mimita::host::HostResult::Ok;
}

bool readDomain(mimita::host::DomainId domain, mimita::host::EntityHandle handle,
                void* record, std::uint32_t recordSize) {
    return gDomain && gDomain->read_domain &&
           gDomain->read_domain(gHostData, domain, handle, record, recordSize) ==
               mimita::host::HostResult::Ok;
}

bool writeDomain(mimita::host::DomainId domain, mimita::host::EntityHandle handle,
                 const void* record, std::uint32_t recordSize) {
    return gDomain && gDomain->write_domain &&
           gDomain->write_domain(gHostData, domain, handle, record, recordSize) ==
               mimita::host::HostResult::Ok;
}

}  // namespace mimita::game
