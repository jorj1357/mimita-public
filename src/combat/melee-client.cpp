// 2026-10-17
/* purpose
* Implements the local client Melee action driver. See melee-client.h.
*/

#include "melee-client.h"

#include "camera.h"
#include "entities/player.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "network/community-server-config.h"
#include "audio/audio.h"
#include "debug/structured-log.h"
#include "debug/debug-visuals.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

namespace MimitaMelee {
namespace {

using WeaponData::MeleeAction;
using WeaponData::MeleeActorState;
using WeaponData::MeleeButton;
using WeaponData::MeleeModifier;
using WeaponData::MeleeMoveSet;

// Test/dev forced action (agent.* commands) and limb-pose tracking for logging.
std::string g_forceActionId;
bool g_debugCrouch = false;
uint32_t g_lastChainsawRevTick = 0;
std::unordered_map<std::string, glm::vec3> g_lastPartPos;
std::unordered_map<std::string, glm::vec3> g_lastPartTip;
std::string g_lastPhaseKey;

struct ClientActionState {
    bool active = false;
    std::string definitionId;
    std::string actionId;         // current action id (may be the double-click upgrade)
    std::string basePrimaryId;    // base primary action id for the double-click window
    uint32_t startSimTick = 0;
    int64_t lastPrimarySimTick = -1000000;
    int64_t lastSecondarySimTick = -1000000;
    std::string baseSecondaryId;
    glm::vec3 forward{1.0f, 0.0f, 0.0f};
    uint32_t lastAdvanceSimTick = 0;
    bool hasLastAdvance = false;
    // Per-tick impulse guard: the last phase tick processed.
    int lastImpulsePhaseTick = -1;
    int lastImpulsePhaseIndex = -1; // 0=windup 1=active 2=recovery
    int currentPhaseIndex = -1;
    int currentPhaseTick = -1;
    // Snapshot of the action data at start (retains the starting revision).
    WeaponData::MeleeAction actionSnapshot;
};

ClientActionState g_state;
bool g_shieldHeld = false;
bool g_debugShield = false;
ClientInput g_pendingInput;
bool g_primaryHeld = false, g_secondaryHeld = false;

int actionRequestSlot(const WeaponDefinition& def, const MimitaNet::MultiplayerContext& ctx)
{
    if (ctx.communityWeaponSetId > 0) {
        auto& config = MimitaNet::CommunityServerConfig::instance();
        if (config.weaponSets().empty()) config.load();
        const int logical = config.slotForWeapon(ctx.communityWeaponSetId, def.id);
        if (logical > 0) return logical;
    }
    return def.slot;
}

glm::vec3 horizontalForward(const glm::vec3& raw)
{
    glm::vec3 forward(raw.x, raw.y, 0.0f);
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
        glm::length(forward) < 0.001f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(forward);
}

const MeleeMoveSet* equippedMoveSet(const Player& player, WeaponSystem& weapons)
{
    const WeaponDefinition* def = weapons.getCurrentDef(player);
    if (!def)
        return nullptr;
    return WeaponData::MeleeMoveSetRegistry::instance().get(def->id);
}

const MeleeAction* resolveAction(const MeleeMoveSet& moveSet,
                                 MeleeButton button,
                                 MeleeActorState state,
                                 MeleeModifier modifier)
{
    const MeleeAction* best = nullptr;
    for (const MeleeAction& a : moveSet.actions) {
        if (a.tapCount != 1) continue;
        if (a.button != button)
            continue;
        if (a.actorState != MeleeActorState::Any && a.actorState != state)
            continue;
        if (a.modifier != modifier)
            continue;
        if (!best || a.priority > best->priority)
            best = &a;
    }
    return best;
}

glm::vec3 impulseWorld(const glm::vec3& forward, const WeaponData::MeleeImpulse& imp)
{
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    right = glm::normalize(right);
    return forward * imp.forward + right * imp.right + up * imp.up;
}

// One Melee-move sound channel per actor: retriggering cuts the previous Melee
// sound on that actor and starts the new one from the beginning. Music and
// unrelated sounds continue. The action definition is the sole sound owner.
void playMeleeSound(uint32_t actorId, const std::string& name, float pitch, float volume,
                    const glm::vec3& position, const glm::vec3& offset)
{
    if (name.empty() || actorId == 0)
        return;
    AudioEvent event{name, AudioCategory::Impacts, true, position + offset,
                     volume, pitch, 40.0f, audioOwnerKey(actorId, "melee"),
                     0.0f, 0.0f, true};
    AudioManager::instance().play(event);
}

// Logs the world position of each hitbox-attached body part so limb motion is
// measurable in events.jsonl (spec: hitbox follows the animated part).
void logMeleeLimbPose(Player& player, const MeleeAction& action, const char* event,
                      const char* phase, uint32_t simTick)
{
    nlohmann::json parts = nlohmann::json::array();
    for (const WeaponData::MeleeHitbox& hb : action.hitboxes) {
        for (const PhysicalBodyPart& p : player.physicalBody.parts) {
            if (p.name != hb.attachedPart)
                continue;
            const glm::vec3 wp(p.worldTransform[3][0], p.worldTransform[3][1], p.worldTransform[3][2]);
            // Limb tip vertex (collider max) in world space — moves under rotation.
            const glm::vec3 localTip(
                (p.collider.localMin.x + p.collider.localMax.x) * 0.5f,
                (p.collider.localMin.y + p.collider.localMax.y) * 0.5f,
                p.collider.localMax.z);
            const glm::vec3 tip = glm::vec3(p.worldTransform * glm::vec4(localTip, 1.0f));
            const glm::vec3 rotDeg = glm::degrees(glm::eulerAngles(glm::quat_cast(glm::mat3(p.worldTransform))));
            auto it = g_lastPartPos.find(p.name);
            const glm::vec3 prev = it != g_lastPartPos.end() ? it->second : wp;
            const glm::vec3 delta = wp - prev;
            auto tipIt = g_lastPartTip.find(p.name);
            const glm::vec3 prevTip = tipIt != g_lastPartTip.end() ? tipIt->second : tip;
            const glm::vec3 tipDelta = tip - prevTip;
            g_lastPartPos[p.name] = wp;
            g_lastPartTip[p.name] = tip;
            parts.push_back({{"part", p.name},
                             {"world", {wp.x, wp.y, wp.z}},
                             {"pivot_delta", {delta.x, delta.y, delta.z}},
                             {"tip_world", {tip.x, tip.y, tip.z}},
                             {"tip_delta", {tipDelta.x, tipDelta.y, tipDelta.z}},
                             {"tip_moved_m", glm::length(tipDelta)},
                             {"world_rot_deg", {rotDeg.x, rotDeg.y, rotDeg.z}}});
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        event, action.id, "melee limb pose sample", simTick,
        {{"action", action.id}, {"phase", phase}, {"parts", parts}});
}

void startAction(Player& player,
                 MimitaNet::MultiplayerContext& ctx,
                 const WeaponDefinition& def,
                 const MeleeAction& action,
                 int actionIndex,
                 const glm::vec3& cameraFront,
                 uint32_t simTick,
                 bool isPrimary,
                 const std::string& basePrimaryId,
                 const char* source)
{
    g_state.active = true;
    g_state.definitionId = def.id;
    g_state.actionId = action.id;
    g_state.actionSnapshot = action;
    g_state.startSimTick = simTick;
    g_state.forward = horizontalForward(cameraFront);
    if (def.id == "chainsaw")
        g_lastChainsawRevTick = simTick;
    g_state.lastAdvanceSimTick = simTick;
    g_state.hasLastAdvance = true;
    g_state.lastImpulsePhaseTick = -1;
    g_state.lastImpulsePhaseIndex = -1;
    g_lastPartPos.clear();
    g_lastPartTip.clear();
    g_lastPhaseKey.clear();
    if (isPrimary) {
        g_state.lastPrimarySimTick = action.tapCount == 2 ? -1000000 : (int64_t)simTick;
        g_state.basePrimaryId = basePrimaryId;
    } else if (action.button == MeleeButton::Secondary) {
        g_state.lastSecondarySimTick = action.tapCount == 2 ? -1000000 : (int64_t)simTick;
        g_state.baseSecondaryId = basePrimaryId;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        "melee.client.action", action.id, "melee action started locally", simTick,
        {{"action", action.id}, {"index", actionIndex}, {"source", source},
         {"weapon", def.id}, {"actor", ctx.localPlayerId}, {"actor_type", "player"},
         {"forward", {g_state.forward.x, g_state.forward.y, g_state.forward.z}},
         {"windup_ticks", action.windup.ticks},
         {"active_ticks", action.active.ticks},
         {"recovery_ticks", action.recovery.ticks}});
    logMeleeLimbPose(player, action, "melee.client.limb", "start", simTick);

    // Send the intent to the server through the generic AttackRequest path. The
    // variant carries the action index; the reserved bytes carry the move-set
    // revision so the server can align revisions before accepting the action.
    const uint16_t netId = MimitaNet::weaponDefNetworkIdFor(def.id);
    uint16_t revision = 0;
    if (const MeleeMoveSet* ms = WeaponData::MeleeMoveSetRegistry::instance().get(def.id))
        revision = (uint16_t)(ms->revision & 0xffff);
    if (netId != 0) {
        MimitaNet::mpSendAttackRequest(
            ctx, netId, (int16_t)actionRequestSlot(def, ctx),
            player.pos, g_state.forward, player.pos,
            (uint8_t)std::clamp(actionIndex, 0, 255),
            0u, glm::vec3(0.0f), 0, revision);
    }

    // Action start sound plays at action tick 0 and cuts the prior Melee sound.
    playMeleeSound(ctx.localPlayerId, action.onStartSound, action.onStartPitch,
                   action.onStartVolume, player.pos, action.onStartOffset);
}

void endAction(Player& player)
{
    g_state.active = false;
    g_state.actionId.clear();
    player.weaponPoseActionState.clear();
    player.clearOneShot();
    player.proceduralMeleePoseActive = false;
}

} // namespace

bool hasEquippedMoveSet(const Player& player, WeaponSystem& weapons)
{
    return equippedMoveSet(player, weapons) != nullptr;
}

bool shieldHeld()
{
    return g_shieldHeld;
}

void reset()
{
    g_state = ClientActionState{};
    g_shieldHeld = false;
    g_debugShield = false;
    g_forceActionId.clear();
    g_lastChainsawRevTick = 0;
    g_pendingInput = ClientInput{};
    g_primaryHeld = g_secondaryHeld = false;
    g_lastPartPos.clear();
    g_lastPartTip.clear();
    g_lastPhaseKey.clear();
}

void drawActiveHitbox(const Player& player, const Camera& camera)
{
    if (!g_state.active || g_state.currentPhaseIndex != 1 ||
        g_state.currentPhaseTick < 0)
        return;

    const MeleeAction& action = g_state.actionSnapshot;
    for (const WeaponData::MeleeHitbox& authoredHitbox : action.hitboxes) {
        if (g_state.currentPhaseTick < authoredHitbox.activeStartTick ||
            g_state.currentPhaseTick > authoredHitbox.activeEndTick)
            continue;
        const WeaponData::MeleeHitbox hitbox =
            WeaponData::resolveMeleeHitboxForTick(authoredHitbox, g_state.currentPhaseTick);
        if (hitbox.shape != "sphere") continue;

        for (const PhysicalBodyPart& part : player.physicalBody.parts) {
            if (part.name != hitbox.attachedPart)
                continue;
            const glm::vec3 center = glm::vec3(
                part.worldTransform * glm::vec4(hitbox.localOffset, 1.0f));
            const float radius = std::max(0.01f, hitbox.size.x);
            const glm::vec4 color{1.0f, 0.0f, 0.0f, 0.5f};
            DebugVis::drawFilledSphere(camera, center, radius, color);
            DebugVis::drawWireSphere(camera, center, radius, {1.0f, 0.0f, 0.0f, 0.9f});
            break;
        }
    }
}

void debugRequestAction(const std::string& actionId)
{
    g_forceActionId = actionId;
}

void debugSetCrouch(bool crouch)
{
    g_debugCrouch = crouch;
}

bool debugCrouchHeld()
{
    return g_debugCrouch;
}

void debugSetShield(bool shield)
{
    g_debugShield = shield;
}

bool debugShieldHeld()
{
    return g_debugShield;
}

void requestButton(bool secondary, bool held)
{
    bool& previous = secondary ? g_secondaryHeld : g_primaryHeld;
    if (held && !previous) {
        if (secondary) g_pendingInput.secondaryPressed = true;
        else g_pendingInput.primaryPressed = true;
    }
    previous = held;
}

void clientTick(Player& player,
                const Camera& camera,
                WeaponSystem& weapons,
                MimitaNet::MultiplayerContext& ctx,
                const ClientInput& rawInput,
                uint32_t simTick)
{
    ClientInput input = rawInput;
    input.primaryPressed = input.primaryPressed || g_pendingInput.primaryPressed;
    input.secondaryPressed = input.secondaryPressed || g_pendingInput.secondaryPressed;
    input.secondaryHeld = input.secondaryHeld || g_secondaryHeld;
    g_pendingInput = ClientInput{};
    const MeleeMoveSet* moveSet = equippedMoveSet(player, weapons);
    if (!moveSet || player.dead) {
        if (g_state.active)
            endAction(player);
        g_shieldHeld = false;
        g_state = ClientActionState{};
        return;
    }
    if (!g_state.definitionId.empty() && g_state.definitionId != moveSet->definitionId) {
        endAction(player);
        g_state = ClientActionState{};
    }
    g_shieldHeld = !moveSet->animationOnly && (input.shieldHeld || g_debugShield);
    // Publish crouch intent so the animation state machine can select the
    // authored `crouch` state clip on the grounded fighter.
    player.crouching = input.crouchHeld || g_debugCrouch;

    if (const WeaponDefinition* equipped = weapons.getCurrentDef(player)) {
        if (equipped->id == "chainsaw") {
            float rev = 1.0f;
            const float decayTicks = std::max(1.0f,
                equipped->customParams.count("revDecayTicks")
                    ? equipped->customParams.at("revDecayTicks") : 60.0f);
            const float maxRev = std::max(1.0f,
                equipped->customParams.count("revDamageMultiplier")
                    ? equipped->customParams.at("revDamageMultiplier") : 2.0f);
            if (g_lastChainsawRevTick != 0 && simTick >= g_lastChainsawRevTick) {
                const float t = std::clamp(
                    (float)(simTick - g_lastChainsawRevTick) / decayTicks, 0.0f, 1.0f);
                rev = 1.0f + (maxRev - 1.0f) * (1.0f - t);
            }
            auto rtIt = player.weaponRuntimes.find(equipped->id);
            if (rtIt != player.weaponRuntimes.end())
                rtIt->second.customFloats["chainsawRevMultiplier"] = rev;
        }
    }

    const MeleeActorState state = player.ground.stableOnGround
        ? MeleeActorState::Grounded : MeleeActorState::Airborne;
    const MeleeModifier modifier = moveSet->animationOnly ? MeleeModifier::None : input.crouchHeld
        ? MeleeModifier::Crouch
        : (input.jumpHeld ? MeleeModifier::Jump : MeleeModifier::None);

    // A held shield suppresses attacks until released (Phase 5 owns the shield).
    const bool wantsAttack = !g_shieldHeld &&
        (input.primaryPressed || input.secondaryPressed || input.grabPressed);

    // Test/dev forced action (agent.melee): start the named action directly.
    if (!g_forceActionId.empty()) {
        const std::string forcedId = g_forceActionId;
        g_forceActionId.clear();
        if (const MeleeAction* forced = moveSet->find(forcedId)) {
            int index = 0;
            for (size_t i = 0; i < moveSet->actions.size(); ++i)
                if (moveSet->actions[i].id == forced->id) { index = (int)i; break; }
            const WeaponDefinition* def = weapons.getCurrentDef(player);
            if (def)
                startAction(player, ctx, *def, *forced, index, camera.front, simTick,
                            false, forcedId, "agent");
        }
    }

    if (wantsAttack) {
        MeleeButton button = MeleeButton::Primary;
        bool isPrimary = false;
        if (input.secondaryPressed)
            button = MeleeButton::Secondary;
        else if (input.grabPressed)
            button = MeleeButton::Grab;
        else
            isPrimary = true;

        const MeleeAction* action = resolveAction(*moveSet, button, state, modifier);
        std::string baseId = action ? action->id : std::string();

        // Both buttons use this shared fixed-tick double-click resolver.
        const int64_t lastPress = isPrimary ? g_state.lastPrimarySimTick : g_state.lastSecondarySimTick;
        const std::string& previousBase = isPrimary ? g_state.basePrimaryId : g_state.baseSecondaryId;
        if (action && !action->doubleClickAction.empty() && lastPress >= 0 &&
            (int64_t)simTick - lastPress <= action->doubleClickWindowTicks &&
            previousBase == baseId) {
            if (const MeleeAction* dbl = moveSet->find(action->doubleClickAction))
                action = dbl;
        }

        if (action) {
            int index = 0;
            for (size_t i = 0; i < moveSet->actions.size(); ++i)
                if (moveSet->actions[i].id == action->id) { index = (int)i; break; }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "melee.client.input", action->id, "melee input resolved", simTick,
                {{"button", (int)button}, {"modifier", (int)modifier},
                 {"grounded", player.ground.stableOnGround},
                 {"on_ground", player.ground.onGround},
                 {"stable_on_ground", player.ground.stableOnGround},
                 {"action", action->id},
                 {"tap_count", action->tapCount}, {"secondary_held", input.secondaryHeld},
                 {"animation_only", moveSet->animationOnly},
                 {"jump_held", input.jumpHeld}, {"crouch_held", input.crouchHeld},
                 {"shield_held", g_shieldHeld}});
            const WeaponDefinition* def = weapons.getCurrentDef(player);
            if (def)
                startAction(player, ctx, *def, *action, index, camera.front, simTick,
                            isPrimary, baseId, "input");
        }
    }

