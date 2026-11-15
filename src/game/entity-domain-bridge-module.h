// 2026-11-14
// Game generation bridge for per-domain resident state records.
#pragma once

#include "host/abi/entity-domain-capability.h"

namespace mimita::game {
void initializeHostEntityDomain(const mimita::host::HostServicesV1* services);
bool readPlayerCore(mimita::host::EntityHandle handle,
                    mimita::host::state::player_core::Record* out);
bool writePlayerCore(mimita::host::EntityHandle handle,
                     const mimita::host::state::player_core::Record* in);
bool readPlayerSession(mimita::host::EntityHandle handle,
                       mimita::host::state::player_session::Record* out);
bool writePlayerSession(
    mimita::host::EntityHandle handle,
    const mimita::host::state::player_session::Record* in);

bool readDomain(mimita::host::DomainId domain, mimita::host::EntityHandle handle,
                void* record, std::uint32_t recordSize);
bool writeDomain(mimita::host::DomainId domain, mimita::host::EntityHandle handle,
                 const void* record, std::uint32_t recordSize);

template <typename Record>
bool readDomain(mimita::host::DomainId domain, mimita::host::EntityHandle handle,
                Record* out) {
    return readDomain(domain, handle, out, static_cast<std::uint32_t>(sizeof(Record)));
}

template <typename Record>
bool writeDomain(mimita::host::DomainId domain, mimita::host::EntityHandle handle,
                 const Record* in) {
    return writeDomain(domain, handle, in, static_cast<std::uint32_t>(sizeof(Record)));
}
}  // namespace mimita::game
