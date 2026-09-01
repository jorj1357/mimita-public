// 09 01 2026, 12 00
/* purpose
* Publishes the local avatar manifest and lazily receives remote avatar images.
* Uses SHA-256 content names, a persistent cache, bounded packets, and retries.
* Applies avatar.json plus cached images to the existing remote Player renderer.
* Does NOT transfer bundled GLB cosmetics; those continue to resolve locally.
* Does NOT block the join handshake while image bytes are uploaded or downloaded.
* Does NOT use a local path as a network identity or trust an unverified cache file.
*/

#include "network/multiplayer-context.h"
#include "network/avatar-transfer.h"
#include "avatar/avatar.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <nlohmann/json.hpp>

namespace MimitaNet {

namespace {

using json = nlohmann::json;
constexpr size_t kMaxAvatarJsonBytes = 1024u * 1024u;
constexpr size_t kMaxAvatarImageBytes = 16u * 1024u * 1024u;
constexpr size_t kMaxAvatarImages = 64;

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

std::string manifestKey(uint32_t source, const std::string& hash)
{
    return std::to_string(source) + ":" + hash;
}

std::filesystem::path cacheDirectory()
{
    return std::filesystem::path("config") / "cache" / "avatars";
}

std::filesystem::path cachePath(const std::string& hash)
{
    return cacheDirectory() / (hash + ".asset");
}

bool readFile(const std::filesystem::path& path, std::vector<uint8_t>& out,
              size_t maxBytes)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0 || size > maxBytes)
        return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return file.good() || file.eof();
}

bool writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

bool cacheHas(const RemoteAvatarAsset& asset)
{
    std::vector<uint8_t> bytes;
    return readFile(cachePath(asset.hashHex), bytes, kMaxAvatarImageBytes) &&
           bytes.size() == asset.size &&
           avatarSha256Hex(bytes.data(), bytes.size()) == asset.hashHex;
}

bool safeAssetName(const std::string& name)
{
    if (name.empty() || name.size() >= AVATAR_ASSET_NAME_BYTES ||
        name.find("..") != std::string::npos ||
        name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos)
        return false;
    for (unsigned char c : name)
        if (std::iscntrl(c)) return false;
    return true;
}

bool allAssetsCached(const RemoteAvatarManifest& manifest)
{
    for (const RemoteAvatarAsset& asset : manifest.assets)
        if (!cacheHas(asset)) return false;
    return true;
}

void queuePacket(std::vector<std::vector<uint8_t>>& queue, const void* data,
                 size_t bytes)
{
    const auto* begin = reinterpret_cast<const uint8_t*>(data);
    queue.emplace_back(begin, begin + bytes);
}

