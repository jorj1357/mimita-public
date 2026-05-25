// C:\important\quiet\n\mimita-priv-v7\src\entities\player.h
// feb 10 2026 CLEANED: slim + physics-safe

#pragma once
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include "map/map_common.h"
#include "physics/physics-types.h"

// ---------------- Player ----------------
//
// Player is pure data + presentation.
// 
// Owns:
// - state (pos, vel, flags)
// - audio triggers
// - rendering
//
// Does NOT own:
// - gravity
// - movement
// - jumping
// - collisions
//

struct OBB {
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::mat4 orientation;
};

class Player {
public:
    // -------- Core State --------
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec2 dashVel{0.0f};

    bool onGround = false;
    float yaw = 0.0f;

    // -------- Jump --------
    int airJumpsLeft = 1;
    bool jumpHeldPrev = false;
    // so we dont use air jump on ground
    bool airJumpLocked = false;
    // so we press space to air jump, not let go of space
    bool airJumpArmed = false;

    // -------- Dash --------
    // mar 8 2026, no dashcharges, its airjump style, touch object = get a dash back
    // int dashCharges = 3;
    // mar 7 2026 we need to remove cooldowns i think?
    // i want less cooldowns less waiting no waiting at all
    // but spamming moves intrinsically limits itself
    // bc dash will go infinite, but u gain so much speed that u end up crashing into a wall
    // and dying
    // removing the need for a artificial cooldown
    // plauers will cool themselves down
    // float dashRechargeTimer = 0.0f;
    // float dashCooldown = 0.0f;
    // no holding it to dash forever 1000/sec
    bool dashHeldPrev = false;

    // reset dash when touching object mar 8 2026
    bool dashAvailable = true;

    // -------- Ground Return --------
    // old mar 8 2026
    // uncommented but whatever set to 0 mar 8 2026
    // int   groundReturnCharges = 3;
    int   groundReturnCharges = 0;
    float groundReturnRechargeTimer = 0.0f;

    // new cool mar 8 2026 reset when touching smth 
    bool groundReturnAvailable = true;

    // -------- Freeze --------
    bool freezeAvailable = true;
    bool freezeHeldPrev = false;
    bool freezeActive = false;

    float freezeTimer = 0.0f;

    // -------- Freeze sounds --------
    bool freezeHoldSoundPlayed = false;

    // -------- One-Frame Events (set by physics) --------
    bool didGroundJump = false;
    bool didAirJump    = false;
    bool didDash       = false;
    bool didLand       = false;

    // -------- Audio helpers --------
    bool  wasOnGround = false;
    float footstepTimer = 0.0f;

    // -------- Rendering --------
    glm::vec3 meshScale  {1,1,1};
    glm::vec3 meshOffset {0,0,0};
    Mesh renderMesh;
    bool modelLoaded = false;
    std::vector<TransformNode> nodes;
    std::vector<Collider> bodyColliders;

    // -------- Construction --------
    Player();
    void reset();
    bool loadModel(const char* path);
    void updateModelWorldTransforms();

    // -------- Queries --------
    Capsule getCapsule() const;
    OBB     getOBB() const;

    // -------- Systems --------
    void updateAudio(float dt);
    void render(unsigned int shader,
                const glm::mat4& view,
                const glm::mat4& proj) const;
};
