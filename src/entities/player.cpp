// feb 10 2026 CLEANED : slim + correct
// Split into player-config.cpp, player-loader.cpp,
// player-animation.cpp, player-render.cpp

#include "player.h"
#include "combat/weapon-runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

#include "audio/audio.h"
#include "avatar/character-registry.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "physics/config.h"

// =====================================================
// Player
// =====================================================

static const char* PLAYER_GLB_PATH = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
static const char* DEFAULT_CHARACTER = "DefaultGuy";

glm::quat yawRotation(float yawDegrees)
{
    return glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
}

glm::mat4 transformMatrix(const glm::vec3& position, const glm::quat& rotation)
{
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
}

Player::Player()
    : Player(true)
{
}

Player::Player(bool loadRenderModel)
{
    if (loadRenderModel)
    {
        std::string charName = GetPlayerSettings().characterName;
        if (charName.empty())
            charName = DEFAULT_CHARACTER;
        if (!loadCharacter(charName))
            loadModel(PLAYER_GLB_PATH);
    }
    reset();
}

void Player::reset()
{
    // pos = {0,0,50};
    // debug test
    // pos = {1,5,2};
    // debug test 2 for the ctf map
    // pos = {1,5,30};
    pos = {1,5,60};
    vel = {0,0,0};
    externalImpulse = {0,0,0};
    ground.onGround = false;
    currentHp = maxHp;
    dead = false;
    proceduralFrozen = false;
    respawnTimer = 0.0f;
    spawnFlashTimer = 0.0f;
    killedBy.clear();
    respawnPosition = pos;

    jump.jumpHeldPrev = false;
    dash.moveHeldPrev = false;
    dash.dashHeldPrev = false;
    jump.jumpIntentTimer = 0.0f;
    jump.coyoteTimer = 0.0f;
    jump.airJumpsLeft = 1;
    groundReturn.charges = GROUND_RETURN_MAX_CHARGES;

    freeze.freezeTimer = 0.0f;
    freeze.freezeActive = false;
    freeze.freezeAvailable = true;
    freeze.freezeHeldPrev = false;
    freeze.freezeHoldSoundPlayed = false;

    previousProceduralVelocity = glm::vec3(0.0f);
    proceduralTime = 0.0f;
    animStateTime = 0.0f;
    currentAnimName = "idle";
    dashPoseTimer = -1.0f;
    forceDashPose = false;
    freezePoseTimer = -1.0f;
    freezePoseActive = false;
    equippedWeaponId.clear();
    collision.hasWeaponCollisionCapsule = false;
    weaponCollisionCapsule = Capsule{};
    weaponCollisionName.clear();

    for (BodyPart& part : bodyParts)
    {
        part.pose = ProceduralPose{};
        part.translationSpring = SpringState{};
        part.rotationSpring = SpringState{};
    }

    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        part.pose = ProceduralPose{};
        part.perfectPose = ProceduralPose{};
        part.translationSpring = SpringState{};
        part.rotationSpring = SpringState{};
    }

    resetAllWeaponRuntimesForSpawn(*this, "Player::reset");
    syncLegacyStateToLayers();
    updateModelWorldTransforms();
}

void Player::syncLegacyStateToLayers()
{
    origin.position = pos;
    origin.rotation = yawRotation(yaw);

    movementCapsule.position = origin.position;
    movementCapsule.rotation = origin.rotation;
    movementCapsule.velocity = vel;
    movementCapsule.radius = PLAYER_RADIUS;
    movementCapsule.height = PLAYER_HEIGHT;
}

void Player::syncLayersToLegacyState()
{
    pos = origin.position;
    vel = movementCapsule.velocity;
}