void buildLocalAvatarPublication(MultiplayerContext& ctx)
{
    const std::string avatarName = AvatarSystem::instance().currentName();
    if (avatarName.empty()) return;

    json avatarJson;
    avatarToJson(AvatarSystem::instance().current(), avatarJson);
    avatarJson["playerModel"] = ""; // GLB stays bundled/local for now.
    avatarJson["basePath"] = "";

    json manifest;
    manifest["version"] = 1;
    manifest["avatarName"] = avatarName;
    manifest["avatar"] = avatarJson;
    manifest["assets"] = json::array();

    struct LocalAsset { std::string name; std::vector<uint8_t> bytes; std::string hash; };
    std::vector<LocalAsset> assets;
    const auto pngs = AvatarSystem::instance().listPngs(avatarName);
    for (const std::string& name : pngs)
    {
        if (assets.size() >= kMaxAvatarImages || !safeAssetName(name)) continue;
        std::string ext = std::filesystem::path(name).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;
        LocalAsset asset;
        asset.name = name;
        if (!readFile(std::filesystem::path(AvatarSystem::instance().avatarPath(avatarName)) /
                          name, asset.bytes, kMaxAvatarImageBytes))
            continue;
        asset.hash = avatarSha256Hex(asset.bytes.data(), asset.bytes.size());
        if (asset.hash.empty()) continue;
        manifest["assets"].push_back({{"name", asset.name},
                                       {"hash", asset.hash},
                                       {"size", asset.bytes.size()}});
        assets.push_back(std::move(asset));
    }

    const std::string manifestBytes = manifest.dump();
    const std::string manifestHash = avatarSha256Hex(
        reinterpret_cast<const uint8_t*>(manifestBytes.data()), manifestBytes.size());
    uint8_t manifestHashBytes[32] = {};
    if (manifestHash.empty() || !avatarSha256HexToBytes(manifestHash, manifestHashBytes))
        return;

    ctx.avatarNetworkUploadQueue.clear();
    ctx.avatarNetworkUploadIndex = 0;
    const size_t manifestChunkCount =
        (manifestBytes.size() + AVATAR_MANIFEST_PAYLOAD_BYTES - 1) /
        AVATAR_MANIFEST_PAYLOAD_BYTES;
    if (manifestChunkCount == 0 || manifestChunkCount > 256) return;
    for (size_t i = 0; i < manifestChunkCount; ++i)
    {
        AvatarManifestChunkPacket packet{};
        packet.header.type = PACKET_AVATAR_MANIFEST;
        packet.header.tick = ctx.tick;
        packet.header.playerId = ctx.localPlayerId;
        packet.ownerPlayerId = ctx.localPlayerId;
        std::copy(manifestHashBytes, manifestHashBytes + 32, packet.manifestHash);
        packet.chunkIndex = static_cast<uint16_t>(i);
        packet.chunkCount = static_cast<uint16_t>(manifestChunkCount);
        const size_t offset = i * AVATAR_MANIFEST_PAYLOAD_BYTES;
        packet.payloadBytes = static_cast<uint16_t>(std::min<size_t>(
            AVATAR_MANIFEST_PAYLOAD_BYTES, manifestBytes.size() - offset));
        std::memcpy(packet.payload, manifestBytes.data() + offset, packet.payloadBytes);
        queuePacket(ctx.avatarNetworkUploadQueue, &packet, sizeof(packet));
    }

    for (const LocalAsset& asset : assets)
    {
        const size_t chunkCount =
            (asset.bytes.size() + AVATAR_ASSET_PAYLOAD_BYTES - 1) /
            AVATAR_ASSET_PAYLOAD_BYTES;
        if (chunkCount == 0 || chunkCount > 256) continue;
        uint8_t assetHash[32] = {};
        if (!avatarSha256HexToBytes(asset.hash, assetHash)) continue;
        for (size_t i = 0; i < chunkCount; ++i)
        {
            AvatarAssetChunkPacket packet{};
            packet.header.type = PACKET_AVATAR_ASSET_CHUNK;
            packet.header.tick = ctx.tick;
            packet.header.playerId = ctx.localPlayerId;
            packet.sourcePlayerId = ctx.localPlayerId;
            std::copy(assetHash, assetHash + 32, packet.assetHash);
            packet.chunkIndex = static_cast<uint16_t>(i);
            packet.chunkCount = static_cast<uint16_t>(chunkCount);
            packet.totalBytes = static_cast<uint32_t>(asset.bytes.size());
            packet.assetType = AVATAR_ASSET_TYPE_IMAGE;
            std::strncpy(packet.logicalName, asset.name.c_str(),
                         sizeof(packet.logicalName) - 1);
            const size_t offset = i * AVATAR_ASSET_PAYLOAD_BYTES;
            packet.payloadBytes = static_cast<uint16_t>(std::min<size_t>(
                AVATAR_ASSET_PAYLOAD_BYTES, asset.bytes.size() - offset));
            std::memcpy(packet.payload, asset.bytes.data() + offset, packet.payloadBytes);
            queuePacket(ctx.avatarNetworkUploadQueue, &packet, sizeof(packet));
        }
    }

    ctx.avatarNetworkManifestHash = manifestHash;
    ctx.avatarNetworkPublished = true;
    Debug::log(Debug::Category::Networking,
        "[AVATAR NET] publish avatar=%s manifest=%s images=%zu packets=%zu\n",
        avatarName.c_str(), manifestHash.c_str(), assets.size(),
        ctx.avatarNetworkUploadQueue.size());
}

void requestMissingAssets(MultiplayerContext& ctx, RemoteAvatarManifest& manifest)
{
    const uint64_t now = nowMs();
    if (manifest.requestsSent && now - manifest.lastRequestMs < 1000)
        return;
    manifest.requestsSent = true;
    manifest.lastRequestMs = now;
    for (const RemoteAvatarAsset& asset : manifest.assets)
    {
        if (cacheHas(asset)) continue;
        AvatarAssetRequestPacket request{};
        request.header.type = PACKET_AVATAR_ASSET_REQUEST;
        request.header.tick = ctx.tick;
        request.header.playerId = ctx.localPlayerId;
        request.sourcePlayerId = manifest.sourcePlayerId;
        request.requestId = ctx.nextAvatarAssetRequestId++;
        if (!avatarSha256HexToBytes(asset.hashHex, request.assetHash)) continue;
        mpSendPacket(ctx, &request, sizeof(request));
        Debug::log(Debug::Category::Networking,
            "[AVATAR NET] request source=%u hash=%s name=%s\n",
            manifest.sourcePlayerId, asset.hashHex.c_str(), asset.logicalName.c_str());
    }
}

