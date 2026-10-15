// 09 01 2026, 12 00
/* purpose
* Owns server-side forwarding and temporary storage for remote avatar assets.
* Relays avatar.json manifests to peers and answers SHA-256 image cache misses.
* Keeps player paths private: the server stores bytes under their content hash.
* Does NOT render avatars, load GLB cosmetics, or decide the local fallback model.
* Does NOT trust arbitrary packet sizes, chunk counts, or cross-player ownership.
* Does NOT broadcast PNG bytes until a receiver explicitly requests that hash.
*/

#include "network/server.h"
#include "network/avatar-transfer.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <nlohmann/json.hpp>

namespace MimitaNet {

namespace {

// Reassemble a complete manifest chunk set and read only the avatar name. The
// server needs this so a mid-session avatar change re-resolves the movement
// capability instead of leaving the player on its join-time mode (spec 7.1).
std::string manifestAvatarName(
    const std::vector<std::vector<uint8_t>>& chunks)
{
    std::string jsonBytes;
    uint16_t expected = 0;
    for (size_t i = 0; i < chunks.size(); ++i)
    {
        if (chunks[i].size() != sizeof(AvatarManifestChunkPacket))
            return {};
        const auto& pk = *reinterpret_cast<const AvatarManifestChunkPacket*>(
            chunks[i].data());
        if (i == 0)
            expected = pk.chunkCount;
        if (pk.chunkIndex != i || pk.chunkCount != expected ||
            pk.payloadBytes > AVATAR_MANIFEST_PAYLOAD_BYTES)
            return {};
        jsonBytes.append(reinterpret_cast<const char*>(pk.payload),
                         pk.payloadBytes);
    }
    if (jsonBytes.empty())
        return {};
    try
    {
        const auto j = nlohmann::json::parse(jsonBytes);
        const auto it = j.find("avatarName");
        if (it != j.end() && it->is_string())
            return it->get<std::string>();
    }
    catch (...)
    {
    }
    return {};
}

std::string hashHex(const uint8_t bytes[32])
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (int i = 0; i < 32; ++i)
    {
        result.push_back(hex[(bytes[i] >> 4) & 0xf]);
        result.push_back(hex[bytes[i] & 0xf]);
    }
    return result;
}

bool validPlayerSource(const ServerPlayer& player, const sockaddr_in& from,
                       const TransportConnectionId* connectionId,
                       const char* buffer, int bytes, size_t expected)
{
    if (bytes < static_cast<int>(expected)) return false;
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    return header->playerId == player.id &&
           playerOwnsConnectionSource(player, &from, connectionId);
}

bool completeAsset(const std::vector<AvatarAssetChunkPacket>& chunks)
{
    if (chunks.empty()) return false;
    const uint16_t count = chunks.front().chunkCount;
    if (count == 0 || chunks.size() != count) return false;
    for (uint16_t i = 0; i < count; ++i)
    {
        if (chunks[i].chunkCount != count || chunks[i].chunkIndex != i)
            return false;
    }
    return true;
}

} // namespace

