// 09 14 2026
/* purpose
* Generic dynamic-component replication. One opaque envelope carries schema
* descriptors, component upserts, and component removals for ANY dynamic
* component type, chosen by the schema's networkPolicy. There is no
* component-specific packet, struct, encoder, or decoder.
* The kernel owns encode/decode/apply; the transport stays component-agnostic.
* Does NOT own simulation, gameplay policy, or the dynamic store.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace MimitaRuntime { class DynamicComponentStore; class RelationshipStore; }

namespace MimitaNet {

struct ServerPlayer;

struct DynamicComponentRecord {
    std::uint8_t op = 0;   // 0 = upsert, 1 = remove, 2 = schema descriptor
    std::uint32_t schemaVersion = 0;
    std::uint32_t changeVersion = 0;
    std::uint32_t networkPolicy = 0;
    std::uint64_t entity = 0;
    std::uint64_t typeId = 0;
    std::uint64_t schemaHash = 0;
    std::vector<std::uint8_t> payload;
    // schema descriptor (op == 2)
    std::uint32_t schemaSize = 0;
    std::uint32_t schemaAlign = 1;
    char schemaName[32] = {};
};

// Generic relationship edge record carried in the same envelope. No
// item-/equip-/ownership-specific packet exists.
struct RelationshipRecord {
    std::uint8_t op = 3;   // 3 = add/update, 4 = remove
    std::uint32_t changeVersion = 0;
    std::uint32_t networkPolicy = 0;
    std::uint64_t source = 0;
    std::uint64_t typeId = 0;
    std::uint64_t target = 0;
    std::uint64_t value = 0;
};

// Opaque wire body: PacketHeader + eventId/session + component records +
// relationship records.
std::vector<std::uint8_t> dynamicReplicationEncode(
    const std::vector<DynamicComponentRecord>& records,
    const std::vector<RelationshipRecord>& relationships = {});
bool dynamicReplicationDecode(
    const std::uint8_t* data, std::size_t size,
    std::vector<DynamicComponentRecord>& out,
    std::vector<RelationshipRecord>& outRelationships, std::string& error);

// Generic client apply. Registers unknown schemas from descriptors, attaches/
// updates/removes components by id, applies relationship edges, and rejects
// mismatched payload sizes and incompatible schemas without reinterpreting
// bytes. No switch on component or relationship type.
bool dynamicReplicationApply(const std::vector<DynamicComponentRecord>& records,
                             const std::vector<RelationshipRecord>& relationships,
                             MimitaRuntime::DynamicComponentStore& store,
                             MimitaRuntime::RelationshipStore& relations,
                             std::string& error);

// Server-side collection of every replicated component and relationship.
void dynamicReplicationCollectServer(
    std::vector<DynamicComponentRecord>& out,
    std::vector<RelationshipRecord>& outRelationships);

// Server tick: send changed replicated components to each connected player over
// the existing reliable gameplay-event channel. `sock` is a native SOCKET
// (uintptr_t) kept untyped here so this header stays independent of server.h.
void serverReplicateDynamicComponents(std::uintptr_t sock,
                                      std::unordered_map<uint32_t, ServerPlayer>& players,
                                      uint32_t tick, uint64_t& totalPacketsOut);

} // namespace MimitaNet