void applyReadyManifest(MultiplayerContext& ctx, RemoteAvatarManifest& manifest)
{
    if (!manifest.parsed || manifest.applied || !allAssetsCached(manifest))
        return;
    auto playerIt = ctx.remotePlayers.find(manifest.sourcePlayerId);
    if (playerIt == ctx.remotePlayers.end()) return;

    json wrapper;
    try { wrapper = json::parse(manifest.avatarJson); }
    catch (...) { return; }
    wrapper["playerModel"] = "";
    wrapper["basePath"] = "";
    const std::string alias = "remote_" + manifest.manifestHash.substr(0, 24);
    const std::filesystem::path avatarDir = AvatarSystem::avatarPath(alias);
    std::error_code error;
    std::filesystem::create_directories(avatarDir, error);
    if (error) return;
    const std::string avatarText = wrapper.dump(2);
    std::vector<uint8_t> avatarBytes(avatarText.begin(), avatarText.end());
    if (!writeFile(avatarDir / "avatar.json", avatarBytes)) return;
    for (const RemoteAvatarAsset& asset : manifest.assets)
    {
        if (!safeAssetName(asset.logicalName)) return;
        std::vector<uint8_t> bytes;
        if (!readFile(cachePath(asset.hashHex), bytes, kMaxAvatarImageBytes) ||
            !writeFile(avatarDir / asset.logicalName, bytes))
            return;
    }

    Player& player = playerIt->second;
    if (!AvatarSystem::instance().applyAvatarToPlayer(player, alias))
        return;
    player.setAvatarName(manifest.avatarName);
    manifest.applied = true;
    Debug::warn(Debug::Category::Avatar,
        "[REMOTE AVATAR READY] player=%u avatar=%s manifest=%s images=%zu\n",
        manifest.sourcePlayerId, manifest.avatarName.c_str(),
        manifest.manifestHash.c_str(), manifest.assets.size());
}

} // namespace

void mpResetAvatarNetwork(MultiplayerContext& ctx)
{
    ctx.avatarNetworkPublished = false;
    ctx.avatarNetworkManifestHash.clear();
    ctx.avatarNetworkUploadQueue.clear();
    ctx.avatarNetworkUploadIndex = 0;
    ctx.remoteAvatarManifests.clear();
    ctx.incomingAvatarAssets.clear();
    ctx.nextAvatarAssetRequestId = 1;
}

void mpAvatarNetworkTick(MultiplayerContext& ctx)
{
    if (!ctx.connected || ctx.localPlayerId == 0) return;
    if (!ctx.avatarNetworkPublished)
        buildLocalAvatarPublication(ctx);
    if (ctx.avatarNetworkUploadIndex < ctx.avatarNetworkUploadQueue.size())
    {
        const auto& packet = ctx.avatarNetworkUploadQueue[ctx.avatarNetworkUploadIndex];
        if (mpSendPacket(ctx, packet.data(), static_cast<int>(packet.size())))
            ++ctx.avatarNetworkUploadIndex;
    }
    for (auto& item : ctx.remoteAvatarManifests)
    {
        RemoteAvatarManifest& manifest = item.second;
        if (manifest.parsed && !manifest.applied)
        {
            requestMissingAssets(ctx, manifest);
            applyReadyManifest(ctx, manifest);
        }
    }
}