    // Held shield: cancel any attack, hold the shield pose (Phase 5 server owns
    // the durability/decay/break; the client only predicts the pose).
    if (g_shieldHeld) {
        if (g_state.active)
            endAction(player);
        if (const MeleeAction* shieldAction = moveSet->find("shield")) {
            if (!shieldAction->animationClipId.empty()) {
                // A held shield stays on the clip's authored active pose for as
                // long as Q is held; its timeline still owns the boundary.
                player.playOneShotAtTick(shieldAction->animationClipId,
                                         shieldAction->windup.ticks);
            } else {
                const std::string& clip = !shieldAction->windup.clipId.empty()
                    ? shieldAction->windup.clipId : shieldAction->active.clipId;
                if (!clip.empty())
                    player.playOneShotPhase(clip, 0, 1);
            }
        }
        return;
    }

    if (!g_state.active)
        return;

    // Use the snapshot taken at start so a mid-action hot reload does not change
    // this action's phases.
    const MeleeAction* action = &g_state.actionSnapshot;
    if (action->holdWhileButton && action->button == MeleeButton::Secondary &&
        !input.secondaryHeld && g_forceActionId.empty()) {
        StructuredLogger::instance().writeEvent(StructuredCategory::Weapons, StructuredLevel::Important,
            "weapon.action.release", action->id, "held weapon pose released", simTick,
            {{"weapon", moveSet->definitionId}, {"action", action->id}, {"blocking", false}});
        const WeaponDefinition* def = weapons.getCurrentDef(player);
        if (def && moveSet->animationOnly && ctx.active && ctx.gameplayActive)
            MimitaNet::mpSendAttackRequest(ctx, MimitaNet::weaponDefNetworkIdFor(def->id),
                (int16_t)actionRequestSlot(*def, ctx), player.pos, camera.front, player.pos,
                255, 0u, glm::vec3(0), 0, (uint16_t)(moveSet->revision & 0xffff));
        endAction(player);
        return;
    }

