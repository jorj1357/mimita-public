// 09 23 2026
/* purpose
* Hot audio-policy owner: load the logical-sound recipe snapshot from
* config/audio-recipes.json and turn a recipe key plus plain-data overrides into
* one GameAudioCommandV2 through the stable audio.play capability. The mixer,
* device, and voice lifetime stay in the EXE.
* Does NOT own the device, decoded resources, or the low-level voice table.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"

// Optional per-call overrides. Defaults leave the recipe value untouched.
struct HotAudioOverrideV1 {
    float volumeScale = 1.0f;
    float pitchScale = 1.0f;
    float volumeBase = -1.0f;   // >=0 overrides recipe base volume
    float pitchBase = -1.0f;    // >=0 overrides recipe base pitch
    std::uint64_t seed = 0;     // 0 => use ctx->tick for variant/jitter
    // Optional explicit logical sound id; when set it overrides the recipe's
    // sound list while the recipe still owns policy. Null/empty = use recipe.
    const char* sound = nullptr;
};

// Resolve `recipeKey` against the last valid recipe snapshot and emit one
// audio.play command. Returns true when a recipe matched and a command was sent.
// Never touches the device directly.
bool hotEmitRecipeSound(GameplayContextV1* ctx, std::uint64_t recipeKey,
                        const float position[3], std::uint64_t ownerEntity,
                        bool spatial, const HotAudioOverrideV1* overrides);

// Set (start/replace) or stop a logical persistent slot keyed by
// (ownerEntity, slotId). The kernel maps it to a physical voice; a SET replaces
// any voice already in that slot (idempotent per desired state). No raw voice
// handle crosses the boundary.
bool hotEmitRecipeSlot(GameplayContextV1* ctx, std::uint64_t recipeKey,
                       std::uint64_t ownerEntity, std::uint64_t slotId,
                       bool loop, const HotAudioOverrideV1* overrides);
bool hotStopRecipeSlot(GameplayContextV1* ctx, std::uint64_t ownerEntity,
                       std::uint64_t slotId);

// Read-only diagnostics for tests and `audio status`.
bool hotAudioRecipeExists(std::uint64_t recipeKey);
std::uint32_t hotAudioRecipeCount();

#endif // MIMITA_GAME_DLL
