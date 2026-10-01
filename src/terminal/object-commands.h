// 2026-09-30
// Exposes registerObjectCommands() for main-systems.cpp.
#pragma once

#include <string>
#include <vector>

// Registers object_spawn and object_spawn_list.
void registerObjectCommands();

// Every .glb in assets/objects/things/physics-objects, alphabetically. Exposed
// so a self-test can verify numbered spawning without a live terminal.
std::vector<std::string> listPhysicsObjectGlbs();