    // Advance the phase machine only on a new fixed tick.
    if (g_state.hasLastAdvance && simTick == g_state.lastAdvanceSimTick) {
        // Same tick: still sample the current pose so the first frame shows it.
    } else {
        g_state.hasLastAdvance = true;
        g_state.lastAdvanceSimTick = simTick;
    }

    const int windup = action->windup.ticks;
    const int active = action->active.ticks;
    const int recovery = action->recovery.ticks;
    const int total = action->totalTicks();
    const int elapsed = action->holdWhileButton
        ? std::min((int)(simTick - g_state.startSimTick), total - 1)
        : (int)(simTick - g_state.startSimTick);
    if (elapsed >= total) {
        endAction(player);
        return;
    }

    int phaseIndex = 0;          // 0 windup, 1 active, 2 recovery
    int phaseTick = elapsed;
    int phaseTicks = windup;
    std::string clip = action->animationClipId.empty()
        ? action->windup.clipId : action->animationClipId;
    const char* phaseName = "windup";
    if (elapsed >= windup + active) {
        phaseIndex = 2;
        phaseTick = elapsed - windup - active;
        phaseTicks = recovery;
        if (action->animationClipId.empty()) clip = action->recovery.clipId;
        phaseName = "recovery";
    } else if (elapsed >= windup) {
        phaseIndex = 1;
        phaseTick = elapsed - windup;
        phaseTicks = active;
        if (action->animationClipId.empty()) clip = action->active.clipId;
        phaseName = "active";
    }
    g_state.currentPhaseIndex = phaseIndex;
    g_state.currentPhaseTick = phaseTick;