void mpProcessAvatarManifestPacket(MultiplayerContext& ctx,
                                   const AvatarManifestChunkPacket& packet,
                                   int bytes)
{
    if (bytes < static_cast<int>(sizeof(packet)) ||
        packet.ownerPlayerId == 0 || packet.ownerPlayerId == ctx.localPlayerId ||
        packet.chunkCount == 0 || packet.chunkCount > 256 ||
        packet.chunkIndex >= packet.chunkCount ||
        packet.payloadBytes > AVATAR_MANIFEST_PAYLOAD_BYTES)
        return;
    const std::string hash = hashHex(packet.manifestHash);
    RemoteAvatarManifest& manifest = ctx.remoteAvatarManifests[
        manifestKey(packet.ownerPlayerId, hash)];
    manifest.sourcePlayerId = packet.ownerPlayerId;
    manifest.manifestHash = hash;
    if (manifest.manifestChunks.size() != packet.chunkCount)
    {
        manifest.manifestChunks.assign(packet.chunkCount, {});
        manifest.expectedManifestChunks = packet.chunkCount;
    }
    manifest.manifestChunks[packet.chunkIndex].assign(
        packet.payload, packet.payload + packet.payloadBytes);
    for (const auto& chunk : manifest.manifestChunks)
        if (chunk.empty()) return;

    std::string text;
    for (const auto& chunk : manifest.manifestChunks)
        text.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
    if (text.size() > kMaxAvatarJsonBytes) return;
    try
    {
        const json wrapper = json::parse(text);
        manifest.avatarName = wrapper.at("avatarName").get<std::string>();
        manifest.avatarJson = wrapper.at("avatar").dump();
        manifest.assets.clear();
        for (const auto& item : wrapper.at("assets"))
        {
            RemoteAvatarAsset asset;
            asset.logicalName = item.at("name").get<std::string>();
            asset.hashHex = item.at("hash").get<std::string>();
            asset.size = item.at("size").get<uint32_t>();
            uint8_t ignored[32] = {};
            if (!safeAssetName(asset.logicalName) || asset.size == 0 ||
                asset.size > kMaxAvatarImageBytes ||
                !avatarSha256HexToBytes(asset.hashHex, ignored))
                return;
            manifest.assets.push_back(std::move(asset));
        }
        manifest.parsed = true;
        Debug::log(Debug::Category::Networking,
            "[AVATAR NET] manifest received source=%u hash=%s images=%zu\n",
            manifest.sourcePlayerId, manifest.manifestHash.c_str(),
            manifest.assets.size());
    }
    catch (...) { return; }
}

void mpProcessAvatarAssetChunkPacket(MultiplayerContext& ctx,
                                     const AvatarAssetChunkPacket& packet,
                                     int bytes)
{
    if (bytes < static_cast<int>(sizeof(packet)) ||
        packet.targetPlayerId != ctx.localPlayerId || packet.sourcePlayerId == 0 ||
        packet.chunkCount == 0 || packet.chunkCount > 256 ||
        packet.chunkIndex >= packet.chunkCount ||
        packet.payloadBytes > AVATAR_ASSET_PAYLOAD_BYTES || packet.totalBytes == 0 ||
        packet.totalBytes > kMaxAvatarImageBytes)
        return;
    const std::string hash = hashHex(packet.assetHash);
    IncomingAvatarAsset& incoming = ctx.incomingAvatarAssets[
        manifestKey(packet.sourcePlayerId, hash)];
    incoming.sourcePlayerId = packet.sourcePlayerId;
    incoming.requestId = packet.requestId;
    incoming.hashHex = hash;
    incoming.logicalName = packet.logicalName;
    incoming.totalBytes = packet.totalBytes;
    incoming.expectedChunks = packet.chunkCount;
    if (incoming.chunks.size() != packet.chunkCount)
        incoming.chunks.assign(packet.chunkCount, {});
    incoming.chunks[packet.chunkIndex].assign(
        packet.payload, packet.payload + packet.payloadBytes);
    for (const auto& chunk : incoming.chunks)
        if (chunk.empty()) return;

    std::vector<uint8_t> bytesOut;
    bytesOut.reserve(incoming.totalBytes);
    for (const auto& chunk : incoming.chunks)
        bytesOut.insert(bytesOut.end(), chunk.begin(), chunk.end());
    if (bytesOut.size() != incoming.totalBytes ||
        avatarSha256Hex(bytesOut.data(), bytesOut.size()) != incoming.hashHex)
    {
        Debug::warn(Debug::Category::Networking,
            "[AVATAR NET] asset rejected source=%u hash=%s reason=sha256-mismatch\n",
            incoming.sourcePlayerId, incoming.hashHex.c_str());
        ctx.incomingAvatarAssets.erase(manifestKey(packet.sourcePlayerId, hash));
        return;
    }
    if (!writeFile(cachePath(incoming.hashHex), bytesOut)) return;
    ctx.incomingAvatarAssets.erase(manifestKey(packet.sourcePlayerId, hash));
    Debug::log(Debug::Category::Networking,
        "[AVATAR NET] asset cached source=%u hash=%s bytes=%zu\n",
        packet.sourcePlayerId, hash.c_str(), bytesOut.size());
}

} // namespace MimitaNet
