// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-loader.h
// dec 18 2025
/**
 * purpose
 * json loader stuff
 */

#pragma once

#include "world/world.h"

// jan 5 2026 fix
class TextureManager;

bool loadWorldFromJSON(World& world, TextureManager& tex, const char* path);
