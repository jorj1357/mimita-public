#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "combat/weapon-registry.h"
#include "config.h"
#include "physics/config.h"

struct AnimOverlayResult {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
};

static glm::mat4 poseMatrix(const ProceduralPose& pose)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pose.translation);
    m = glm::rotate(m, glm::radians(pose.rotationEuler.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.z), glm::vec3(0, 0, 1));
    return m;
}

static glm::vec3 springVec3(SpringState& spring, const glm::vec3& target, float stiffness, float damping, float dt)
{
    float safeDt = std::min(dt, 0.05f);
    glm::vec3 acceleration = (target - spring.value) * stiffness - spring.velocity * damping;
    spring.velocity += acceleration * safeDt;
    spring.value += spring.velocity * safeDt;
    return spring.value;
}

static std::unordered_map<std::string, AnimOverlayResult> interpolateAnimClip(
    const AnimClip& clip, float timeSeconds, float speedScale = 1.0f)
{
    std::unordered_map<std::string, AnimOverlayResult> result;
    if (clip.keyframes.empty()) return result;

    float tickTime = timeSeconds * 60.0f * speedScale;
    float duration = (float)clip.durationTicks;
    if (duration <= 0.0f) duration = 1.0f;

    float loopedTick = clip.loop ? std::fmod(tickTime, duration) : std::min(tickTime, duration - 1.0f);
    if (loopedTick < 0.0f) loopedTick += duration;

    const AnimKeyframe* prev = &clip.keyframes[0];
    const AnimKeyframe* next = &clip.keyframes[0];

    for (size_t i = 0; i < clip.keyframes.size(); ++i) {
        if ((float)clip.keyframes[i].tick <= loopedTick) prev = &clip.keyframes[i];
        if ((float)clip.keyframes[i].tick >= loopedTick) { next = &clip.keyframes[i]; break; }
    }

    float range;
    float t;

    if (prev == next && clip.loop && clip.keyframes.size() > 1) {
        const AnimKeyframe* last = &clip.keyframes.back();
        prev = last;
        float wrappedDist = (duration - (float)last->tick) + (float)next->tick;
        float currentDist = loopedTick - (float)last->tick;
        if (currentDist < 0.0f) currentDist += duration;
        range = wrappedDist;
        t = (range > 0.001f) ? currentDist / range : 0.0f;
    } else {
        range = (float)(next->tick - prev->tick);
        t = (range > 0.001f) ? (loopedTick - (float)prev->tick) / range : 0.0f;
    }
    t = std::clamp(t, 0.0f, 1.0f);

    std::vector<std::string> allParts;
    for (const auto& p : prev->parts) allParts.push_back(p.first);
    for (const auto& p : next->parts) {
        bool found = false;
        for (const auto& existing : allParts) {
            if (existing == p.first) { found = true; break; }
        }
        if (!found) allParts.push_back(p.first);
    }

    for (const auto& partName : allParts) {
        AnimOverlayResult overlay;
        auto prevIt = prev->parts.find(partName);
        auto nextIt = next->parts.find(partName);

        glm::vec3 prevTrans = (prevIt != prev->parts.end()) ? prevIt->second.translation : glm::vec3(0.0f);
        glm::vec3 nextTrans = (nextIt != next->parts.end()) ? nextIt->second.translation : glm::vec3(0.0f);
        glm::vec3 prevRot = (prevIt != prev->parts.end()) ? prevIt->second.rotation : glm::vec3(0.0f);
        glm::vec3 nextRot = (nextIt != next->parts.end()) ? nextIt->second.rotation : glm::vec3(0.0f);

        overlay.translation = glm::mix(prevTrans, nextTrans, t);
        overlay.rotation = glm::mix(prevRot, nextRot, t);
        result[partName] = overlay;
    }

    return result;
}

static void applyAxisLocks(ProceduralPose& pose, const std::string& partName)
{
    auto it = gPlayerProcedural.axisLocks.find(partName);
    if (it == gPlayerProcedural.axisLocks.end()) return;
    if (!it->second.x) pose.rotationEuler.x = 0.0f;
    if (!it->second.y) pose.rotationEuler.y = 0.0f;
    if (!it->second.z) pose.rotationEuler.z = 0.0f;
}