void Player::updateModelWorldTransforms()
{
    syncLegacyStateToLayers();

    // Save previous transforms for limb sweep collision
    for (PhysicalBodyPart& part : physicalBody.parts)
        part.previousWorldTransform = part.worldTransform;

    glm::mat4 rootWorld = transformMatrix(movementCapsule.position, movementCapsule.rotation);

    for (int i = 0; i < (int)perfectPoseSkeleton.nodes.size(); ++i)
    {
        TransformNode& poseNode = perfectPoseSkeleton.nodes[i];
        if (poseNode.parent < 0)
            poseNode.worldTransform = rootWorld * poseNode.localTransform;
        else
            poseNode.worldTransform = perfectPoseSkeleton.nodes[poseNode.parent].worldTransform * poseNode.localTransform;

        if (i < (int)nodes.size())
        {
            nodes[i].localTransform = poseNode.localTransform;
            nodes[i].worldTransform = poseNode.worldTransform;
        }
    }

    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        if (part.nodeIndex >= 0 && part.nodeIndex < (int)perfectPoseSkeleton.nodes.size())
            part.worldTransform = perfectPoseSkeleton.nodes[part.nodeIndex].worldTransform;
    }
}

Capsule Player::getCapsule() const
{
    Capsule c;
    c.r = movementCapsule.radius > 0.0f ? movementCapsule.radius : PLAYER_RADIUS;

    float height = movementCapsule.height > 0.0f ? movementCapsule.height : PLAYER_HEIGHT;
    float half = height * 0.5f;
    glm::vec3 center = movementCapsule.position;
    if (glm::length(center - pos) > 0.0001f)
        center = pos;
    c.a = center - glm::vec3(0,0,half - c.r);
    c.b = center + glm::vec3(0,0,half - c.r);

    return c;
}

OBB Player::getOBB() const
{
    OBB b;
    b.center = pos;
    b.halfSize = glm::vec3(PLAYER_WIDTH,PLAYER_DEPTH,PLAYER_HEIGHT) * 0.5f;
    b.orientation = glm::rotate(glm::mat4(1.0f),
                                glm::radians(-yaw),
                                glm::vec3(0,0,1));
    return b;
}

void Player::updateAudio(float dt)
{
    // Jump sound debounce: prevent frame-after-frame spam during wall climb.
    // Only play if enough time has passed since the last jump sound.
    jump.jumpSoundTimer = std::max(0.0f, jump.jumpSoundTimer - dt);

    if (jump.didGroundJump) {
        if (jump.jumpSoundTimer <= 0.0f) {
            playWorldSound("entity/player/jump", pos, 1.0f, 1.0f, 28.0f);
            jump.jumpSoundTimer = 0.08f;
        }
        glm::vec3 jumpDir = glm::length(inputWishMove) > 0.001f
            ? glm::normalize(glm::vec3(inputWishMove.x, inputWishMove.y, 0.0f))
            : glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 groundJumpPos = pos;
        groundJumpPos.z -= 0.5f;
        HitEffects::spawnGroundJumpBurst(groundJumpPos, jumpDir);
    }

    if (jump.didAirJump) {
        if (jump.jumpSoundTimer <= 0.0f) {
            playAirJumpSound();
            jump.jumpSoundTimer = 0.08f;
        }
        glm::vec3 jumpDir = glm::length(inputWishMove) > 0.001f
            ? glm::normalize(glm::vec3(inputWishMove.x, inputWishMove.y, 0.0f))
            : glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 airJumpPos = pos;
        airJumpPos.z -= 1.0f;
        HitEffects::spawnAirJumpBurst(airJumpPos, jumpDir);
    }

    if (dash.didDash) {
        bool perfect = (dash.lastDashQuality == 0);
        playWorldSound("entity/player/dash", pos, perfect ? 1.3f : 1.0f, perfect ? 1.2f : 1.0f, 36.0f);
        glm::vec3 dashDir = glm::length(vel) > 0.001f ? glm::normalize(vel) : glm::vec3(0,1,0);
        HitEffects::spawnMovementDashBurst(pos, dashDir, glm::length(vel));
        dash.lastDashQuality = 0;
    }

    if (freeze.didFreeze) {
        playWorldSound("entity/player/freezebegin", pos, 1.0f, 1.0f, 30.0f);
        glm::vec3 freezePos = pos;
        freezePos.z -= 0.3f;
        EffectPartSystem::instance().spawnFreeze(freezePos, freeze.freezeTimer);
    }

    if (freeze.freezeActive) {
        EffectPartSystem::instance().spawnFreezeTrail(pos);
    }

    if (dash.didDownDash) {
        glm::vec3 downDashPos = pos;
        downDashPos.z -= 0.3f;
        EffectPartSystem::instance().spawnDownDash(downDashPos);
    }

    // Landing: sound + directional VFX
    if (ground.didLand) {
        playWorldSound("entity/player/land", pos, 1.0f, 1.0f, 32.0f);
        glm::vec3 landDir = glm::length(inputWishMove) > 0.001f
            ? glm::normalize(glm::vec3(inputWishMove.x, inputWishMove.y, 0.0f))
            : glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 landPos = pos;
        landPos.z -= 0.3f;
        HitEffects::spawnLandingBurst(landPos, landDir, glm::length(glm::vec2(vel.x, vel.y)));
    }

    // Walk VFX: directional ground spheres offset opposite travel direction
    glm::vec2 xy = glm::vec2(vel.x,vel.y);
    float speed = glm::length(xy);
    if (ground.stableOnGround && speed > 0.5f) {
        footstepTimer -= dt;
        if (footstepTimer <= 0.0f) {
            playWorldSound("entity/player/walk" + std::to_string(1 + rand() % 4), pos, 0.8f, 1.0f, 22.0f);
            Capsule cap = getCapsule();
            glm::vec3 footPos = cap.a;
            footPos.z -= cap.r;
            EffectPartSystem::instance().spawnFootstep(footPos);
            // Walk burst: opposite direction of travel
            glm::vec3 walkDir = glm::length(inputWishMove) > 0.001f
                ? glm::normalize(glm::vec3(inputWishMove.x, inputWishMove.y, 0.0f))
                : glm::vec3(0.0f, 0.0f, 0.0f);
            HitEffects::spawnWalkBurst(pos, -walkDir, speed);
            footstepTimer = 0.35f;
        }
    } else {
        footstepTimer = 0.0f;
    }

    jump.didGroundJump = jump.didAirJump = dash.didDash = dash.didDownDash = ground.didLand = freeze.didFreeze = false;
}

