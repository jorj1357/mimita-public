#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct GamemodePresentation;
struct MatchRoleDefinition;

namespace ActorPresetWeapons {

enum class PresentationFlag : uint8_t {
    DamageNumbers,
    HitEffects,
    WorldImpactEffects,
    Blood,
    HitMarkers,
    HitSounds
};

// Builds an in-memory weapon table from the normal weapon registry plus the
// selected actor-preset overrides. The source JSON files are never modified.
bool apply(const MatchRoleDefinition& preset);
bool refresh();
void clear();
void setModePresentationOverride(const GamemodePresentation& presentation);
void clearModePresentationOverride();
std::optional<bool> modePresentationOverride(PresentationFlag flag);
std::optional<GamemodePresentation> modePresentationSettings();
const std::string& activePresetId();
bool avatarAllowedForActivePreset(const std::string& avatar);
bool damageNumbersEnabled();
bool hitEffectsEnabled();
bool worldImpactEffectsEnabled();
bool bloodEffectsEnabled();
bool muzzleFlashEnabled();
bool hitMarkersEnabled();
bool hitSoundsEnabled();

} // namespace ActorPresetWeapons
