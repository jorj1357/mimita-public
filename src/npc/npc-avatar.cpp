// 08 22 2026, 12 35
/* purpose
* Loads NPC avatar selection settings and applies a chosen avatar to an NPC.
* Resolves forced avatar.json paths or randomly selects valid avatar folders.
* Uses the existing AvatarSystem so body textures and cosmetics share game behavior.
* Does NOT save avatar data or change the local player's configured avatar.
* Does NOT own NPC spawn timing or respawn rules.
* Does NOT serialize replay actor data.
*/
#include "npc/npc-avatar.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "avatar/avatar.h"
#include "debug/debug-log.h"
#include "npc/npc.h"

namespace {
constexpr const char* kConfigPath = "config/npc-avatar.json";
struct NpcAvatarConfig { bool forceAvatar = false; std::string forceAvatarPath; };
NpcAvatarConfig gConfig;
std::filesystem::file_time_type gLastWrite{};
bool gLoaded = false;

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
    if (!forced.empty()) return forced;
    const std::vector<std::string> avatars = AvatarSystem::instance().listAvatars();
    if (avatars.empty()) return {};
    uint32_t random = npcId ^ (static_cast<uint32_t>(transformEpoch) * 747796405u);
    random = random * 1664525u + 1013904223u;
    return avatars[random % avatars.size()];
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
    AvatarSystem& avatars = AvatarSystem::instance();
    if (!avatars.loadAvatar(name) || !avatars.applyToPlayer(npc.body, true)) {
        npc.avatarName.clear();
        Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] failed to apply avatar=%s npc=%u\n", name.c_str(), npc.id);
        return false;
    }
    npc.avatarName = name;
    Debug::warn(Debug::Category::Avatar, "[NPC AVATAR] assigned npc=%u avatar=%s epoch=%u\n", npc.id, npc.avatarName.c_str(), (unsigned)npc.transformEpoch);
    return true;
}
