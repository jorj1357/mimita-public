// 09 14 2026
/* purpose
* Implements the generic dynamic-component replication envelope and client apply.
* Does NOT own simulation, gameplay policy, or the dynamic store.
*/
#include "network/dynamic-replication.h"

#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "network/packets.h"
#include "network/server.h"

namespace MimitaNet {

namespace {

constexpr std::size_t kRecordFixedBytes =
    1 + 3 + 4 + 4 + 4 + 4 + 8 + 8 + 8;  // 44
constexpr std::size_t kSchemaDescTail = 4 + 4 + 32;  // schemaSize/align/name
constexpr std::size_t kRelationshipFixedBytes =
    1 + 3 + 4 + 4 + 4 + 8 + 8 + 8 + 8;  // 48
constexpr std::size_t kMaxRecordsPerPacket = 16;

void appendBytes(std::vector<std::uint8_t>& out, const void* data, std::size_t size)
{
    const auto* p = static_cast<const std::uint8_t*>(data);
    out.insert(out.end(), p, p + size);
}

template <typename T>
void appendPod(std::vector<std::uint8_t>& out, const T& value)
{
    appendBytes(out, &value, sizeof(T));
}

struct Reader {
    const std::uint8_t* p = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
    bool ok = true;

    bool need(std::size_t n)
    {
        if (pos + n > size) { ok = false; return false; }
        return true;
    }
    template <typename T>
    T read()
    {
        T value{};
        if (!need(sizeof(T))) return value;
        std::memcpy(&value, p + pos, sizeof(T));
        pos += sizeof(T);
        return value;
    }
    const std::uint8_t* readBytes(std::size_t n)
    {
        if (!need(n)) return nullptr;
        const std::uint8_t* at = p + pos;
        pos += n;
        return at;
    }
};

bool replicatedPolicy(std::uint32_t policy)
{
    return policy == GAME_NET_ALL || policy == GAME_NET_OWNER;
}

} // namespace

std::vector<std::uint8_t> dynamicReplicationEncode(
    const std::vector<DynamicComponentRecord>& records,
    const std::vector<RelationshipRecord>& relationships)
{
    std::vector<std::uint8_t> out;
    out.reserve(sizeof(PacketHeader) + 20 + records.size() * (kRecordFixedBytes + 32) +
                relationships.size() * kRelationshipFixedBytes);
    PacketHeader header{};
    header.type = PACKET_DYNAMIC_COMPONENT;
    appendPod(out, header);
    appendPod(out, std::uint32_t{0});  // eventId (filled by the reliable queue)
    appendPod(out, std::uint32_t{0});  // eventSessionId
    appendPod(out, static_cast<std::uint32_t>(records.size()));
    appendPod(out, std::uint32_t{0});  // reserved

    for (const DynamicComponentRecord& r : records) {
        appendPod(out, r.op);
        appendPod(out, std::uint8_t{0});
        appendPod(out, std::uint8_t{0});
        appendPod(out, std::uint8_t{0});
        appendPod(out, r.schemaVersion);
        appendPod(out, r.changeVersion);
        appendPod(out, r.networkPolicy);
        appendPod(out, static_cast<std::uint32_t>(r.payload.size()));
        appendPod(out, r.entity);
        appendPod(out, r.typeId);
        appendPod(out, r.schemaHash);
        if (r.op == 2) {
            appendPod(out, r.schemaSize);
            appendPod(out, r.schemaAlign);
            char name[32] = {};
            std::strncpy(name, r.schemaName, sizeof(name) - 1);
            appendBytes(out, name, sizeof(name));
        } else if (r.op == 0 && !r.payload.empty()) {
            appendBytes(out, r.payload.data(), r.payload.size());
        }
    }

    appendPod(out, static_cast<std::uint32_t>(relationships.size()));
    for (const RelationshipRecord& r : relationships) {
        appendPod(out, r.op);
        appendPod(out, std::uint8_t{0});
        appendPod(out, std::uint8_t{0});
        appendPod(out, std::uint8_t{0});
        appendPod(out, r.changeVersion);
        appendPod(out, r.networkPolicy);
        appendPod(out, std::uint32_t{0});  // reserved
        appendPod(out, r.value);
        appendPod(out, r.source);
        appendPod(out, r.typeId);
        appendPod(out, r.target);
    }
    return out;
}

bool dynamicReplicationDecode(
    const std::uint8_t* data, std::size_t size,
    std::vector<DynamicComponentRecord>& out,
    std::vector<RelationshipRecord>& outRelationships, std::string& error)
{
    out.clear();
    outRelationships.clear();
    if (!data || size < sizeof(PacketHeader) + 16) {
        error = "short packet";
        return false;
    }
    Reader reader{data, size, 0};
    reader.read<PacketHeader>();
    reader.read<std::uint32_t>();  // eventId
    reader.read<std::uint32_t>();  // eventSessionId
    const std::uint32_t count = reader.read<std::uint32_t>();
    reader.read<std::uint32_t>();  // reserved
    if (!reader.ok) { error = "truncated header"; return false; }

    for (std::uint32_t i = 0; i < count; ++i) {
        DynamicComponentRecord r;
        r.op = reader.read<std::uint8_t>();
        reader.read<std::uint8_t>();
        reader.read<std::uint8_t>();
        reader.read<std::uint8_t>();
        r.schemaVersion = reader.read<std::uint32_t>();
        r.changeVersion = reader.read<std::uint32_t>();
        r.networkPolicy = reader.read<std::uint32_t>();
        const std::uint32_t payloadSize = reader.read<std::uint32_t>();
        r.entity = reader.read<std::uint64_t>();
        r.typeId = reader.read<std::uint64_t>();
        r.schemaHash = reader.read<std::uint64_t>();
        if (!reader.ok) { error = "truncated record"; return false; }
        if (r.op == 2) {
            r.schemaSize = reader.read<std::uint32_t>();
            r.schemaAlign = reader.read<std::uint32_t>();
            const std::uint8_t* name = reader.readBytes(32);
            if (!reader.ok || !name) { error = "truncated schema desc"; return false; }
            std::memcpy(r.schemaName, name, 31);
            r.schemaName[31] = '\0';
        } else if (r.op == 0 && payloadSize > 0) {
            const std::uint8_t* payload = reader.readBytes(payloadSize);
            if (!reader.ok || !payload) { error = "truncated payload"; return false; }
            r.payload.assign(payload, payload + payloadSize);
        }
        out.push_back(std::move(r));
    }

    // Relationship section is optional so an encoded body with none still
    // decodes cleanly.
    if (reader.pos + sizeof(std::uint32_t) <= reader.size) {
        const std::uint32_t relCount = reader.read<std::uint32_t>();
        if (!reader.ok) { error = "truncated relationship count"; return false; }
        for (std::uint32_t i = 0; i < relCount; ++i) {
            RelationshipRecord r;
            r.op = reader.read<std::uint8_t>();
            reader.read<std::uint8_t>();
            reader.read<std::uint8_t>();
            reader.read<std::uint8_t>();
            r.changeVersion = reader.read<std::uint32_t>();
            r.networkPolicy = reader.read<std::uint32_t>();
            reader.read<std::uint32_t>();  // reserved
            r.value = reader.read<std::uint64_t>();
            r.source = reader.read<std::uint64_t>();
            r.typeId = reader.read<std::uint64_t>();
            r.target = reader.read<std::uint64_t>();
            if (!reader.ok) { error = "truncated relationship record"; return false; }
            outRelationships.push_back(r);
        }
    }
    return true;
}

bool dynamicReplicationApply(const std::vector<DynamicComponentRecord>& records,
                             const std::vector<RelationshipRecord>& relationships,
                             MimitaRuntime::DynamicComponentStore& store,
                             MimitaRuntime::RelationshipStore& relations,
                             std::string& error)
{
    for (const RelationshipRecord& r : relationships) {
        if (r.source == 0 || r.target == 0 || r.typeId == 0) {
            error = "invalid relationship ids";
            return false;
        }
        if (r.op == 4) {
            relations.remove(r.typeId, static_cast<EntityId>(r.source),
                             static_cast<EntityId>(r.target));
        } else {
            relations.addVersioned(r.typeId, static_cast<EntityId>(r.source),
                                   static_cast<EntityId>(r.target), r.value,
                                   r.changeVersion);
        }
    }

    for (const DynamicComponentRecord& r : records) {
        if (r.op == 2) {
            const MimitaRuntime::DynamicComponentSchema* existing = store.schema(r.typeId);
            if (existing && existing->schemaHash == r.schemaHash &&
                existing->version == r.schemaVersion)
                continue;
            MimitaRuntime::DynamicComponentSchema schema;
            schema.typeId = r.typeId;
            schema.schemaHash = r.schemaHash;
            schema.version = r.schemaVersion ? r.schemaVersion : 1;
            schema.size = r.schemaSize;
            schema.align = r.schemaAlign ? r.schemaAlign : 1;
            schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
            schema.networkPolicy = r.networkPolicy;
            schema.name = r.schemaName;
            if (existing && existing->version != schema.version) {
                // Version change: migrate existing client state through the
                // registered migration; failure preserves last-good state.
                if (!store.applySchemaUpdate({schema}, error))
                    return false;
            } else {
                store.registerSchema(schema);
            }
            continue;
        }
        if (r.entity == 0) {
            error = "invalid entity id";
            return false;
        }
        if (r.op == 1) {
            store.remove(static_cast<EntityId>(r.entity), r.typeId);
            continue;
        }
        // Upsert: require a known schema and an exact (or unspecified) size.
        const MimitaRuntime::DynamicComponentSchema* schema = store.schema(r.typeId);
        if (!schema) { error = "unknown component schema"; return false; }
        if (schema->size != 0 && schema->size != r.payload.size()) {
            error = "payload size mismatch";
            return false;
        }
        if (!store.write(static_cast<EntityId>(r.entity), r.typeId, r.payload.data(),
                         static_cast<std::uint32_t>(r.payload.size()))) {
            error = "component write rejected";
            return false;
        }
    }
    return true;
}

void dynamicReplicationCollectServer(
    std::vector<DynamicComponentRecord>& out,
    std::vector<RelationshipRecord>& outRelationships)
{
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    for (std::uint64_t typeId : store.typeIds()) {
        const MimitaRuntime::DynamicComponentSchema* schema = store.schema(typeId);
        if (!schema || !replicatedPolicy(schema->networkPolicy))
            continue;
        EntityId entities[128] = {0};
        const std::size_t count = store.enumerate(typeId, entities, 128);
        for (std::size_t i = 0; i < count; ++i) {
            DynamicComponentRecord r;
            r.op = 0;
            r.entity = entities[i];
            r.typeId = typeId;
            r.schemaHash = schema->schemaHash;
            r.schemaVersion = schema->version;
            r.changeVersion = store.changeVersionOf(entities[i], typeId);
            r.networkPolicy = schema->networkPolicy;
            r.payload = store.blob(entities[i], typeId);
            out.push_back(std::move(r));
        }
    }

    MimitaRuntime::RelationshipStore& relations =
        MimitaRuntime::RelationshipStore::instance();
    for (std::uint64_t typeId : relations.typeIds()) {
        const std::uint32_t policy = relations.networkPolicy(typeId);
        if (!replicatedPolicy(policy))
            continue;
        MimitaRuntime::RelationshipEdge edges[128] = {};
        const std::size_t count = relations.edgesOfType(typeId, edges, 128);
        for (std::size_t i = 0; i < count; ++i) {
            RelationshipRecord r;
            r.op = 3;
            r.source = edges[i].from;
            r.target = edges[i].to;
            r.typeId = typeId;
            r.value = edges[i].value;
            r.changeVersion = edges[i].changeVersion;
            r.networkPolicy = policy;
            outRelationships.push_back(r);
        }
    }
}

void serverReplicateDynamicComponents(std::uintptr_t sock,
                                      std::unordered_map<uint32_t, ServerPlayer>& players,
                                      uint32_t tick, uint64_t& totalPacketsOut)
{
    (void)tick;
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    MimitaRuntime::RelationshipStore& relations =
        MimitaRuntime::RelationshipStore::instance();
    const std::vector<std::uint64_t> componentTypes = store.typeIds();
    const std::vector<std::uint64_t> relationshipTypes = relations.typeIds();

    struct PerClient {
        std::map<std::pair<std::uint64_t, std::uint64_t>, std::pair<std::uint32_t, std::uint32_t>> sent;
        std::unordered_set<std::uint64_t> knownSchemas;
        // (typeId, source, target) -> changeVersion for edges already sent.
        std::map<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>, std::uint32_t> sentEdges;
    };
    static std::unordered_map<uint32_t, PerClient> s_clients;

    auto flush = [&](const std::vector<DynamicComponentRecord>& comps,
                     const std::vector<RelationshipRecord>& rels,
                     ServerPlayer& player) {
        if (comps.empty() && rels.empty())
            return;
        const std::vector<std::uint8_t> bytes = dynamicReplicationEncode(comps, rels);
        queueReliableGameplayEventToPlayer(
            static_cast<SOCKET>(sock), player, bytes.data(), bytes.size(),
            nextReliableGameplayEventId(),
            reliableGameplayEventSessionForPlayer(player), totalPacketsOut);
    };

    for (auto& kv : players) {
        ServerPlayer& player = kv.second;
        if (player.spawnState != ServerPlayer::Active)
            continue;
        PerClient& client = s_clients[player.id];
        std::vector<DynamicComponentRecord> records;
        std::vector<RelationshipRecord> edgeRecords;

        // ── Components: schema descriptors + changed upserts ───────────
        for (std::uint64_t typeId : componentTypes) {
            const MimitaRuntime::DynamicComponentSchema* schema = store.schema(typeId);
            if (!schema || !replicatedPolicy(schema->networkPolicy))
                continue;
            if (client.knownSchemas.find(typeId) == client.knownSchemas.end()) {
                DynamicComponentRecord desc;
                desc.op = 2;
                desc.typeId = typeId;
                desc.schemaHash = schema->schemaHash;
                desc.schemaVersion = schema->version;
                desc.networkPolicy = schema->networkPolicy;
                desc.schemaSize = schema->size;
                desc.schemaAlign = schema->align;
                std::strncpy(desc.schemaName, schema->name.c_str(),
                             sizeof(desc.schemaName) - 1);
                records.push_back(std::move(desc));
                client.knownSchemas.insert(typeId);
            }
            EntityId entities[128] = {0};
            const std::size_t count = store.enumerate(typeId, entities, 128);
            for (std::size_t i = 0; i < count; ++i) {
                const EntityId entity = entities[i];
                if (schema->networkPolicy == GAME_NET_OWNER) {
                    if (entityDomain(entity) != EntityDomain::Player ||
                        entityLegacyId(entity) != player.id)
                        continue;
                }
                const std::uint32_t changeVersion = store.changeVersionOf(entity, typeId);
                const auto key = std::make_pair(entity, typeId);
                auto sit = client.sent.find(key);
                if (sit != client.sent.end() &&
                    sit->second.first == changeVersion &&
                    sit->second.second == schema->version)
                    continue;
                DynamicComponentRecord r;
                r.op = 0;
                r.entity = entity;
                r.typeId = typeId;
                r.schemaHash = schema->schemaHash;
                r.schemaVersion = schema->version;
                r.changeVersion = changeVersion;
                r.networkPolicy = schema->networkPolicy;
                r.payload = store.blob(entity, typeId);
                client.sent[key] = {changeVersion, schema->version};
                records.push_back(std::move(r));
            }
        }
        // Component removals: anything previously sent that no longer exists.
        for (auto it = client.sent.begin(); it != client.sent.end();) {
            const EntityId entity = static_cast<EntityId>(it->first.first);
            const std::uint64_t typeId = it->first.second;
            if (store.has(entity, typeId)) {
                ++it;
                continue;
            }
            DynamicComponentRecord r;
            r.op = 1;
            r.entity = entity;
            r.typeId = typeId;
            records.push_back(std::move(r));
            it = client.sent.erase(it);
        }

        // ── Relationships: changed additions + removals ────────────────
        for (std::uint64_t typeId : relationshipTypes) {
            const std::uint32_t policy = relations.networkPolicy(typeId);
            if (!replicatedPolicy(policy))
                continue;
            MimitaRuntime::RelationshipEdge edges[128] = {};
            const std::size_t count = relations.edgesOfType(typeId, edges, 128);
            for (std::size_t i = 0; i < count; ++i) {
                const EntityId source = edges[i].from;
                if (policy == GAME_NET_OWNER) {
                    if (entityDomain(source) != EntityDomain::Player ||
                        entityLegacyId(source) != player.id)
                        continue;
                }
                const auto key = std::make_tuple(
                    typeId, static_cast<std::uint64_t>(source),
                    static_cast<std::uint64_t>(edges[i].to));
                auto sit = client.sentEdges.find(key);
                if (sit != client.sentEdges.end() &&
                    sit->second == edges[i].changeVersion)
                    continue;
                RelationshipRecord r;
                r.op = 3;
                r.source = source;
                r.target = edges[i].to;
                r.typeId = typeId;
                r.value = edges[i].value;
                r.changeVersion = edges[i].changeVersion;
                r.networkPolicy = policy;
                client.sentEdges[key] = edges[i].changeVersion;
                edgeRecords.push_back(r);
            }
        }
        for (auto it = client.sentEdges.begin(); it != client.sentEdges.end();) {
            const std::uint64_t typeId = std::get<0>(it->first);
            const EntityId source = static_cast<EntityId>(std::get<1>(it->first));
            const EntityId target = static_cast<EntityId>(std::get<2>(it->first));
            if (relations.has(typeId, source, target)) {
                ++it;
                continue;
            }
            RelationshipRecord r;
            r.op = 4;
            r.source = source;
            r.target = target;
            r.typeId = typeId;
            edgeRecords.push_back(std::move(r));
            it = client.sentEdges.erase(it);
        }

        // Send in bounded batches so no changed record is silently dropped.
        std::size_t compIndex = 0;
        std::size_t edgeIndex = 0;
        while (compIndex < records.size() || edgeIndex < edgeRecords.size()) {
            const std::size_t compTake =
                std::min(kMaxRecordsPerPacket, records.size() - compIndex);
            const std::size_t edgeTake =
                std::min(kMaxRecordsPerPacket - compTake,
                         edgeRecords.size() - edgeIndex);
            std::vector<DynamicComponentRecord> compBatch(
                records.begin() + compIndex, records.begin() + compIndex + compTake);
            std::vector<RelationshipRecord> edgeBatch(
                edgeRecords.begin() + edgeIndex,
                edgeRecords.begin() + edgeIndex + edgeTake);
            flush(compBatch, edgeBatch, player);
            compIndex += compTake;
            edgeIndex += edgeTake;
        }
    }
}

} // namespace MimitaNet
