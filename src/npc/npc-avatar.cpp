// 08 22 2026, 12 35
/* purpose
* Loads NPC avatar selection settings and applies a chosen avatar to an NPC.
* Resolves forced avatar.json paths or deterministically selects avatar folders per life.
* Uses the existing AvatarSystem so body textures and cosmetics share game behavior.
* Does NOT save avatar data or change the local player's configured avatar.
* Does NOT own NPC spawn timing or respawn rules.
* Does NOT serialize replay actor data.
*/
#include "npc/npc-avatar.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include <nlohmann/json.hpp>

#include "avatar/avatar.h"
#include "debug/debug-log.h"
#include "live-code/live-behavior.h"
#include "live-code/live-journal.h"
#include "hot-reload/hot-reload-system.h"
#include "npc/npc.h"

#include <GLFW/glfw3.h>

namespace {
constexpr const char* kConfigPath = "config/npc-avatar.json";
struct NpcAvatarConfig { bool forceAvatar = false; std::string forceAvatarPath; };
NpcAvatarConfig gConfig;
std::filesystem::file_time_type gLastWrite{};
bool gLoaded = false;

void recordAvatarDecision(std::uint32_t npcId, std::uint16_t transformEpoch,
                          const std::string& avatar, const char* result)
{
    LiveEventJournal::Fields fields;
    const auto status = HotReloadSystem::instance().status();
    fields.tick = 0;
    fields.generation = status.activeGeneration;
    fields.hasGeneration = status.activeGeneration != 0;
    fields.module = "actor.avatar-policy";
    fields.actorId = std::to_string(npcId);
    fields.result = result ? result : "unknown";
    nlohmann::json extra;
    extra["life_generation"] = transformEpoch;
    extra["avatar"] = avatar;
    fields.extra = extra.dump();
    if (fields.extra.size() >= 2 && fields.extra.front() == '{' && fields.extra.back() == '}')
        fields.extra = fields.extra.substr(1, fields.extra.size() - 2);
    LiveEventJournal::instance().record("npc_avatar_selected", fields);
}

void loadConfig()
{
    NpcAvatarConfig loaded;
    std::ifstream file(kConfigPath);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] config missing; using random valid avatars\n");
        gConfig = loaded;
        return;
    }
    try {
        nlohmann::json root;
        file >> root;
        loaded.forceAvatar = root.value("forceAvatar", false);
        loaded.forceAvatarPath = root.value("forceAvatarPath", "");
        gConfig = std::move(loaded);
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] config loaded force=%d path=%s\n", (int)gConfig.forceAvatar, gConfig.forceAvatarPath.c_str());
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] config invalid; using random valid avatars: %s\n", e.what());
        gConfig = NpcAvatarConfig{};
    }
}

std::string forcedAvatarName()
{
    if (!gConfig.forceAvatar || gConfig.forceAvatarPath.empty()) return {};
    const std::filesystem::path path(gConfig.forceAvatarPath);
    if (path.filename() != "avatar.json" || !std::filesystem::exists(path)) {
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] forced path invalid: %s; using random fallback\n", gConfig.forceAvatarPath.c_str());
        return {};
    }
    const std::filesystem::path expected = std::filesystem::path("assets") / "avatars" / path.parent_path().filename() / "avatar.json";
    std::error_code ec;
    if (!std::filesystem::equivalent(path, expected, ec)) {
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] forced path must stay under assets/avatars: %s; using random fallback\n", gConfig.forceAvatarPath.c_str());
        return {};
    }
    return path.parent_path().filename().string();
}