    if (moveSet->definitionId == "swordsword" ||
        moveSet->definitionId == "chainsaw") {
        player.weaponPoseActionState = action->id;
    } else if (moveSet->definitionId == "mace") {
        player.weaponPoseActionState = action->id + "_" + phaseName;
    } else {
        player.weaponPoseActionState.clear();
    }

    if (!action->animationClipId.empty()) {
        player.playOneShotAtTick(action->animationClipId, elapsed);
        player.proceduralMeleePoseActive = false;
    } else if (!clip.empty()) {
        player.playOneShotPhase(clip, phaseTick, phaseTicks);
        player.proceduralMeleePoseActive = false;
    } else {
        WeaponData::MeleeProceduralPose pose;
        if (WeaponData::resolveMeleeProceduralPose(*action, phaseName,
                phaseTick, phaseTicks, pose)) {
            player.proceduralMeleeRightArmRotation = pose.rightArmRotation;
            player.proceduralMeleeRightArmTranslation = pose.rightArmTranslation;
            player.proceduralMeleeTorsoRotation = pose.torsoRotation;
            player.proceduralMeleePoseActive = true;
        } else {
            player.proceduralMeleePoseActive = false;
        }
    }

    // Log each phase transition with the limb pose so animation motion and the
    // attached hitbox path are verifiable in events.jsonl.
    {
        const std::string phaseKey =
            action->id + ":" + std::to_string(phaseIndex);
        if (phaseKey != g_lastPhaseKey) {
            g_lastPhaseKey = phaseKey;
            nlohmann::json phaseData = {
                {"action", action->id}, {"phase", phaseName},
                {"phase_index", phaseIndex}, {"phase_tick", phaseTick},
                {"phase_ticks", phaseTicks}, {"clip", clip}};
            if (!action->animationClipId.empty()) {
                const int sourceStart = action->animationSourceTicks[phaseIndex];
                const int sourceEnd = action->animationSourceTicks[phaseIndex + 1];
                phaseData["runtime_clip_start_tick"] = elapsed - phaseTick;
                phaseData["source_clip_start_tick"] = sourceStart;
                phaseData["source_clip_end_tick"] = sourceEnd;
                phaseData["source_clip_span_ticks"] = sourceEnd - sourceStart;
                phaseData["phase_speed_ratio"] = phaseTicks > 0
                    ? (float)(sourceEnd - sourceStart) / (float)phaseTicks : 0.0f;
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "melee.client.phase", action->id, "melee phase entered", simTick,
                phaseData);
            logMeleeLimbPose(player, *action, "melee.client.limb", phaseName, simTick);
        }
    }