void Player::takeDamage(int damage, const glm::vec3& knockbackDir, float knockbackForce)
{
    printf("[APPLY DAMAGE] target=%s hpBefore=%d damage=%d\n",
           username.c_str(), currentHp, damage);

    if (damage <= 0) {
        printf("[APPLY DAMAGE] damage <= 0, abort\n");
        return;
    }
    
    int oldHp = currentHp;
    currentHp = std::max(0, currentHp - damage);
    int actualDamage = oldHp - currentHp;
    
    if (actualDamage <= 0) {
        printf("[APPLY DAMAGE] no actual damage (already dead?)\n");
        return;
    }

    printf("[APPLY DAMAGE] hpAfter=%d actualDamage=%d\n", currentHp, actualDamage);
    
    // Play hurt sound with volume/pitch based on damage
    float severity = std::clamp((float)actualDamage / 100.0f, 0.0f, 1.0f);
    float vol, pit;
    computeImpactAudio(1.2f, 0.0f, severity, vol, pit);
    playWorldSound("player_hurt", pos, vol, pit, 60.0f);
    Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=player_hurt damage=%d severity=%.2f pitch=%.2f volume=%.2f\n",
               actualDamage, severity, pit, vol);
    
    // Apply knockback to external velocity (cleared on movement input)
    if (knockbackForce > 0.0f && glm::length(knockbackDir) > 0.001f) {
        externalImpulse += glm::normalize(knockbackDir) * knockbackForce;
        externalImpulse.z += knockbackForce * 0.2f; // Slight upward knockback
    }
    
    // Spawn blood effect at player position
    {
        HitEvent ev;
        ev.position = pos;
        ev.normal = glm::vec3(0, 0, 1);
        ev.direction = glm::length(knockbackDir) > 0.001f ? glm::normalize(knockbackDir) : glm::vec3(0, 0, 1);
        ev.hitEntity = true;
        ev.damage = actualDamage;
        ev.attacker = "world";
        ev.victim = username;
        ev.weaponSource = "player_take_damage";
        HitEffects::onHit(ev);
    }
    
    if (DebugConfig::DEBUG_COMMANDS) {
        Debug::log(Debug::Category::General, "[PLAYER HURT] damage=%d hp=%d/%d severity=%.2f\n",
                   actualDamage, currentHp, maxHp, severity);
    }
}