std::string chooseAvatar(std::uint32_t npcId, std::uint16_t transformEpoch)
{
    const std::string forced = forcedAvatarName();
    if (!forced.empty()) {
        recordAvatarDecision(npcId, transformEpoch, forced, "forced");
        return forced;
    }
    const std::vector<std::string> discoveredAvatars = AvatarSystem::instance().listAvatars();
    // NPC avatar identity currently travels in CompactEntityData::avatarName,
    // whose wire capacity is 15 characters plus the terminator.  Do not let a
    // longer folder name enter the selection path: truncating it in a snapshot
    // makes the client request a non-existent avatar and leaves the NPC on the
    // white/default body.  A future packet/schema migration can raise this
    // limit, but silently truncating a live identity is never safe.
    std::vector<std::string> avatars;
    avatars.reserve(discoveredAvatars.size());
    for (const std::string& avatar : discoveredAvatars) {
        if (avatar.size() < 16)
            avatars.push_back(avatar);
        else
            Debug::warn(Debug::Category::Avatar,
                        "[NPC AVATAR] skipping '%s': exceeds 15-byte network identity\n",
                        avatar.c_str());
    }
    if (avatars.empty()) return {};

    // The EXE owns discovery and validation of asset names. The hot policy
    // owns the per-life choice and returns only a plain-data result.
    ActorAvatarPolicyV1 policy{};
    policy.entityId = npcId;
    policy.lifeGeneration = transformEpoch;
    policy.candidateCount = static_cast<std::uint32_t>(std::min(
        avatars.size(), static_cast<std::size_t>(ACTOR_AVATAR_POLICY_MAX_CANDIDATES)));
    for (std::uint32_t i = 0; i < policy.candidateCount; ++i) {
        std::strncpy(policy.candidates[i], avatars[i].c_str(),
                     sizeof(policy.candidates[i]) - 1);
        policy.candidates[i][sizeof(policy.candidates[i]) - 1] = '\0';
    }
    if (LiveBehavior::dispatchActorAvatarPolicy(policy, 0) &&
        policy.selectedAvatar[0] != '\0') {
        Debug::log(Debug::Category::Avatar,
            "[NPC AVATAR HOT] npc=%u epoch=%u index=%u avatar=%s\n",
            npcId, (unsigned)transformEpoch, policy.selectedIndex,
            policy.selectedAvatar);
        recordAvatarDecision(npcId, transformEpoch, policy.selectedAvatar, "hot");
        return policy.selectedAvatar;
    }

    // Compatibility fallback: a missing/failed hot generation must not make a
    // new actor lose its appearance.
    // Deterministic: same NPC + same life (epoch) = same avatar.
    // New life (new epoch) produces a different but stable avatar.
    const std::uint64_t seed = static_cast<std::uint64_t>(npcId) * 65537ULL + transformEpoch;
    std::mt19937 rng(static_cast<std::mt19937::result_type>(seed));
    std::uniform_int_distribution<size_t> dist(0, avatars.size() - 1);
    const std::string selected = avatars[dist(rng)];
    recordAvatarDecision(npcId, transformEpoch, selected, "fallback");
    return selected;
}
}

void pollNpcAvatarConfig()
{
    std::error_code ec;
    const auto write = std::filesystem::last_write_time(kConfigPath, ec);
    if (!gLoaded || (!ec && write != gLastWrite)) {
        gLastWrite = ec ? std::filesystem::file_time_type{} : write;
        gLoaded = true;
        loadConfig();
    }
}

std::string npcAvatarNameForLife(std::uint32_t npcId, std::uint16_t transformEpoch)
{
    pollNpcAvatarConfig();
    return chooseAvatar(npcId, transformEpoch);
}

bool assignNpcAvatar(Npc& npc)
{
    const std::string name = npcAvatarNameForLife(npc.id, npc.transformEpoch);
    if (name.empty()) {
        npc.avatarName.clear();
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] no valid avatar folders; npc=%u retains default appearance\n", npc.id);
        return false;
    }
    // Headless servers have no GL context — skip GPU loading, just store the name.
    // Clients will load the avatar visuals themselves from the name in snapshots.
    if (glfwGetCurrentContext()) {
        if (!AvatarSystem::instance().applyAvatarToPlayer(npc.body, name)) {
            npc.avatarName.clear();
            Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] failed to apply avatar=%s npc=%u\n", name.c_str(), npc.id);
            return false;
        }
        Debug::warn(Debug::Category::Avatar,
            "[NPC AVATAR SERVER] npc=%u avatar='%s' epoch=%u atlas=%u model=%s\n",
            npc.id, name.c_str(), (unsigned)npc.transformEpoch,
            npc.body.avatarInstance ? npc.body.avatarInstance->atlasTexture : 0,
            npc.body.avatarInstance ? npc.body.avatarInstance->definition.playerModel.c_str() : "?");
    } else {
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] headless server, skipping GL apply for avatar=%s npc=%u\n", name.c_str(), npc.id);
    }
    npc.avatarName = name;
    return true;
}