void Player::updateProceduralAnimation(float dt, const glm::vec3& camForward, const glm::vec3& camPos, bool movementPressed)
{
    updatePlayerProceduralHotReload(dt);

    if (perfectPoseSkeleton.nodes.empty() ||
        perfectPoseSkeleton.restLocalTransforms.size() != perfectPoseSkeleton.nodes.size())
        return;
    if (proceduralFrozen) {
        updateModelWorldTransforms();
        return;
    }

    // Store aim data for weapon positioning
    if (glm::length(camForward) > 0.001f) {
        aimDirection = glm::normalize(camForward);
        aimPosition = camPos;
        hasAimData = true;
    }

    syncLegacyStateToLayers();
    weaponSwayTime += dt;

    glm::vec2 planarVel = glm::vec2(vel.x, vel.y);
    float speed = glm::length(planarVel);
    float move01 = std::min(speed / std::max(PHYS.moveSpeed, 0.001f), 1.6f);
    previousProceduralVelocity = vel;

    // === ANIMATION STATE MACHINE ===
    auto walkClipIt = gPlayerProcedural.layers.animations.find("walk");
    bool walkInputTriggered = (walkClipIt != gPlayerProcedural.layers.animations.end())
        ? walkClipIt->second.inputTriggered : false;
    bool nowMoving = walkInputTriggered ? movementPressed : (move01 >= 0.01f);
    bool wasMoving = previousMove01 >= 0.01f;
    previousMove01 = move01;

    std::string activeAnim = currentAnimName;
    if (nowMoving)
    {
        activeAnim = "walk";
    }
    else if (currentAnimName == "walk")
    {
        if (walkClipIt != gPlayerProcedural.layers.animations.end()
            && walkClipIt->second.tickBasedReturnToIdle
            && gPlayerProcedural.layers.animations.find("return_to_idle")
               != gPlayerProcedural.layers.animations.end())
            activeAnim = "return_to_idle";
        else
            activeAnim = "idle";
    }
    else if (currentAnimName == "return_to_idle")
    {
        auto retClipIt = gPlayerProcedural.layers.animations.find("return_to_idle");
        if (retClipIt != gPlayerProcedural.layers.animations.end())
        {
            float clipDuration = (float)retClipIt->second.durationTicks / 60.0f;
            if (animStateTime >= clipDuration)
                activeAnim = "idle";
        }
        else
        {
            activeAnim = "idle";
        }
    }

    // On state change: reset timer and snap walk to start tick
    if (activeAnim != currentAnimName) {
        currentAnimName = activeAnim;
        if (activeAnim == "walk")
            animStateTime = 1.0f / 60.0f;
        else
            animStateTime = 0.0f;
    }

    animStateTime += dt;

    // === COMPUTE JSON KEYFRAME ANIMATION ===
    auto animIt = gPlayerProcedural.layers.animations.find(activeAnim);
    std::unordered_map<std::string, AnimOverlayResult> animOverlay;
    if (animIt != gPlayerProcedural.layers.animations.end()) {
        float animSpeedScale = 1.0f;
        if (animIt->second.speedBased && animIt->second.speedScaleFromVelocity)
            animSpeedScale = 0.3f + move01 * 1.2f;

        float playbackMultiplier = std::max(0.01f,
            gPlayerProcedural.walkFrequency *
            gPlayerProcedural.walkFrequencyMultiplier);
        animSpeedScale *= playbackMultiplier;

        if (DebugConfig::DEBUG_ANIMATION) {
            printf("[ANIM] state=%s walkFrequency=%.2f finalSpeed=%.2f\n",
                   activeAnim.c_str(), gPlayerProcedural.walkFrequency,
                   animSpeedScale);
        }

        animOverlay = interpolateAnimClip(animIt->second, animStateTime, animSpeedScale);
    }

    // Reload state
    bool isReloading = false;
    for (const auto& pair : weaponRuntimes) {
        if (pair.first == "revolver" || pair.first == "shotgun") {
            if (pair.second.isReloading) {
                isReloading = true;
                break;
            }
        }
    }

    // Weapon state
    bool weaponEquipped = hasValidWeapon && (equippedSlot >= 1);
    std::string weaponId = equippedWeaponId;
    std::string poseLookupId = weaponId;
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        if (def && !def->poseId.empty())
            poseLookupId = def->poseId;
    }
    bool hasWeaponPose = false;
    WeaponPoseConfig* weaponPoseCfg = nullptr;
    if (weaponEquipped) {
        auto wpIt = gPlayerProcedural.weaponPoses.find(poseLookupId);
        if (wpIt != gPlayerProcedural.weaponPoses.end() && wpIt->second.useWeaponPose) {
            hasWeaponPose = true;
            weaponPoseCfg = &wpIt->second;
        }
    }

    // Weapon sway computation (arms only, when weapon has a pose)
    float swayPhase = weaponSwayTime * gPlayerProcedural.weaponSwaySpeed;
    float swayAmount = hasWeaponPose ? gPlayerProcedural.weaponSwayAmount + move01 * 0.1f : 0.0f;
    float swayX = std::sin(swayPhase) * swayAmount;
    float swayY = std::cos(swayPhase * 1.3f) * swayAmount * 0.6f;
    float swayZ = std::sin(swayPhase * 0.7f) * swayAmount * 0.4f;
    float idleSway = hasWeaponPose ? std::sin(weaponSwayTime * gPlayerProcedural.idleSwaySpeed) * gPlayerProcedural.idleSwayAmount : 0.0f;

    // Aim tracking for weapon-equipped upper body
    float aimYaw = 0.0f, aimPitch = 0.0f;
    if (hasAimData && hasWeaponPose) {
        glm::vec3 flatAim = glm::normalize(glm::vec3(aimDirection.x, aimDirection.y, 0.0f));
        glm::vec3 flatForward = glm::normalize(glm::vec3(movementCapsule.rotation * glm::vec3(0,1,0)));
        aimYaw = std::atan2(flatAim.y, flatAim.x) - std::atan2(flatForward.y, flatForward.x);
        while (aimYaw > 3.14159265f) aimYaw -= 2.0f * 3.14159265f;
        while (aimYaw < -3.14159265f) aimYaw += 2.0f * 3.14159265f;
        aimYaw = std::clamp(aimYaw, -1.0f, 1.0f);
        aimPitch = std::asin(std::clamp(aimDirection.z, -1.0f, 1.0f));
        aimPitch = std::clamp(aimPitch, -0.8f, 0.8f);
    }

    // === PERFECT POSE GENERATION ===
    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size())
            continue;

        ProceduralPose target;

        // JSON keyframe values are the base target
        {
            auto ovIt = animOverlay.find(part.name);
            if (ovIt != animOverlay.end()) {
                target.translation = ovIt->second.translation;
                target.rotationEuler = ovIt->second.rotation;
            }
        }

        // Per-weapon pose replaces arm rotation when the weapon defines one
        if (hasWeaponPose && weaponPoseCfg) {
            if (part.name == "leftArm") {
                target.rotationEuler = weaponPoseCfg->leftArm.rotation;
                target.translation += weaponPoseCfg->leftArm.translation;
            } else if (part.name == "rightArm") {
                target.rotationEuler = weaponPoseCfg->rightArm.rotation;
                target.translation += weaponPoseCfg->rightArm.translation;
            }
        }

        // Reload overlay (additive arm lowering)
        if (isReloading && (part.name == "leftArm" || part.name == "rightArm")) {
            target.rotationEuler += gPlayerProcedural.layers.reloadOverlay.rotation;
            target.translation += gPlayerProcedural.layers.reloadOverlay.translation;
        }

        // Axis locks: clamp unwanted axes to 0
        applyAxisLocks(target, part.name);

        // Store the clean perfect pose (exact JSON target, no smoothing)
        part.perfectPose = target;

        // Physical body follows perfect pose with spring smoothing
        part.pose.translation =
            springVec3(part.translationSpring, target.translation, 90.0f, 16.0f, dt);
        part.pose.rotationEuler =
            springVec3(part.rotationSpring, target.rotationEuler, 80.0f, 14.0f, dt);

        // Apply to skeleton for rendering
        perfectPoseSkeleton.nodes[part.nodeIndex].localTransform =
            perfectPoseSkeleton.restLocalTransforms[part.nodeIndex] *
            poseMatrix(part.pose);

        // Sync legacy body parts
        for (BodyPart& legacyPart : bodyParts)
        {
            if (legacyPart.nodeIndex != part.nodeIndex)
                continue;
            legacyPart.pose = part.pose;
            legacyPart.translationSpring = part.translationSpring;
            legacyPart.rotationSpring = part.rotationSpring;
            break;
        }
    }

    // Pose debug logging (always enabled for debug builds)
    if (DebugConfig::DEBUG_ANIMATION) {
        static float poseDebugTimer2 = 0.0f;
        poseDebugTimer2 -= dt;
        if (poseDebugTimer2 <= 0.0f) {
            poseDebugTimer2 = 1.0f;
            const PhysicalBodyPart* leftArm = nullptr;
            const PhysicalBodyPart* rightArm = nullptr;
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name == "leftArm") leftArm = &part;
                if (part.name == "rightArm") rightArm = &part;
            }
            if (leftArm && rightArm) {
                printf("[POSE] weapon=%s usePose=%d equipped=%d "
                       "leftArmFinal=(%.2f %.2f %.2f) rightArmFinal=(%.2f %.2f %.2f)\n",
                       weaponId.c_str(), (int)hasWeaponPose, (int)weaponEquipped,
                       leftArm->pose.translation.x, leftArm->pose.translation.y, leftArm->pose.translation.z,
                       rightArm->pose.translation.x, rightArm->pose.translation.y, rightArm->pose.translation.z);
            }
        }
    }

    // Arm debug logging (enable with `anim_debug_arms 1`)
    if (DebugConfig::DEBUG_ANIM_ARMS) {
        static float armDebugTimer = 0.0f;
        armDebugTimer -= dt;
        if (armDebugTimer <= 0.0f) {
            armDebugTimer = 0.5f;
            printf("[ANIM_ARMS] state=%s weaponEquipped=%d hasWeaponPose=%d weapon=%s\n",
                   activeAnim.c_str(), (int)weaponEquipped, (int)hasWeaponPose, weaponId.c_str());
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name != "leftArm" && part.name != "rightArm") continue;
                printf("[ANIM_ARMS]   %s:\n", part.name.c_str());
                glm::vec3 animTrans(0.0f);
                glm::vec3 animRot(0.0f);
                auto ovIt = animOverlay.find(part.name);
                if (ovIt != animOverlay.end()) {
                    animTrans = ovIt->second.translation;
                    animRot = ovIt->second.rotation;
                }
                printf("[ANIM_ARMS]     animKeyframes trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       animTrans.x, animTrans.y, animTrans.z,
                       animRot.x, animRot.y, animRot.z);
                glm::vec3 wpTrans(0.0f);
                glm::vec3 wpRot(0.0f);
                if (hasWeaponPose && weaponPoseCfg) {
                    if (part.name == "leftArm") {
                        wpTrans = weaponPoseCfg->leftArm.translation;
                        wpRot = weaponPoseCfg->leftArm.rotation;
                    } else {
                        wpTrans = weaponPoseCfg->rightArm.translation;
                        wpRot = weaponPoseCfg->rightArm.rotation;
                    }
                }
                printf("[ANIM_ARMS]     weaponPose    trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       wpTrans.x, wpTrans.y, wpTrans.z,
                       wpRot.x, wpRot.y, wpRot.z);
                printf("[ANIM_ARMS]     perfectPose   trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.perfectPose.translation.x, part.perfectPose.translation.y, part.perfectPose.translation.z,
                       part.perfectPose.rotationEuler.x, part.perfectPose.rotationEuler.y, part.perfectPose.rotationEuler.z);
                printf("[ANIM_ARMS]     physicalPose  trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.pose.translation.x, part.pose.translation.y, part.pose.translation.z,
                       part.pose.rotationEuler.x, part.pose.rotationEuler.y, part.pose.rotationEuler.z);
            }
            const PhysicalBodyPart* leftArm = nullptr;
            const PhysicalBodyPart* rightArm = nullptr;
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name == "leftArm") leftArm = &part;
                if (part.name == "rightArm") rightArm = &part;
            }
            if (leftArm && rightArm) {
                printf("[ANIM_ARMS]   symmetry check:\n");
                printf("[ANIM_ARMS]     leftArm.translation.y  = %.3f  rightArm.translation.y  = %.3f  (should be negated)\n",
                       leftArm->perfectPose.translation.y, rightArm->perfectPose.translation.y);
                printf("[ANIM_ARMS]     leftArm.rotationEuler  = (%.1f %.1f %.1f)  rightArm.rotationEuler = (%.1f %.1f %.1f)\n",
                       leftArm->perfectPose.rotationEuler.x, leftArm->perfectPose.rotationEuler.y, leftArm->perfectPose.rotationEuler.z,
                       rightArm->perfectPose.rotationEuler.x, rightArm->perfectPose.rotationEuler.y, rightArm->perfectPose.rotationEuler.z);
            }
        }
    }

    // Debug logging (enable with `animation_debug 1`)
    if (DebugConfig::DEBUG_ANIMATION) {
        static float debugTimer = 0.0f;
        debugTimer -= dt;
        if (debugTimer <= 0.0f) {
            debugTimer = 1.0f;
            float tickTime = animStateTime * 60.0f;
            printf("[ANIM] state=%s tick=%.2f move01=%.2f keyframes=%zu\n",
                   activeAnim.c_str(), tickTime, move01,
                   animIt != gPlayerProcedural.layers.animations.end() ? animIt->second.keyframes.size() : 0);
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                printf("[ANIM]   perfectPose %s trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.name.c_str(),
                       part.perfectPose.translation.x, part.perfectPose.translation.y, part.perfectPose.translation.z,
                       part.perfectPose.rotationEuler.x, part.perfectPose.rotationEuler.y, part.perfectPose.rotationEuler.z);
                printf("[ANIM]   physical  %s trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.name.c_str(),
                       part.pose.translation.x, part.pose.translation.y, part.pose.translation.z,
                       part.pose.rotationEuler.x, part.pose.rotationEuler.y, part.pose.rotationEuler.z);
            }
        }
    }

    updateModelWorldTransforms();
}
