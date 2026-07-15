#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "combat/weapon-registry.h"
#include "config.h"
#include "physics/config.h"
#include "debug/debug-log.h"

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

    static bool loggedSkeleton = false;
    if (!loggedSkeleton) {
        loggedSkeleton = true;
        Debug::log(Debug::Category::Animation,
            "[ANIM PROFILE] entity=%s single global animation profile (gPlayerProcedural) nodes=%zu bodyParts=%zu\n",
            username.c_str(), perfectPoseSkeleton.nodes.size(), physicalBody.parts.size());
        for (const PhysicalBodyPart& bp : physicalBody.parts) {
            Debug::log(Debug::Category::Animation,
                "[ANIM BONE] %s -> nodeIndex=%d name=%s\n",
                bp.name.c_str(), bp.nodeIndex,
                bp.nodeIndex >= 0 && bp.nodeIndex < (int)perfectPoseSkeleton.nodes.size()
                    ? perfectPoseSkeleton.nodes[bp.nodeIndex].name.c_str() : "INVALID");
        }
    }

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

    if (activeAnim != currentAnimName) {
        currentAnimName = activeAnim;
        if (activeAnim == "walk")
            animStateTime = 1.0f / 60.0f;
        else
            animStateTime = 0.0f;
    }

    animStateTime += dt;

    // === DASH POSE TIMER ===
    if (dash.didDash)
    {
        dashPoseTimer = 0.0f;
        dash.didDash = false;
    }

    float dashWeight = 0.0f;
    if (forceDashPose) {
        dashWeight = 1.0f;
    } else if (dashPoseTimer >= 0.0f) {
        const auto& dp = gPlayerProcedural.dashPose;
        if (dp.snapIn && dashPoseTimer == 0.0f) {
            dashWeight = 1.0f;
            dashPoseTimer = dp.blendInTime;
        }
        dashPoseTimer += dt;
        float holdDuration = 0.15f;
        float totalDuration = dp.blendInTime + holdDuration + dp.blendOutTime;
        if (dashPoseTimer <= dp.blendInTime) {
            float t = dashPoseTimer / dp.blendInTime;
            dashWeight = t * t * (3.0f - 2.0f * t);
        } else if (dashPoseTimer <= dp.blendInTime + holdDuration) {
            dashWeight = 1.0f;
        } else if (dashPoseTimer <= totalDuration) {
            float t = (dashPoseTimer - dp.blendInTime - holdDuration) / dp.blendOutTime;
            dashWeight = 1.0f - t * t * (3.0f - 2.0f * t);
        } else {
            dashWeight = 0.0f;
            dashPoseTimer = -1.0f;
        }
    }

    // === FREEZE POSE TIMER ===
    bool wasFreezeActive = freezePoseActive;
    freezePoseActive = freeze.freezeActive;
    if (freeze.freezeActive && !wasFreezeActive)
        freezePoseTimer = 0.0f;
    else if (!freeze.freezeActive && wasFreezeActive)
        freezePoseTimer = 0.0f;

    float freezeWeight = 0.0f;
    if (freezePoseTimer >= 0.0f) {
        freezePoseTimer += dt;
        const auto& fp = gPlayerProcedural.freezePose;
        if (freeze.freezeActive) {
            if (fp.snapIn) {
                freezeWeight = 1.0f;
                freezePoseTimer = fp.blendInTime;
            } else if (freezePoseTimer <= fp.blendInTime) {
                float t = freezePoseTimer / fp.blendInTime;
                freezeWeight = t * t * (3.0f - 2.0f * t);
            } else {
                freezeWeight = 1.0f;
            }
        } else {
            if (freezePoseTimer <= fp.blendOutTime) {
                float t = freezePoseTimer / fp.blendOutTime;
                freezeWeight = 1.0f - t * t * (3.0f - 2.0f * t);
            } else {
                freezeWeight = 0.0f;
                freezePoseTimer = -1.0f;
            }
        }
    }

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
            Debug::log(Debug::Category::Animation,
                "[ANIM] state=%s walkFrequency=%.2f finalSpeed=%.2f move01=%.2f nowMoving=%d\n",
                activeAnim.c_str(), gPlayerProcedural.walkFrequency,
                animSpeedScale, move01, (int)nowMoving);
        }

        animOverlay = interpolateAnimClip(animIt->second, animStateTime, animSpeedScale);
    }

    // Weapon state
    bool weaponEquipped = hasValidWeapon && (equippedSlot >= 1);
    std::string weaponId = equippedWeaponId;
    std::string poseLookupId = weaponId;
    const WeaponRuntime* currentWeaponRuntime = nullptr;
    auto runtimeIt = weaponRuntimes.find(weaponId);
    if (runtimeIt != weaponRuntimes.end())
        currentWeaponRuntime = &runtimeIt->second;
    bool isReloading = currentWeaponRuntime && currentWeaponRuntime->isReloading;
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        if (def && !def->poseId.empty())
            poseLookupId = def->poseId;
    }

    auto tryWeaponPoseState = [&](const char* stateName) {
        const std::string stateKey = poseLookupId + ":" + stateName;
        auto stateIt = gPlayerProcedural.weaponPoses.find(stateKey);
        if (stateIt == gPlayerProcedural.weaponPoses.end() || !stateIt->second.useWeaponPose)
            return false;
        poseLookupId = stateKey;
        return true;
    };
    if (weaponEquipped && currentWeaponRuntime) {
        auto equipIt = currentWeaponRuntime->customFloats.find("equipTimer");
        const bool equipping = equipIt != currentWeaponRuntime->customFloats.end() && equipIt->second > 0.0f;
        if (equipping) {
            if (!tryWeaponPoseState("equipping"))
                tryWeaponPoseState("equip");
        } else if (isReloading) {
            if (!tryWeaponPoseState("reloading"))
                tryWeaponPoseState("reload");
        } else if (currentWeaponRuntime->shootEffectTimer > 0.0f) {
            // Allow weapon to override pose via custom float (used by Swordsword)
            auto poseIt = currentWeaponRuntime->customFloats.find("swordPoseState");
            if (poseIt != currentWeaponRuntime->customFloats.end()) {
                if (poseIt->second == 1.0f)
                    tryWeaponPoseState("slash");
                else if (poseIt->second == 2.0f)
                    tryWeaponPoseState("lunge");
                else
                    tryWeaponPoseState("shooting");
            } else if (!tryWeaponPoseState("shooting")) {
                tryWeaponPoseState("fire");
            }
        } else if (currentWeaponRuntime->fireCooldown > 0.0f) {
            if (!tryWeaponPoseState("cooldown"))
                tryWeaponPoseState("just_shot");
        } else if (!tryWeaponPoseState("idle")) {
            tryWeaponPoseState("equipped");
        }
    }

    bool hasWeaponPose = false;
    WeaponPoseConfig* weaponPoseCfg = nullptr;
    if (weaponEquipped) {
        auto wpIt = gPlayerProcedural.weaponPoses.find(poseLookupId);
        if (wpIt != gPlayerProcedural.weaponPoses.end() && wpIt->second.useWeaponPose) {
            hasWeaponPose = true;
            weaponPoseCfg = &wpIt->second;
            Debug::log(Debug::Category::Animation,
                "[ANIM] selected weapon pose poseLookupId=%s weaponId=%s\n",
                poseLookupId.c_str(), weaponId.c_str());
        }
    }

    // === PERFECT POSE GENERATION ===
    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size())
            continue;

        ProceduralPose target;

        {
            auto ovIt = animOverlay.find(part.name);
            if (ovIt != animOverlay.end()) {
                target.translation = ovIt->second.translation;
                target.rotationEuler = ovIt->second.rotation;
            }
        }

        if (hasWeaponPose && weaponPoseCfg) {
            if (part.name == "leftArm") {
                target.rotationEuler = weaponPoseCfg->leftArm.rotation;
                target.translation += weaponPoseCfg->leftArm.translation;
            } else if (part.name == "rightArm") {
                target.rotationEuler = weaponPoseCfg->rightArm.rotation;
                target.translation += weaponPoseCfg->rightArm.translation;
            }
        }

        if (isReloading && (part.name == "leftArm" || part.name == "rightArm")) {
            target.rotationEuler += gPlayerProcedural.layers.reloadOverlay.rotation;
            target.translation += gPlayerProcedural.layers.reloadOverlay.translation;
        }

        // Freeze pose blending (overrides base, before dash/weapon)
        if (freezeWeight > 0.0f) {
            const auto& fp = gPlayerProcedural.freezePose;
            auto blend = [&](const glm::vec3& rot, const glm::vec3& trans) {
                target.rotationEuler = glm::mix(target.rotationEuler, rot, freezeWeight);
                target.translation = glm::mix(target.translation, trans, freezeWeight);
            };
            if (part.name == "torso")
                blend(fp.torsoRotation, fp.torsoTranslation);
            else if (part.name == "head")
                blend(fp.headRotation, fp.headTranslation);
            else if (part.name == "leftArm")
                blend(fp.leftArmRotation, fp.leftArmTranslation);
            else if (part.name == "rightArm")
                blend(fp.rightArmRotation, fp.rightArmTranslation);
            else if (part.name == "leftLeg")
                blend(fp.leftLegRotation, fp.leftLegTranslation);
            else if (part.name == "rightLeg")
                blend(fp.rightLegRotation, fp.rightLegTranslation);
        }

        if (dashWeight > 0.0f) {
            bool partHasWeaponPose = hasWeaponPose && (part.name == "leftArm" || part.name == "rightArm");
            if (!partHasWeaponPose) {
                const auto& dp = gPlayerProcedural.dashPose;
                if (part.name == "torso") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.torsoRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.torsoTranslation, dashWeight);
                } else if (part.name == "head") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.headRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.headTranslation, dashWeight);
                } else if (part.name == "leftArm") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.leftArmRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.leftArmTranslation, dashWeight);
                } else if (part.name == "rightArm") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.rightArmRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.rightArmTranslation, dashWeight);
                } else if (part.name == "leftLeg") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.leftLegRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.leftLegTranslation, dashWeight);
                } else if (part.name == "rightLeg") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.rightLegRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.rightLegTranslation, dashWeight);
                }
            }
        }

        // Axis locks: clamp unwanted axes to 0
        applyAxisLocks(target, part.name);

        if (activeAnim == "idle") {
            auto& c = gPlayerProcedural;
            float t = weaponSwayTime, str = c.idleDebugStrength;
            float breathMult = freeze.freezeActive ? 0.2f : 1.0f;
            if (part.name == "leftArm")
                target.rotationEuler.x += std::sin(t * c.idleArmSpeed) * c.idleArmRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "rightArm")
                target.rotationEuler.x += std::sin(t * c.idleArmSpeed + 1.5f) * c.idleArmRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "leftLeg")
                target.rotationEuler.x += std::sin(t * c.idleLegSpeed) * c.idleLegRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "rightLeg")
                target.rotationEuler.x += std::sin(t * c.idleLegSpeed + 2.0f) * c.idleLegRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "torso") {
                target.rotationEuler.z += std::sin(t * c.idleTorsoSpeed) * c.idleTorsoRotationDeg * str;
                target.translation.y += std::sin(t * c.idleBreathingSpeed) * c.idleBreathingAmount * breathMult * str;
            } else if (part.name == "head")
                target.rotationEuler.x += std::sin(t * c.idleHeadSpeed) * c.idleHeadRotationDeg * str;
        }

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

    if (DebugConfig::DEBUG_ANIMATION) {
        static float poseDebugTimer2 = 0.0f;
        poseDebugTimer2 -= dt;
        if (poseDebugTimer2 <= 0.0f) {
            poseDebugTimer2 = 1.0f;
            const PhysicalBodyPart* leftArm = nullptr;
            const PhysicalBodyPart* rightArm = nullptr;
            const PhysicalBodyPart* head = nullptr;
            const PhysicalBodyPart* torso = nullptr;
            const PhysicalBodyPart* leftLeg = nullptr;
            const PhysicalBodyPart* rightLeg = nullptr;
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name == "leftArm") leftArm = &part;
                if (part.name == "rightArm") rightArm = &part;
                if (part.name == "head") head = &part;
                if (part.name == "torso") torso = &part;
                if (part.name == "leftLeg") leftLeg = &part;
                if (part.name == "rightLeg") rightLeg = &part;
            }
            Debug::log(Debug::Category::Animation,
                "[ANIM] state=%s weapon=%s usePose=%d equipped=%d skeleton=%zu nodes move01=%.2f\n",
                activeAnim.c_str(), weaponId.c_str(), (int)hasWeaponPose, (int)weaponEquipped,
                perfectPoseSkeleton.nodes.size(), move01);
            if (head)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] head perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    head->perfectPose.translation.x, head->perfectPose.translation.y, head->perfectPose.translation.z,
                    head->perfectPose.rotationEuler.x, head->perfectPose.rotationEuler.y, head->perfectPose.rotationEuler.z);
            if (torso)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] torso perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    torso->perfectPose.translation.x, torso->perfectPose.translation.y, torso->perfectPose.translation.z,
                    torso->perfectPose.rotationEuler.x, torso->perfectPose.rotationEuler.y, torso->perfectPose.rotationEuler.z);
            if (leftArm)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] leftArm perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)  physicalPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    leftArm->perfectPose.translation.x, leftArm->perfectPose.translation.y, leftArm->perfectPose.translation.z,
                    leftArm->perfectPose.rotationEuler.x, leftArm->perfectPose.rotationEuler.y, leftArm->perfectPose.rotationEuler.z,
                    leftArm->pose.translation.x, leftArm->pose.translation.y, leftArm->pose.translation.z,
                    leftArm->pose.rotationEuler.x, leftArm->pose.rotationEuler.y, leftArm->pose.rotationEuler.z);
            if (rightArm)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] rightArm perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)  physicalPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    rightArm->perfectPose.translation.x, rightArm->perfectPose.translation.y, rightArm->perfectPose.translation.z,
                    rightArm->perfectPose.rotationEuler.x, rightArm->perfectPose.rotationEuler.y, rightArm->perfectPose.rotationEuler.z,
                    rightArm->pose.translation.x, rightArm->pose.translation.y, rightArm->pose.translation.z,
                    rightArm->pose.rotationEuler.x, rightArm->pose.rotationEuler.y, rightArm->pose.rotationEuler.z);
            if (leftLeg)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] leftLeg perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    leftLeg->perfectPose.translation.x, leftLeg->perfectPose.translation.y, leftLeg->perfectPose.translation.z,
                    leftLeg->perfectPose.rotationEuler.x, leftLeg->perfectPose.rotationEuler.y, leftLeg->perfectPose.rotationEuler.z);
            if (rightLeg)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] rightLeg perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    rightLeg->perfectPose.translation.x, rightLeg->perfectPose.translation.y, rightLeg->perfectPose.translation.z,
                    rightLeg->perfectPose.rotationEuler.x, rightLeg->perfectPose.rotationEuler.y, rightLeg->perfectPose.rotationEuler.z);
        }
    }

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
