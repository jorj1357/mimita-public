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
#include <unordered_map>

#include "player.h"

class AvatarAnimationRegistry {
public:
    static AvatarAnimationRegistry& instance();

    // The animation set for an avatar name. Never returns null; when no
    // animations file exists the returned set has valid=false so callers fall
    // back to the global profile.
    const AvatarAnimationSet* getAnimationSet(const std::string& avatarName);

    // The rig for an avatar name. present=false when no rig is declared.
    const AvatarRig* getRig(const std::string& avatarName);

    // Drop one entry (or all) so the next get() re-reads from disk.
    void invalidate(const std::string& avatarName = "");

    // Re-read changed per-avatar files (client main loop, 250 ms throttle).
    void pollReload();

private:
    AvatarAnimationRegistry() = default;

    struct Entry {
        AvatarRig rig;
        AvatarAnimationSet set;
        bool loaded = false;
        std::uint64_t avatarWrite = 0;
        std::uint64_t animWrite = 0;
        std::uint64_t includeWrite = 0;
    };

    Entry& load(const std::string& avatarName);

    std::unordered_map<std::string, Entry> mEntries;
    std::uint64_t mLastPollMs = 0;
};
