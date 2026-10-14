#pragma once

// 2026-10-14
/* purpose
* Declare the per-avatar rig: named animatable parts (roles, physical flag) and
* the movement capability. Shared by player.h and avatar.h so the avatar
* definition can carry a rig without a circular include.
* See docs/specs/animations/animations.md.
*/

#include <string>
#include <vector>

// Maps a stable animation part name to a GLB node and a role. The six canonical
// humanoid names remain the default when no rig is declared.
struct AvatarRigPart {
    std::string name;    // stable animation key
    std::string node;    // GLB node name (defaults to name); ".NNN" tolerated
    std::string role;    // head|torso|arm|leg|wing|tail|stinger|weapon|helper
    bool physical = false;
};

struct AvatarMovementCap {
    std::string mode = "walk";   // walk|fly|crawl|swim
    float gravityScale = 1.0f;   // 0 = weightless (flyer)
    bool hover = false;          // maintain a target altitude
    float hoverHeight = 1.0f;    // preferred altitude above ground/water
    float altitudeChange = 4.0f; // max vertical command speed while flying
};

struct AvatarRig {
    bool present = false;
    std::vector<AvatarRigPart> parts;
    AvatarMovementCap movement;
};
