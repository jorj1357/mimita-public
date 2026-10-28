#pragma once

// 2026-10-14
/* purpose
* Resolve and cache the per-avatar rig and animation set declared in
* assets/avatars/<name>/avatar.json and assets/avatars/<name>/animations.json.
* Merge shared clips from the config/animations library via "include".
* Provide one profile per avatar name so players, NPCs, and remote replicas run
* the same clips (see docs/specs/animations/animations.md).
* Does NOT evaluate clips, does not own the state machine, does not render.
*/

#include <string>
#include <cstdint>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "player.h"

// Parse the avatar.json "rig" object into an AvatarRig. Shared by the sync
// avatar loader, the per-instance parser, and the animation registry.
void parseAvatarRigJson(const nlohmann::json& root, AvatarRig& out);

// Runtime animation settings, hot-reloaded from config/animations/settings.json.
struct AnimationRuntimeSettings {
    int rootTranslationMode = 1;
    bool renderPoseInterpolation = true;
    std::string deathPresentation = "auto";
    int hurtTierMode = 1;
    float hurtPercentSmallMax = 15.0f;
    float hurtPercentNormalMax = 30.0f;
    int hurtFixedSmallMax = 10;
    int hurtFixedNormalMax = 50;
    std::unordered_map<std::string, int> statePriorities;
};

const AnimationRuntimeSettings& animationRuntimeSettings();
void pollAnimationRuntimeSettings();
int animationStatePriority(const std::string& state);
// Presentation-only interpolation factor between the last two fixed poses.
float animationRenderAlpha();
void setAnimationRenderAlpha(float alpha);

class AvatarAnimationRegistry {
public:
    static AvatarAnimationRegistry& instance();

    // The animation set for an avatar name. Never returns null; when no
    // animations file exists the returned set has valid=false so callers fall
    // back to the global profile.
    const AvatarAnimationSet* getAnimationSet(const std::string& avatarName,
                                              std::uint32_t activationTick = 0);

    // The rig for an avatar name. present=false when no rig is declared.
    const AvatarRig* getRig(const std::string& avatarName);

    // Bounded cache-size counters for performance evidence.
    std::size_t cacheSize() const { return mEntries.size(); }
    std::size_t cachedClipCount() const {
        std::size_t count = 0;
        for (const auto& item : mEntries)
            count += item.second.set.clips.size();
        return count;
    }

    // Drop one entry (or all) so the next get() re-reads from disk.
    void invalidate(const std::string& avatarName = "");

    // Re-read changed per-avatar files (live actor update path, 100 ms throttle).
    void pollReload(std::uint32_t activationTick = 0);

private:
    AvatarAnimationRegistry() = default;

    struct Entry {
        AvatarRig rig;
        AvatarAnimationSet set;
        bool loaded = false;
        std::uint64_t avatarWrite = 0;
        std::uint64_t animWrite = 0;
        std::uint64_t revision = 0;
        std::string animPath;
        std::unordered_map<std::string, std::uint64_t> dependencyWrites;
    };

    Entry& load(const std::string& avatarName, std::uint32_t activationTick = 0);

    std::unordered_map<std::string, Entry> mEntries;
    std::uint64_t mLastPollMs = 0;
};