    // Phase impulses: fire each authored event once, when its phase-local tick
    // is reached or passed.
    for (const WeaponData::MeleeImpulse& imp : action->impulses) {
        if (imp.phase != phaseName)
            continue;
        if (imp.tick > phaseTick)
            continue;
        // Only fire events reached since the previously processed phase tick.
        const bool samePhase = g_state.lastImpulsePhaseIndex == phaseIndex;
        if (samePhase && imp.tick <= g_state.lastImpulsePhaseTick)
            continue;
        const glm::vec3 impulse = impulseWorld(g_state.forward, imp);
        if (glm::length(impulse) > 0.001f)
            player.externalImpulse += impulse;
    }

    // Phase audio events: play once when the phase-local tick is reached.
    for (const WeaponData::MeleeAudioEvent& ev : action->audioEvents) {
        if (ev.phase != phaseName || ev.tick > phaseTick)
            continue;
        const bool samePhase = g_state.lastImpulsePhaseIndex == phaseIndex;
        if (samePhase && ev.tick <= g_state.lastImpulsePhaseTick)
            continue;
        playMeleeSound(ctx.localPlayerId, ev.soundId, ev.pitch, ev.volume,
                       player.pos, ev.offset);
    }

    g_state.lastImpulsePhaseIndex = phaseIndex;
    g_state.lastImpulsePhaseTick = phaseTick;
}

} // namespace MimitaMelee
