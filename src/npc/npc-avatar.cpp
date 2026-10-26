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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "avatar/avatar.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "npc/npc.h"
#include "perf/perf-spike.h"

#include <GLFW/glfw3.h>

namespace {
constexpr const char* kConfigPath = "config/npc-avatar.json";
struct NpcAvatarConfig { bool forceAvatar = false; std::string forceAvatarPath; };
NpcAvatarConfig gConfig;
std::filesystem::file_time_type gLastWrite{};
bool gLoaded = false;
std::vector<std::string> gAvatarChoices;
std::chrono::steady_clock::time_point gAvatarChoicesLastRefresh{};
bool gAvatarChoicesLoaded = false;

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
        root = nlohmann::json::parse(file, nullptr, true, true);
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

const std::vector<std::string>& avatarChoices()
{
    const auto now = std::chrono::steady_clock::now();
    if (!gAvatarChoicesLoaded ||
        now - gAvatarChoicesLastRefresh >= std::chrono::seconds(1)) {
        MIMITA_PERF_SCOPE("NpcAvatar::RefreshChoices");
        gAvatarChoices = AvatarSystem::instance().listAvatars();
        gAvatarChoicesLastRefresh = now;
        gAvatarChoicesLoaded = true;
    }
    return gAvatarChoices;
}

std::string chooseAvatar(std::uint32_t npcId, std::uint16_t transformEpoch)
{
    const std::string forced = forcedAvatarName();
    if (!forced.empty()) return forced;
    const std::vector<std::string>& avatars = avatarChoices();
    if (avatars.empty()) return {};
    // Deterministic: same NPC + same life (epoch) = same avatar.
    // New life (new epoch) produces a different but stable avatar.
    const std::uint64_t seed = static_cast<std::uint64_t>(npcId) * 65537ULL + transformEpoch;
    std::mt19937 rng(static_cast<std::mt19937::result_type>(seed));
    std::uniform_int_distribution<size_t> dist(0, avatars.size() - 1);
    return avatars[dist(rng)];
}

bool applyAvatar(Npc& npc, const std::string& name)
{
    if (name.empty()) {
        npc.avatarName.clear();
        return false;
    }
    if (glfwGetCurrentContext() &&
        !AvatarSystem::instance().applyAvatarToPlayer(npc.body, name)) {
        npc.avatarName.clear();
        Debug::warn(Debug::Category::Avatar,
            "[NPC AVATAR] failed to apply avatar=%s npc=%u\n", name.c_str(), npc.id);
        return false;
    }
    npc.avatarName = name;
    return true;
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
    return applyAvatar(npc, name);
}

bool assignNpcAvatarForMode(Npc& npc, const GamemodeNpcAvatarPolicy& policy,
                            const std::string& modeId, const std::string& preferredName,
                            const std::vector<std::string>& roleAllowed,
                            const std::vector<std::string>& roleDenied)
{
    const std::string previous = npc.avatarName;
    const auto& avatars = avatarChoices();
    const auto permitted = [&](const std::string& name) {
        return std::find(avatars.begin(), avatars.end(), name) != avatars.end() &&
            isAvatarAllowedByPolicy(name, policy.allowed, policy.denied) &&
            isAvatarAllowedByPolicy(name, roleAllowed, roleDenied);
    };
    std::vector<std::string> candidates;
    for (const auto& name : avatars)
        if (permitted(name)) candidates.push_back(name);

    std::string selected;
    const bool keep = policy.spawnPolicy == "keep_per_npc";
    if (!preferredName.empty() && permitted(preferredName))
        selected = preferredName;
    else if (keep && permitted(previous))
        selected = previous;
    else if (!candidates.empty()) {
        const std::uint64_t seed = static_cast<std::uint64_t>(npc.id) * 65537ULL +
            (keep ? 0 : npc.transformEpoch);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(seed));
        std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
        size_t index = dist(rng);
        if (!keep && candidates.size() > 1 && candidates[index] == previous)
            index = (index + 1) % candidates.size();
        selected = candidates[index];
    }
    const bool applied = applyAvatar(npc, selected);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar,
        applied ? StructuredLevel::Important : StructuredLevel::Errors,
        "npc.avatar.spawn-selection", std::to_string(npc.id),
        applied ? "selected" : "no-allowed-avatar", npc.transformEpoch,
        nlohmann::json{{"actor", npc.id}, {"mode", modeId}, {"group", policy.group},
                       {"spawn_policy", policy.spawnPolicy}, {"epoch", npc.transformEpoch},
                       {"previous", previous}, {"preferred", preferredName},
                       {"selected", npc.avatarName}, {"candidate_count", candidates.size()},
                       {"allowed", applied}},
        __FILE__, __LINE__, __FUNCTION__);
    return applied;
}
