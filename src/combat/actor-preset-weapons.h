#pragma once

#include <string>

struct MatchRoleDefinition;

namespace ActorPresetWeapons {

// Builds an in-memory weapon table from the normal weapon registry plus the
// selected actor-preset overrides. The source JSON files are never modified.
bool apply(const MatchRoleDefinition& preset);
bool refresh();
void clear();
const std::string& activePresetId();
bool damageNumbersEnabled();
bool hitEffectsEnabled();
bool worldImpactEffectsEnabled();
bool bloodEffectsEnabled();
bool muzzleFlashEnabled();

} // namespace ActorPresetWeapons
