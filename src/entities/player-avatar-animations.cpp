#include "player-avatar-animations.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils/path_utils.h"
#include "utils/json-file-watch.h"
#include "player-animation-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "perf/perf-spike.h"

namespace {

std::uint64_t fileWriteStamp(const std::string& path)
{
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(path, ec);
    if (ec)
        return 0;
    return (std::uint64_t)t.time_since_epoch().count();
}

// Animation JSON live edit is a presentation feature, not a simulation task.
// Stat-ing every clip and avatar JSON on the gameplay thread caused 20 ms
// fixed-tick stalls on busy scenes. Keep the same 100 ms visibility by doing
// only the directory stamps on a background thread; the main thread validates
// and installs a changed set when that thread publishes a dirty flag.
JsonFileWatch& animationFileWatch()
{
    static JsonFileWatch watch(
        {resolveAssetPath("config/animations"),
         resolveAssetPath("assets/avatars")},
        std::chrono::milliseconds(100));
    return watch;
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
    // Optional per-state clips (absent keys leave the string empty = not provided).
    states.run = s.value("run", states.run);
    states.jump = s.value("jump", states.jump);
    states.fall = s.value("fall", states.fall);
    states.land = s.value("land", states.land);
    states.crouch = s.value("crouch", states.crouch);
    states.crouchWalk = s.value("crouchWalk", states.crouchWalk);
    states.crouchIn = s.value("crouchIn", states.crouchIn);
    states.crouchOut = s.value("crouchOut", states.crouchOut);
    states.dash = s.value("dash", states.dash);
    states.downDash = s.value("downDash", states.downDash);
    states.freeze = s.value("freeze", states.freeze);
    states.knockback = s.value("knockback", states.knockback);
    states.respawn = s.value("respawn", states.respawn);
    states.equip = s.value("equip", states.equip);
    states.hurtSmall = s.value("hurtSmall", states.hurtSmall);
    states.hurtNormal = s.value("hurtNormal", states.hurtNormal);
    states.hurtLarge = s.value("hurtLarge", states.hurtLarge);
    states.deathSmall = s.value("deathSmall", states.deathSmall);
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
void loadIncludes(const nlohmann::json& root, AvatarAnimationSet& set,
                  std::unordered_map<std::string, std::uint64_t>& dependencyWrites);

void loadIncludeEntry(const std::string& entry, AvatarAnimationSet& set,
                      std::unordered_map<std::string, std::uint64_t>& dependencyWrites)
{
    const std::string resolved = resolveAssetPath(entry);
    dependencyWrites[resolved] = fileWriteStamp(resolved);
    std::error_code ec;
    if (entry.size() && entry.back() == '/' && std::filesystem::is_directory(resolved, ec)) {
        std::vector<std::string> files;
        for (const auto& e : std::filesystem::directory_iterator(resolved, ec)) {
            if (e.is_regular_file() && e.path().extension() == ".json")
                files.push_back(e.path().string());
        }
        std::sort(files.begin(), files.end());
        for (const auto& f : files) {
            dependencyWrites[f] = fileWriteStamp(f);
            nlohmann::json j;
            if (!readJsonFile(f, j))
                throw std::runtime_error("included animation file missing or invalid: " + f);
            mergeClips(j, set);
            loadIncludes(j, set, dependencyWrites);
        }
        return;
    }
    nlohmann::json j;
    if (!readJsonFile(resolved, j))
        throw std::runtime_error("included animation file missing or invalid: " + resolved);
    mergeClips(j, set);
    loadIncludes(j, set, dependencyWrites);
}

void loadIncludes(const nlohmann::json& root, AvatarAnimationSet& set,
                  std::unordered_map<std::string, std::uint64_t>& dependencyWrites)
{
    if (!root.contains("include") || !root["include"].is_array())
        return;
    for (const auto& inc : root["include"]) {
        if (inc.is_string())
            loadIncludeEntry(inc.get<std::string>(), set, dependencyWrites);
    }
}

AnimationRuntimeSettings gAnimationSettings;
std::uint64_t gAnimationSettingsWrite = 0;
std::uint64_t gAnimationSettingsPollMs = 0;
bool gAnimationSettingsLoaded = false;
float gAnimationRenderAlpha = 1.0f;

void installDefaultPriorities(AnimationRuntimeSettings& s)
{
    s.statePriorities = {
        {"idle", 0}, {"land", 5}, {"walk", 10}, {"run", 10}, {"jump", 20},
        {"fall", 20}, {"crouch", 30}, {"crouchWalk", 30},
        {"hurt", 60}, {"dash", 70},
        {"downDash", 72}, {"freeze", 74}, {"attack", 80}, {"death", 100}
    };
}

} // namespace

void pollAnimationRuntimeSettings()
{
    const std::uint64_t now = (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (gAnimationSettingsLoaded && now - gAnimationSettingsPollMs < 100)
        return;
    gAnimationSettingsPollMs = now;

    const std::string path = resolveAssetPath("config/animations/settings.json");
    const std::uint64_t write = fileWriteStamp(path);
    if (gAnimationSettingsLoaded && write == gAnimationSettingsWrite)
        return;

    AnimationRuntimeSettings next;
    installDefaultPriorities(next);
    nlohmann::json root;
    bool loaded = false;
    if (readJsonFile(path, root) && root.is_object()) {
        next.rootTranslationMode = root.value("rootTranslationMode", next.rootTranslationMode);
        next.renderPoseInterpolation = root.value("renderPoseInterpolation", next.renderPoseInterpolation);
        next.deathPresentation = root.value("deathPresentation", next.deathPresentation);
        next.hurtTierMode = root.value("hurtTierMode", next.hurtTierMode);
        if (root.contains("hurtPercent") && root["hurtPercent"].is_object()) {
            next.hurtPercentSmallMax = root["hurtPercent"].value("smallMax", next.hurtPercentSmallMax);
            next.hurtPercentNormalMax = root["hurtPercent"].value("normalMax", next.hurtPercentNormalMax);
        }
        if (root.contains("hurtFixed") && root["hurtFixed"].is_object()) {
            next.hurtFixedSmallMax = root["hurtFixed"].value("smallMax", next.hurtFixedSmallMax);
            next.hurtFixedNormalMax = root["hurtFixed"].value("normalMax", next.hurtFixedNormalMax);
        }
        if (root.contains("statePriorities") && root["statePriorities"].is_object()) {
            for (auto it = root["statePriorities"].begin(); it != root["statePriorities"].end(); ++it)
                if (it.value().is_number_integer())
                    next.statePriorities[it.key()] = it.value().get<int>();
        }
        loaded = true;
    }
    if (next.rootTranslationMode != 1 && next.rootTranslationMode != 2)
        next.rootTranslationMode = 1;
    if (next.hurtTierMode != 1 && next.hurtTierMode != 2)
        next.hurtTierMode = 1;
    gAnimationSettings = std::move(next);
    gAnimationSettingsWrite = write;
    gAnimationSettingsLoaded = true;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        "anim.settings.loaded", "config/animations/settings.json",
        loaded ? "animation runtime settings loaded" : "animation runtime settings missing; defaults used",
        0,
        nlohmann::json{{"root_translation_mode", gAnimationSettings.rootTranslationMode},
                       {"render_pose_interpolation", gAnimationSettings.renderPoseInterpolation},
                       {"death_presentation", gAnimationSettings.deathPresentation},
                       {"hurt_tier_mode", gAnimationSettings.hurtTierMode},
                       {"state_priorities", gAnimationSettings.statePriorities}},
        __FILE__, __LINE__, __FUNCTION__);
}

const AnimationRuntimeSettings& animationRuntimeSettings()
{
    pollAnimationRuntimeSettings();
    return gAnimationSettings;
}

int animationStatePriority(const std::string& state)
{
    const auto& priorities = animationRuntimeSettings().statePriorities;
    const auto it = priorities.find(state);
    if (it != priorities.end()) return it->second;
    return 0;
}

float animationRenderAlpha()
{
    return gAnimationRenderAlpha;
}

void setAnimationRenderAlpha(float alpha)
{
    gAnimationRenderAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

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
        rig.movement.force = m.value("force", rig.movement.force);
        rig.movement.mass = m.value("mass", rig.movement.mass);
        rig.movement.maxSpeed = m.value("max_speed", rig.movement.maxSpeed);
        rig.movement.restitution = m.value("restitution", rig.movement.restitution);
        rig.movement.radius = m.value("radius", rig.movement.radius);
        rig.movement.height = m.value("height", rig.movement.height);
        rig.movement.rootOffsetZ = m.value("root_offset_z", rig.movement.rootOffsetZ);
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

AvatarAnimationRegistry::Entry& AvatarAnimationRegistry::load(const std::string& avatarName,
                                                               std::uint32_t activationTick)
{
    Entry& entry = mEntries[avatarName];
    if (entry.loaded)
        return entry;

    // Build a replacement separately. Actors keep pointers into this cache, so
    // install into the existing Entry only after the complete JSON set parses.
    Entry next;
    next.loaded = true;
    next.revision = entry.revision;
    const bool hadValidSet = entry.set.valid;

    const bool sharedProfile = avatarName.rfind("profile:", 0) == 0;
    const std::string dir = sharedProfile ? "" : resolveAssetPath(avatarDir(avatarName));
    const std::string avatarJson = sharedProfile ? "" : dir + "/avatar.json";
    next.avatarWrite = fileWriteStamp(avatarJson);

    std::string animFile = "animations.json";
    nlohmann::json avatarRoot;
    bool inlineProfile = false;
    if (!sharedProfile && readJsonFile(avatarJson, avatarRoot)) {
        parseAvatarRigJson(avatarRoot, next.rig);
        inlineProfile = avatarRoot.contains("animation_profile") &&
                        avatarRoot["animation_profile"].is_object();
        if (avatarRoot.contains("animations") && avatarRoot["animations"].is_string())
            animFile = avatarRoot["animations"].get<std::string>();
    }

    const std::string animPath = sharedProfile
        ? resolveAssetPath(avatarName.substr(8)) :
        inlineProfile ? avatarJson : dir + "/" + animFile;
    next.animPath = animPath;
    next.animWrite = fileWriteStamp(animPath);
    nlohmann::json animRoot;
    bool valid = false;
    try {
        if (inlineProfile)
            animRoot = avatarRoot["animation_profile"];
        else if (!readJsonFile(animPath, animRoot))
            throw std::runtime_error("file missing or invalid JSON");

        // An ordered set supplies shared clips/states before the avatar's own
        // includes and inline overrides, so sets stay reusable and editable.
        if (animRoot.contains("set") && animRoot["set"].is_string()) {
            const std::string setId = animRoot["set"].get<std::string>();
            const std::string setPath = resolveAssetPath(
                "config/animations/masterlist/animation-sets.json");
            next.dependencyWrites[setPath] = fileWriteStamp(setPath);
            nlohmann::json setsRoot;
            if (!readJsonFile(setPath, setsRoot) || !setsRoot.contains("sets") ||
                !setsRoot["sets"].is_object() || !setsRoot["sets"].contains(setId)) {
                throw std::runtime_error("animation set missing or invalid: " + setId);
            }
            const auto& setRoot = setsRoot["sets"][setId];
            loadIncludes(setRoot, next.set, next.dependencyWrites);
            mergeClips(setRoot, next.set);
            parseStateMap(setRoot, next.set.states);
        }

        loadIncludes(animRoot, next.set, next.dependencyWrites);
        mergeClips(animRoot, next.set);
        parseStateMap(animRoot, next.set.states);
        // Exclusive authored-pose policy opt-in (see docs/specs/animations/animations.md).
        if (animRoot.contains("poseComposition") && animRoot["poseComposition"].is_string())
            next.set.authoredOnly = animRoot["poseComposition"].get<std::string>() == "authored_only";
        next.set.valid = !next.set.clips.empty();
        valid = next.set.valid;
        if (!valid)
            throw std::runtime_error("no valid animation clips");
    } catch (const std::exception& e) {
        // Keep the last good set and rig, but remember this file revision so a
        // half-written save is not reparsed every frame. The next save retries.
        next.set = entry.set;
        if (entry.set.valid || entry.rig.present)
            next.rig = entry.rig;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Errors,
            "anim.set.reload_rejected", avatarName,
            "animation JSON rejected; previous valid set retained", 0,
            nlohmann::json{{"avatar", avatarName}, {"anim_file", animPath},
                           {"reason", e.what()}, {"retained_previous", entry.set.valid}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    entry = std::move(next);
    if (!valid)
        return entry;

    ++entry.revision;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        hadValidSet ? "anim.set.reloaded" : "anim.set.loaded", avatarName,
        hadValidSet ? "per-avatar animation set reloaded" : "per-avatar animation set loaded",
        activationTick,
        nlohmann::json{{"avatar", avatarName},
                       {"anim_file", animPath},
                       {"revision", entry.revision},
                       {"activation_tick", activationTick},
                       {"include_paths", [&entry]() {
                           nlohmann::json paths = nlohmann::json::array();
                           for (const auto& dependency : entry.dependencyWrites)
                               paths.push_back(dependency.first);
                           return paths;
                       }()},
                       {"clip_count", entry.set.clips.size()},
                       {"rig_parts", entry.rig.parts.size()},
                       {"movement_mode", entry.rig.movement.mode},
                       {"valid", entry.set.valid}},
        __FILE__, __LINE__, __FUNCTION__);
    return entry;
}

const AvatarAnimationSet* AvatarAnimationRegistry::getAnimationSet(
    const std::string& avatarName, std::uint32_t activationTick)
{
    return &load(avatarName, activationTick).set;
}

const AvatarRig* AvatarAnimationRegistry::getRig(const std::string& avatarName)
{
    return &load(avatarName).rig;
}

void AvatarAnimationRegistry::invalidate(const std::string& avatarName)
{
    // Do not erase cache nodes: live Player instances hold pointers to their
    // AvatarAnimationSet. Mark in place so the next lookup can safely replace it.
    if (avatarName.empty()) {
        for (auto& [name, entry] : mEntries)
            entry.loaded = false;
    } else {
        auto it = mEntries.find(avatarName);
        if (it != mEntries.end())
            it->second.loaded = false;
    }
}

void AvatarAnimationRegistry::pollReload(std::uint32_t activationTick)
{
    const std::uint64_t now = (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (mLastPollMs != 0 && now - mLastPollMs < 100)
        return;
    mLastPollMs = now;

    const bool watchDirty = animationFileWatch().consumeDirty();
    bool hasUnloaded = false;
    if (!watchDirty) {
        for (const auto& [name, entry] : mEntries) {
            (void)name;
            if (!entry.loaded) {
                hasUnloaded = true;
                break;
            }
        }
    }
    if (!watchDirty && !hasUnloaded)
        return;

    MIMITA_PERF_SCOPE("Animation::HotReloadPoll");
    std::vector<std::string> changedAvatars;
    for (auto& [name, entry] : mEntries) {
        bool changed = !entry.loaded ||
            (name.rfind("profile:", 0) != 0 &&
             fileWriteStamp(resolveAssetPath(avatarDir(name)) + "/avatar.json") != entry.avatarWrite) ||
            fileWriteStamp(entry.animPath) != entry.animWrite;
        for (const auto& [path, stamp] : entry.dependencyWrites)
            changed = changed || fileWriteStamp(path) != stamp;
        if (changed) {
            entry.loaded = false;
            changedAvatars.push_back(name);
        }
    }
    // Reload before the tick updates any actors, so everyone using this avatar
    // observes the same complete set for the whole tick.
    for (const auto& name : changedAvatars)
        load(name, activationTick);
}
