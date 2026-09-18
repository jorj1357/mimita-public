// 09 17 2026
/* purpose
* Declares the JSON-versus-C++ movement preset comparison harness.
* Loads each preset from config/movement/*.json as comparison-only reference,
* builds the same preset from the hot C++ registry, then requires matching
* MovementConfig values and identical fixed-60Hz simulation outcomes (position,
* velocity, and movement events) through the shared movement step.
* Does NOT run the game or own movement policy.
*/
#pragma once

#include <string>

bool runMovementPresetParitySelfTest(std::string& report);
