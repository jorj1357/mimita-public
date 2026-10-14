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
    std::string mode = "walk";   // walk|fly|crawl|swim|roll
    float gravityScale = 1.0f;   // 0 = weightless (flyer)
    bool hover = false;          // maintain a target altitude
    float hoverHeight = 1.0f;    // preferred altitude above ground/water
    float altitudeChange = 4.0f; // max vertical command speed while flying

    // roll mode: a physical body driven by a force in the look direction.
    float force = 50.0f;         // newtons applied along camera forward/strafe
    float mass = 1.0f;           // kg; acceleration = force / mass
    float maxSpeed = 999999.0f;  // hard speed clamp (m/s)
    float restitution = 0.6f;    // bounce energy retained on impact (0..1)
    // Collision body size (0 = engine default). A spike-ball avatar sets a
    // large radius so it collides and bounces as a ball.
    float radius = 0.0f;
    float height = 0.0f;
    // Offset applied to the model root in the model frame. A ball whose origin
    // is its center uses this to put the origin at the bottom so it rests on
    // the ground instead of being ejected upward.
    float rootOffsetZ = 0.0f;
};

struct AvatarRig {
    bool present = false;
    std::vector<AvatarRigPart> parts;
    AvatarMovementCap movement;
};
