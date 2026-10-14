#pragma once

#include <string>

#include "player.h"
#include <nlohmann/json.hpp>

bool animationConfigChanged();
void loadAnimationConfig(PlayerProceduralConfig& loaded);

// Parse a single clip object (with keyframes/parts) into an AnimClip.
// Shared by the global config and the per-avatar animation registry.
AnimClip parseAnimClipJson(const nlohmann::json& j);