void handleAvatarManifestPacket(SOCKET sock, const sockaddr_in& from,
                                const char* buffer, int bytes,
                                std::unordered_map<uint32_t, ServerPlayer>& players,
                                uint64_t& totalPacketsOut,
                                const TransportConnectionId* connectionId)
{
    if (bytes < static_cast<int>(sizeof(AvatarManifestChunkPacket)))
        return;
    const auto& packet = *reinterpret_cast<const AvatarManifestChunkPacket*>(buffer);
    auto owner = players.find(packet.header.playerId);
    if (owner == players.end() || packet.ownerPlayerId != packet.header.playerId ||
        !validPlayerSource(owner->second, from, connectionId, buffer, bytes, sizeof(packet)) ||
        packet.chunkCount == 0 || packet.chunkCount > 256 ||
        packet.chunkIndex >= packet.chunkCount ||
        packet.payloadBytes > AVATAR_MANIFEST_PAYLOAD_BYTES)
    {
        Debug::warn(Debug::Category::Networking,
            "[AVATAR NET] manifest rejected player=%u bytes=%d\n",
            packet.header.playerId, bytes);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "griever.manifest.rejected", "server",
            "avatar manifest chunk rejected",
            (uint32_t)packet.header.tick,
            {{"header_player", packet.header.playerId},
             {"owner_player", packet.ownerPlayerId},
             {"bytes", bytes},
             {"expected_bytes", (int)sizeof(packet)},
             {"known_player", (int)(players.find(packet.header.playerId) != players.end())}},
            __FILE__, __LINE__, __FUNCTION__);
        return;
    }

    if (packet.chunkIndex == 0)
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "griever.manifest.arrived", owner->second.avatarName,
            "server received avatar manifest first chunk",
            (uint32_t)owner->second.spawnGeneration,
            {{"chunk_count", packet.chunkCount},
             {"payload_bytes", packet.payloadBytes},
             {"player_id", owner->first}},
            __FILE__, __LINE__, __FUNCTION__);

    const std::string key = hashHex(packet.manifestHash);
    auto& chunks = owner->second.avatarManifestChunks[key];
    if (chunks.size() != packet.chunkCount)
        chunks.assign(packet.chunkCount, {});
    chunks[packet.chunkIndex].assign(
        reinterpret_cast<const uint8_t*>(&packet),
        reinterpret_cast<const uint8_t*>(&packet) + sizeof(packet));

    // A complete manifest tells the server the player's current avatar name.
    // Apply it so server-players re-resolves the movement capability on the
    // next tick (spec 7.1) without waiting for a reconnect.
    if (chunks.size() == packet.chunkCount)
    {
        const std::string name = manifestAvatarName(chunks);
        if (!name.empty() && name.size() <= 32 &&
            name != owner->second.avatarName)
        {
            const std::string previous = owner->second.avatarName;
            owner->second.avatarName = name;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "griever.capability.resolved", name,
                "server applied avatar change from manifest",
                (uint32_t)owner->second.spawnGeneration,
                {{"avatar", name}, {"previous", previous},
                 {"player_id", owner->first}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    for (auto& peer : players)
    {
        if (peer.first == owner->first || !peer.second.spawned)
            continue;
        if (serverSendToPlayer(sock, peer.second, &packet, sizeof(packet)))
            ++totalPacketsOut;
    }

    Debug::logThrottled(Debug::Category::Networking,
        "avatar-manifest-forward", 1.0,
        "[AVATAR NET] manifest player=%u hash=%s chunk=%u/%u forwarded\n",
        packet.ownerPlayerId, key.c_str(), packet.chunkIndex + 1,
        packet.chunkCount);
    (void)from;
}

void handleAvatarAssetChunkPacket(SOCKET sock, const sockaddr_in& from,
                                  const char* buffer, int bytes,
                                  std::unordered_map<uint32_t, ServerPlayer>& players,
                                  uint64_t& totalPacketsOut,
                                  const TransportConnectionId* connectionId)
{
    if (bytes < static_cast<int>(sizeof(AvatarAssetChunkPacket)))
        return;
    const auto& packet = *reinterpret_cast<const AvatarAssetChunkPacket*>(buffer);
    auto owner = players.find(packet.header.playerId);
    if (owner == players.end() || packet.sourcePlayerId != packet.header.playerId ||
        packet.targetPlayerId != 0 ||
        !validPlayerSource(owner->second, from, connectionId, buffer, bytes, sizeof(packet)) ||
        packet.chunkCount == 0 || packet.chunkCount > 256 ||
        packet.chunkIndex >= packet.chunkCount ||
        packet.payloadBytes > AVATAR_ASSET_PAYLOAD_BYTES ||
        packet.totalBytes == 0 || packet.totalBytes > 16u * 1024u * 1024u)
    {
        Debug::warn(Debug::Category::Networking,
            "[AVATAR NET] asset upload rejected player=%u bytes=%d\n",
            packet.header.playerId, bytes);
        return;
    }

    const std::string key = hashHex(packet.assetHash);
    auto& chunks = owner->second.avatarAssetChunks[key];
    if (chunks.size() != packet.chunkCount)
        chunks.assign(packet.chunkCount, {});
    chunks[packet.chunkIndex] = packet;

    Debug::logThrottled(Debug::Category::Networking, "avatar-asset-upload", 1.0,
        "[AVATAR NET] asset upload player=%u hash=%s chunk=%u/%u complete=%d\n",
        packet.sourcePlayerId, key.c_str(), packet.chunkIndex + 1,
        packet.chunkCount, static_cast<int>(completeAsset(chunks)));
    (void)sock;
    (void)from;
    (void)totalPacketsOut;
}

void handleAvatarAssetRequestPacket(SOCKET sock, const sockaddr_in& from,
                                    const char* buffer, int bytes,
                                    std::unordered_map<uint32_t, ServerPlayer>& players,
                                    uint64_t& totalPacketsOut,
                                    const TransportConnectionId* connectionId)
{
    if (bytes < static_cast<int>(sizeof(AvatarAssetRequestPacket)))
        return;
    const auto& request = *reinterpret_cast<const AvatarAssetRequestPacket*>(buffer);
    auto receiver = players.find(request.header.playerId);
    auto source = players.find(request.sourcePlayerId);
    if (receiver == players.end() || source == players.end() ||
        receiver->first == source->first ||
        !validPlayerSource(receiver->second, from, connectionId, buffer, bytes, sizeof(request)))
        return;

    const std::string key = hashHex(request.assetHash);
    auto asset = source->second.avatarAssetChunks.find(key);
    if (asset == source->second.avatarAssetChunks.end() ||
        !completeAsset(asset->second))
    {
        Debug::log(Debug::Category::Networking,
            "[AVATAR NET] asset miss at server source=%u target=%u hash=%s\n",
            request.sourcePlayerId, receiver->first, key.c_str());
        return;
    }

    for (const AvatarAssetChunkPacket& stored : asset->second)
    {
        AvatarAssetChunkPacket response = stored;
        response.header.tick = request.header.tick;
        response.targetPlayerId = receiver->first;
        response.requestId = request.requestId;
        if (serverSendToPlayer(sock, receiver->second, &response, sizeof(response)))
            ++totalPacketsOut;
    }
    Debug::log(Debug::Category::Networking,
        "[AVATAR NET] asset served source=%u target=%u hash=%s chunks=%zu\n",
        request.sourcePlayerId, receiver->first, key.c_str(), asset->second.size());
    (void)from;
}

void sendStoredAvatarManifestsToPlayer(
    SOCKET sock, ServerPlayer& receiver,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint64_t& totalPacketsOut)
{
    for (const auto& peer : players)
    {
        if (peer.first == receiver.id || !peer.second.spawned)
            continue;
        for (const auto& manifest : peer.second.avatarManifestChunks)
        {
            for (const std::vector<uint8_t>& bytes : manifest.second)
            {
                if (bytes.size() != sizeof(AvatarManifestChunkPacket))
                    continue;
                if (serverSendToPlayer(sock, receiver, bytes.data(), bytes.size()))
                    ++totalPacketsOut;
            }
        }
    }
}

} // namespace MimitaNet
