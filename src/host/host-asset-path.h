#pragma once

#include <string>

// Resident asset lookup with host-specific names so the migration EXE can also
// link the game-owned path_utils.cpp implementation.
std::string hostExecutableDirectory();
std::string hostResolveAssetPath(std::string relativePath);
