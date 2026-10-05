// Generic JSON-defined gamemode runtime - community mode-pack manifest.
//
// A mode pack is pure data: identity, capabilities, disasters, and the
// references (maps/weapons/actors/assets/HUD/audio) the runtime needs. Adding
// a new mode or disaster is adding a validated manifest, never a new
// mode-name branch in server code.
//
// This file does NOT load files, execute behavior, or own match state.
// The loader lives in mode-pack-registry.{h,cpp}.

#pragma once

#include <string>
#include <vector>

namespace MimitaGamemode {

// One disaster a mode can run. `durationSeconds` bounds the disaster; when it
// ends without a winner the runtime resolves a deterministic winner. The
// `weaponPool` is the logical weapon-id set a per-actor assignment may pick.
struct DisasterDefinition
{
    std::string id;
    std::string name;
    std::string description;
    float durationSeconds = 0.0f;
    std::vector<std::string> weaponPool;
    std::string winPolicy;  // e.g. "last_actor_alive"
};

struct ModePack
{
    int schemaVersion = 0;
    std::string id;
    std::string name;
    std::string description;
    // Bridge to config/gamemodes/<gamemode_id>.json (the shared lifecycle
    // values). Empty means the pack id is the gamemode id.
    std::string gamemodeId;
    bool enabled = true;

    int intermissionSeconds = 0;
    int countdownSeconds = 0;
    int resultsSeconds = 0;

    // Presentation references resolved by the client HUD/audio owners.
    std::string hudLayoutId;
    std::string bannerAudio;

    // Stable capability ids (see capability-registry.h).
    std::vector<std::string> capabilities;
    std::vector<DisasterDefinition> disasters;

    // Declared references. The loader only requires that they are well-formed;
    // the runtime resolves them lazily so a missing map does not fail a load.
    std::vector<std::string> referencedMaps;
    std::vector<std::string> referencedWeapons;
    std::vector<std::string> referencedActors;
    std::vector<std::string> referencedAssets;
};

} // namespace MimitaGamemode
