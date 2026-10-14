#include "player-avatar-animations.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "utils/path_utils.h"
#include "player-animation-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

namespace {

std::uint64_t fileWriteStamp(const std::string& path)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec)
        return 0;
    const auto t = std::filesystem::last_write_time(path, ec);
    if (ec)
        return 0;
    return (std::uint64_t)t.time_since_epoch().count();
}

bool readJsonFile(const std::string& path, nlohmann::json& out)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    try {
        out = nlohmann::json::parse(file, nullptr, true, true);
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Animation,
            "[ANIM] failed to parse %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

std::string avatarDir(const std::string& avatarName)
{
    return "assets/avatars/" + avatarName;
}

void parseStateMap(const nlohmann::json& root, AvatarStateMap& states)
{
    if (!root.contains("states") || !root["states"].is_object())
        return;
    const auto& s = root["states"];
    states.def = s.value("default", states.def);
    if (s.contains("locomotion") && s["locomotion"].is_object()) {
        const auto& l = s["locomotion"];
        states.walk = l.value("walk", states.walk);
        states.fly = l.value("fly", states.fly);
        states.crawl = l.value("crawl", states.crawl);
        states.swim = l.value("swim", states.swim);
    }
    states.attack = s.value("attack", states.attack);
    states.hurt = s.value("hurt", states.hurt);
    states.death = s.value("death", states.death);
}

void mergeClips(const nlohmann::json& root,
                AvatarAnimationSet& set)
{
    if (!root.contains("clips") || !root["clips"].is_object())
        return;
    for (auto it = root["clips"].begin(); it != root["clips"].end(); ++it)
        set.clips[it.key()] = parseAnimClipJson(it.value());
}

// Recurse into "include" entries: a .json file or a directory (trailing '/').
void loadIncludes(const nlohmann::json& root, AvatarAnimationSet& set);

void loadIncludeEntry(const std::string& entry, AvatarAnimationSet& set)
{
    const std::string resolved = resolveAssetPath(entry);
    std::error_code ec;
    if (entry.size() && entry.back() == '/' && std::filesystem::is_directory(resolved, ec)) {
        std::vector<std::string> files;
        for (const auto& e : std::filesystem::directory_iterator(resolved, ec)) {
            if (e.is_regular_file() && e.path().extension() == ".json")
                files.push_back(e.path().string());
        }
        std::sort(files.begin(), files.end());
        for (const auto& f : files) {
            nlohmann::json j;
            if (readJsonFile(f, j)) {
                mergeClips(j, set);
                loadIncludes(j, set);
            }
        }
        return;
    }
    nlohmann::json j;
    if (readJsonFile(resolved, j)) {
        mergeClips(j, set);
        loadIncludes(j, set);
    }
}

void loadIncludes(const nlohmann::json& root, AvatarAnimationSet& set)
{
    if (!root.contains("include") || !root["include"].is_array())
        return;
    for (const auto& inc : root["include"]) {
        if (inc.is_string())
            loadIncludeEntry(inc.get<std::string>(), set);
    }
}

} // namespace

void parseAvatarRigJson(const nlohmann::json& root, AvatarRig& rig)
{
    rig = AvatarRig{};  // never accumulate across parses
    if (!root.contains("rig") || !root["rig"].is_object())
        return;
    const auto& j = root["rig"];
    rig.present = true;

    if (j.contains("movement") && j["movement"].is_object()) {
        const auto& m = j["movement"];
        rig.movement.mode = m.value("mode", rig.movement.mode);
        rig.movement.gravityScale = m.value("gravity_scale", rig.movement.gravityScale);
        rig.movement.hover = m.value("hover", rig.movement.hover);
        rig.movement.hoverHeight = m.value("hover_height", rig.movement.hoverHeight);
        rig.movement.altitudeChange = m.value("altitude_change", rig.movement.altitudeChange);
    }

    if (!j.contains("parts") || !j["parts"].is_array())
        return;
    for (const auto& p : j["parts"]) {
        if (!p.is_object())
            continue;
        AvatarRigPart part;
        part.name = p.value("name", "");
        if (part.name.empty())
            continue;
        part.node = p.value("node", part.name);
        part.role = p.value("role", "");
        // Six canonical names are physical by default; anything else is not.
        const bool canonical =
            part.name == "head" || part.name == "torso" ||
            part.name == "leftArm" || part.name == "rightArm" ||
            part.name == "leftLeg" || part.name == "rightLeg";
        part.physical = p.value("physical", canonical);
        rig.parts.push_back(std::move(part));
    }
}

AvatarAnimationRegistry& AvatarAnimationRegistry::instance()
{
    static AvatarAnimationRegistry registry;
    return registry;
}

AvatarAnimationRegistry::Entry& AvatarAnimationRegistry::load(const std::string& avatarName)
{
    Entry& entry = mEntries[avatarName];
    if (entry.loaded)
        return entry;
    entry.loaded = true;

    const std::string dir = resolveAssetPath(avatarDir(avatarName));
    const std::string avatarJson = dir + "/avatar.json";
    entry.avatarWrite = fileWriteStamp(avatarJson);

    std::string animFile = "animations.json";
    nlohmann::json avatarRoot;
    if (readJsonFile(avatarJson, avatarRoot)) {
        parseAvatarRigJson(avatarRoot, entry.rig);
        if (avatarRoot.contains("animations") && avatarRoot["animations"].is_string())
            animFile = avatarRoot["animations"].get<std::string>();
    }

    const std::string animPath = dir + "/" + animFile;
    entry.animWrite = fileWriteStamp(animPath);
    nlohmann::json animRoot;
    if (!readJsonFile(animPath, animRoot)) {
        entry.set.valid = false;
        return entry;
    }

    entry.includeWrite = entry.animWrite;
    loadIncludes(animRoot, entry.set);
    mergeClips(animRoot, entry.set);
    parseStateMap(animRoot, entry.set.states);
    entry.set.valid = !entry.set.clips.empty();

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        "anim.set.loaded", avatarName,
        "per-avatar animation set loaded", 0,
        nlohmann::json{{"avatar", avatarName},
                       {"anim_file", animPath},
                       {"clip_count", entry.set.clips.size()},
                       {"rig_parts", entry.rig.parts.size()},
                       {"movement_mode", entry.rig.movement.mode},
                       {"valid", entry.set.valid}},
        __FILE__, __LINE__, __FUNCTION__);
    return entry;
}

const AvatarAnimationSet* AvatarAnimationRegistry::getAnimationSet(const std::string& avatarName)
{
    return &load(avatarName).set;
}

const AvatarRig* AvatarAnimationRegistry::getRig(const std::string& avatarName)
{
    return &load(avatarName).rig;
}

void AvatarAnimationRegistry::invalidate(const std::string& avatarName)
{
    if (avatarName.empty())
        mEntries.clear();
    else
        mEntries.erase(avatarName);
}

void AvatarAnimationRegistry::pollReload()
{
    const std::uint64_t now = (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (mLastPollMs != 0 && now - mLastPollMs < 250)
        return;
    mLastPollMs = now;

    for (auto& [name, entry] : mEntries) {
        if (!entry.loaded)
            continue;
        const std::string dir = resolveAssetPath(avatarDir(name));
        if (fileWriteStamp(dir + "/avatar.json") != entry.avatarWrite ||
            fileWriteStamp(dir + "/animations.json") != entry.animWrite) {
            entry.loaded = false;
        }
    }
}
