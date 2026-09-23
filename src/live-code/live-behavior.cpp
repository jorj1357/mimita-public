// 09 12 2026
/* purpose
* Implements the generic behavior event bridge and the kernel event queue used
* by the emitEvent capability.
* Does NOT own gameplay policy or state.
*/
#include "live-code/live-behavior.h"

#include <cstdio>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "debug/structured-log.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/generic-runtime.h"
#include "ecs/dynamic-components.h"
#include "ecs/relationship-store.h"
#include "live-code/live-modules.h"
#include "live-code/live-identity.h"
#include "live-code/live-ui.h"
#include "hot-reload/hot-pose.h"
#include "render/skeleton-instances.h"
#include "network/server-context.h"
#include "network/server-gamemode.h"
#include "network/server-hitscan-outcome.h"
#include "network/server-damage-outcome.h"
#include "network/server-damage-event.h"
#include "network/server-event-broadcast.h"
#include "network/server-history.h"
#include "hot-reload/hot-packet-codec.h"
#include "network/server-weapon-tuning.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-execution.h"
#include "ecs/entity-types.h"
#include "network/server.h"
#include "network/server.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "world/world.h"
#include "entities/player.h"
#include "camera.h"
#include "input/input-state.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "render/presentation-render.h"
#include "render/dynamic-light.h"
#include "renderer/renderer.h"
#include "config/player-settings.h"

extern Renderer* gRenderer;
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"
#include "network/multiplayer-context.h"

#include <windows.h>

#include <glm/gtc/quaternion.hpp>
#include "physics/ray-utils.h"
#include "ragdoll/ragdoll-components.h"
#include "ragdoll/ragdoll-entities.h"
#include "ragdoll/ragdoll-body.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "world/world.h"

namespace {

const ULONGLONG gProcessStartMs = GetTickCount64();

const GameGameplayModuleV1* gameplayModule()
{
    return static_cast<const GameGameplayModuleV1*>(
        LiveModules::findFunctions("gameplay", sizeof(GameGameplayModuleV1)));
}

constexpr int kMaxQueuedEvents = 64;
constexpr std::size_t kMaxPayloadBytes = 256;

struct QueuedEvent {
    GameEventV1 header{};
    unsigned char payload[kMaxPayloadBytes]{};
};

QueuedEvent gQueue[kMaxQueuedEvents];
int gHead = 0;
int gCount = 0;
bool gDraining = false;
const void* gDispatchWorld = nullptr;
// Server-side collision world (HeadlessWorld). When bound, the physics.move
// capability resolves against it so the same hot movement code runs on the
// dedicated/listen server as on the client.
const void* gDispatchHeadlessWorld = nullptr;
// Current-frame input + camera exposed to hot code through input.read/
// camera.read. Set once per tick by the kernel; never retained by hot code.
const void* gDispatchInput = nullptr;
const void* gDispatchCamera = nullptr;
std::uint64_t g_skeletonApplyCount = 0;
std::uint64_t g_animationUpdateCount = 0;
std::uint64_t g_audioPlayCount = 0;
std::uint64_t g_audioRejectCount = 0;
std::uint64_t g_surfaceEffectCount = 0;
std::uint64_t g_cameraEffectCount = 0;

void MIMITA_GAME_CALL kernelEmitEvent(GameplayContextV1*, const GameEventV1* event);

// ── Generic component capabilities ─────────────────────────
// One generic read/write path over stable component ids. No per-feature call
// site is needed to expose an existing component to hot behavior.
bool MIMITA_GAME_CALL capReadComponent(void*, std::uint64_t entity,
                                       std::uint32_t type, void* out,
                                       std::uint32_t outSize)
{
    if (!out || entity == 0)
        return false;
    EntityRegistry& registry = EntityRegistry::instance();
    const EntityId id = (EntityId)entity;
    switch (type) {
    case GAME_COMPONENT_TRANSFORM: {
        if (outSize < sizeof(GameTransformComponentV1)) return false;
        const auto* c = registry.tryGet<TransformComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameTransformComponentV1*>(out);
        o->position[0]=c->position.x; o->position[1]=c->position.y; o->position[2]=c->position.z;
        o->look[0]=c->look.x; o->look[1]=c->look.y; o->look[2]=c->look.z;
        o->yaw=c->yaw; o->pitch=c->pitch; return true; }
    case GAME_COMPONENT_VELOCITY: {
        if (outSize < sizeof(GameVelocityComponentV1)) return false;
        const auto* c = registry.tryGet<VelocityComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameVelocityComponentV1*>(out);
        o->linear[0]=c->linear.x; o->linear[1]=c->linear.y; o->linear[2]=c->linear.z;
        o->externalImpulse[0]=c->externalImpulse.x; o->externalImpulse[1]=c->externalImpulse.y; o->externalImpulse[2]=c->externalImpulse.z;
        return true; }
    case GAME_COMPONENT_HEALTH: {
        if (outSize < sizeof(GameHealthComponentV1)) return false;
        const auto* c = registry.tryGet<HealthComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameHealthComponentV1*>(out);
        o->current=c->current; o->max=c->max; o->dead=c->dead?1u:0u; return true; }
    case GAME_COMPONENT_MOVEMENT_INTENT: {
        if (outSize < sizeof(GameMovementIntentComponentV1)) return false;
        const auto* c = registry.tryGet<MovementIntentComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameMovementIntentComponentV1*>(out);
        o->moveX=c->moveX; o->moveY=c->moveY; o->pressed=c->pressed?1u:0u;
        o->jump=c->jump?1u:0u; o->dash=c->dash?1u:0u; o->downDash=c->downDash?1u:0u;
        o->freeze=c->freeze?1u:0u; return true; }
    case GAME_COMPONENT_MOVEMENT_RUNTIME_STATE: {
        if (outSize < sizeof(GameMovementRuntimeStateComponentV1)) return false;
        const auto* c = registry.tryGet<MovementRuntimeStateComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameMovementRuntimeStateComponentV1*>(out);
        o->version=c->version; o->grounded=c->grounded?1u:0u;
        o->jumpHeldPreviously=c->jumpHeldPreviously?1u:0u;
        o->jumpAirJumpArmed=c->airJumpArmed?1u:0u;
        o->airJumpsLeft=c->airJumpsLeft;
        o->dashHeldPreviously=c->dashHeldPreviously?1u:0u;
        o->downDashHeldPreviously=c->downDashHeldPreviously?1u:0u;
        o->dashAvailable=c->dashAvailable?1u:0u;
        o->downDashAvailable=c->downDashAvailable?1u:0u;
        o->dashMovementTicks=(std::uint32_t)(c->dashMovementTicks>0?c->dashMovementTicks:0);
        o->dashCooldownSeconds=c->dashCooldownSeconds;
        o->jumpIntentSeconds=c->jumpIntentSeconds;
        o->dashGraceSeconds=c->dashGraceSeconds;
        o->freezePreviously=c->freezePreviously?1u:0u;
        o->freezeActive=c->freezeActive?1u:0u;
        o->freezeAvailable=c->freezeAvailable?1u:0u;
        o->freezeTimerSeconds=c->freezeTimerSeconds;
        o->reserved[GAME_MOVEMENT_STAMP_TICK]=c->lastSimTick;
        o->reserved[GAME_MOVEMENT_STAMP_GENERATION]=c->lastSimGeneration;
        o->reserved[GAME_MOVEMENT_STAMP_FLAGS]=0u;
        return true; }
    case GAME_COMPONENT_CONTROL_SOURCE: {
        if (outSize < sizeof(GameControlSourceComponentV1)) return false;
        const auto* c = registry.tryGet<ControlSourceComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameControlSourceComponentV1*>(out);
        o->source = (std::uint32_t)c->source; o->reserved = 0u; return true; }
    case GAME_COMPONENT_AIM_INTENT: {
        if (outSize < sizeof(GameAimIntentComponentV1)) return false;
        const auto* c = registry.tryGet<AimIntentComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameAimIntentComponentV1*>(out);
        o->direction[0]=c->direction.x; o->direction[1]=c->direction.y; o->direction[2]=c->direction.z;
        o->yaw=c->yaw; o->pitch=c->pitch; return true; }
    case GAME_COMPONENT_FIRE_INTENT: {
        if (outSize < sizeof(GameFireIntentComponentV1)) return false;
        const auto* c = registry.tryGet<FireIntentComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameFireIntentComponentV1*>(out);
        o->weaponNetworkId=c->weaponNetworkId; o->trigger=c->trigger?1u:0u; return true; }
    case GAME_COMPONENT_PROJECTILE: {
        if (outSize < sizeof(GameProjectileComponentV1)) return false;
        const auto* c = registry.tryGet<ProjectileComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameProjectileComponentV1*>(out);
        o->weaponDefNetworkId=c->weaponDefNetworkId; o->fireSerial=c->fireSerial;
        o->spawnTime=c->spawnTime; o->lifetime=c->lifetime; return true; }
    case GAME_COMPONENT_COLLIDER: {
        if (outSize < sizeof(GameColliderComponentV1)) return false;
        const auto* c = registry.tryGet<ColliderComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameColliderComponentV1*>(out);
        o->radius=c->radius; o->height=c->height; return true; }
    case GAME_COMPONENT_BODY: {
        if (outSize < sizeof(GameBodyComponentV1)) return false;
        const auto* c = registry.tryGet<BodyComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameBodyComponentV1*>(out);
        o->sizeScale=c->sizeScale; o->radius=c->radius; o->height=c->height; return true; }
    case GAME_COMPONENT_RAGDOLL_LIMB: {
        if (outSize < sizeof(GameRagdollLimbComponentV1)) return false;
        const auto* c = registry.tryGet<Ragdoll::LimbComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameRagdollLimbComponentV1*>(out);
        o->limbIndex=c->limbIndex; o->parentIndex=c->parentIndex;
        o->position[0]=c->position.x; o->position[1]=c->position.y; o->position[2]=c->position.z;
        o->orientation[0]=c->orientation.w; o->orientation[1]=c->orientation.x; o->orientation[2]=c->orientation.y; o->orientation[3]=c->orientation.z;
        o->linearVelocity[0]=c->linearVelocity.x; o->linearVelocity[1]=c->linearVelocity.y; o->linearVelocity[2]=c->linearVelocity.z;
        o->angularVelocity[0]=c->angularVelocity.x; o->angularVelocity[1]=c->angularVelocity.y; o->angularVelocity[2]=c->angularVelocity.z;
        o->mass=c->mass; o->radius=c->radius; o->halfHeight=c->halfHeight; o->inverseMass=c->inverseMass; return true; }
    case GAME_COMPONENT_RAGDOLL_JOINT: {
        if (outSize < sizeof(GameRagdollJointComponentV1)) return false;
        const auto* c = registry.tryGet<Ragdoll::JointComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameRagdollJointComponentV1*>(out);
        o->limbIndex=c->limbIndex; o->parentLimb=c->parentLimb;
        o->parentLocalAnchor[0]=c->parentLocalAnchor.x; o->parentLocalAnchor[1]=c->parentLocalAnchor.y; o->parentLocalAnchor[2]=c->parentLocalAnchor.z;
        o->childLocalAnchor[0]=c->childLocalAnchor.x; o->childLocalAnchor[1]=c->childLocalAnchor.y; o->childLocalAnchor[2]=c->childLocalAnchor.z;
        o->restLength=c->restLength; o->maxStretch=c->maxStretch; o->stiffness=c->stiffness; o->damping=c->damping; o->positionBeta=c->positionBeta; return true; }
    case GAME_COMPONENT_RAGDOLL_ROOT: {
        if (outSize < sizeof(GameRagdollRootComponentV1)) return false;
        const auto* c = registry.tryGet<Ragdoll::RagdollRootComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameRagdollRootComponentV1*>(out);
        o->ownerActorId=c->ownerActorId; o->limbCount=c->limbCount; o->solverIterations=c->solverIterations;
        o->gravityScale=c->gravityScale; o->stiffness=c->stiffness; o->damping=c->damping;
        o->alive=c->alive?1u:0u; o->corpse=c->corpse?1u:0u; o->lastSolveTick=c->lastSolveTick; return true; }
    case GAME_COMPONENT_RAGDOLL_GRAB: {
        if (outSize < sizeof(GameRagdollGrabComponentV1)) return false;
        const auto* c = registry.tryGet<Ragdoll::GrabComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameRagdollGrabComponentV1*>(out);
        o->active=c->active?1u:0u; o->wasActive=c->wasActive?1u:0u; o->hand=c->hand; o->limbEntity=c->limbEntity;
        o->grabPoint[0]=c->grabPoint.x; o->grabPoint[1]=c->grabPoint.y; o->grabPoint[2]=c->grabPoint.z;
        o->grabNormal[0]=c->grabNormal.x; o->grabNormal[1]=c->grabNormal.y; o->grabNormal[2]=c->grabNormal.z;
        o->handPosition[0]=c->handPosition.x; o->handPosition[1]=c->handPosition.y; o->handPosition[2]=c->handPosition.z;
        o->handLocalAnchor[0]=c->handLocalAnchor.x; o->handLocalAnchor[1]=c->handLocalAnchor.y; o->handLocalAnchor[2]=c->handLocalAnchor.z;
        o->targetEntity=c->targetEntity;
        o->targetLocalAnchor[0]=c->targetLocalAnchor.x; o->targetLocalAnchor[1]=c->targetLocalAnchor.y; o->targetLocalAnchor[2]=c->targetLocalAnchor.z;
        o->grabbedActorId=c->grabbedActorId; o->strength=c->strength; o->constraintSerial=c->constraintSerial; return true; }
    case GAME_COMPONENT_BEHAVIOR_BINDINGS: {
        if (outSize < sizeof(GameBehaviorBindingsComponentV1)) return false;
        const auto* c = registry.tryGet<BehaviorBindingsComponent>(id);
        if (!c) return false;
        auto* o = static_cast<GameBehaviorBindingsComponentV1*>(out);
        *o = GameBehaviorBindingsComponentV1{};
        o->count = (std::uint32_t)glm::clamp(c->count, 0, (int)GAME_MAX_BEHAVIOR_BINDINGS);
        for (std::uint32_t i = 0; i < o->count; ++i) {
            o->bindings[i].eventType = c->bindings[i].eventType;
            o->bindings[i].behaviorId = c->bindings[i].behaviorId;
            o->bindings[i].codeHash = c->bindings[i].codeHash;
            o->bindings[i].generation = c->bindings[i].generation;
        }
        return true; }
    default:
        return false;
    }
}

bool MIMITA_GAME_CALL capWriteComponent(void*, std::uint64_t entity,
                                        std::uint32_t type, const void* in,
                                        std::uint32_t inSize)
{
    if (!in || entity == 0)
        return false;
    EntityRegistry& registry = EntityRegistry::instance();
    const EntityId id = (EntityId)entity;
    if (!registry.alive(id))
        return false;
    switch (type) {
    case GAME_COMPONENT_TRANSFORM: {
        if (inSize < sizeof(GameTransformComponentV1)) return false;
        const auto* i = static_cast<const GameTransformComponentV1*>(in);
        auto& c = registry.add<TransformComponent>(id);
        c.position = glm::vec3(i->position[0], i->position[1], i->position[2]);
        c.look = glm::vec3(i->look[0], i->look[1], i->look[2]);
        c.yaw = i->yaw; c.pitch = i->pitch; return true; }
    case GAME_COMPONENT_VELOCITY: {
        if (inSize < sizeof(GameVelocityComponentV1)) return false;
        const auto* i = static_cast<const GameVelocityComponentV1*>(in);
        auto& c = registry.add<VelocityComponent>(id);
        c.linear = glm::vec3(i->linear[0], i->linear[1], i->linear[2]);
        c.externalImpulse = glm::vec3(i->externalImpulse[0], i->externalImpulse[1], i->externalImpulse[2]);
        return true; }
    case GAME_COMPONENT_HEALTH: {
        if (inSize < sizeof(GameHealthComponentV1)) return false;
        const auto* i = static_cast<const GameHealthComponentV1*>(in);
        auto& c = registry.add<HealthComponent>(id);
        c.current=i->current; c.max=i->max; c.dead=i->dead!=0; return true; }
    case GAME_COMPONENT_MOVEMENT_INTENT: {
        if (inSize < sizeof(GameMovementIntentComponentV1)) return false;
        const auto* i = static_cast<const GameMovementIntentComponentV1*>(in);
        auto& c = registry.add<MovementIntentComponent>(id);
        c.moveX=i->moveX; c.moveY=i->moveY; c.pressed=i->pressed!=0;
        c.jump=i->jump!=0; c.dash=i->dash!=0; c.downDash=i->downDash!=0; c.freeze=i->freeze!=0; return true; }
    case GAME_COMPONENT_MOVEMENT_RUNTIME_STATE: {
        if (inSize < sizeof(GameMovementRuntimeStateComponentV1)) return false;
        const auto* i = static_cast<const GameMovementRuntimeStateComponentV1*>(in);
        auto& c = registry.add<MovementRuntimeStateComponent>(id);
        c.version=i->version; c.grounded=i->grounded!=0;
        c.jumpHeldPreviously=i->jumpHeldPreviously!=0;
        c.airJumpArmed=i->jumpAirJumpArmed!=0; c.airJumpsLeft=i->airJumpsLeft;
        c.dashHeldPreviously=i->dashHeldPreviously!=0;
        c.downDashHeldPreviously=i->downDashHeldPreviously!=0;
        c.dashAvailable=i->dashAvailable!=0; c.downDashAvailable=i->downDashAvailable!=0;
        c.dashMovementTicks=(int)i->dashMovementTicks;
        c.dashCooldownSeconds=i->dashCooldownSeconds;
        c.jumpIntentSeconds=i->jumpIntentSeconds;
        c.dashGraceSeconds=i->dashGraceSeconds;
        c.freezePreviously=i->freezePreviously!=0;
        c.freezeActive=i->freezeActive!=0;
        c.freezeAvailable=i->freezeAvailable!=0;
        c.freezeTimerSeconds=i->freezeTimerSeconds;
        c.lastSimTick=i->reserved[GAME_MOVEMENT_STAMP_TICK];
        c.lastSimGeneration=i->reserved[GAME_MOVEMENT_STAMP_GENERATION];
        return true; }
    case GAME_COMPONENT_CONTROL_SOURCE: {
        if (inSize < sizeof(GameControlSourceComponentV1)) return false;
        const auto* i = static_cast<const GameControlSourceComponentV1*>(in);
        auto& c = registry.add<ControlSourceComponent>(id);
        c.source = (ControlSource)i->source; return true; }
    case GAME_COMPONENT_AIM_INTENT: {
        if (inSize < sizeof(GameAimIntentComponentV1)) return false;
        const auto* i = static_cast<const GameAimIntentComponentV1*>(in);
        auto& c = registry.add<AimIntentComponent>(id);
        c.direction = glm::vec3(i->direction[0], i->direction[1], i->direction[2]);
        c.yaw=i->yaw; c.pitch=i->pitch; return true; }
    case GAME_COMPONENT_FIRE_INTENT: {
        if (inSize < sizeof(GameFireIntentComponentV1)) return false;
        const auto* i = static_cast<const GameFireIntentComponentV1*>(in);
        auto& c = registry.add<FireIntentComponent>(id);
        c.weaponNetworkId=i->weaponNetworkId; c.trigger=i->trigger!=0; return true; }
    case GAME_COMPONENT_PROJECTILE: {
        if (inSize < sizeof(GameProjectileComponentV1)) return false;
        const auto* i = static_cast<const GameProjectileComponentV1*>(in);
        auto& c = registry.add<ProjectileComponent>(id);
        c.weaponDefNetworkId=i->weaponDefNetworkId; c.fireSerial=i->fireSerial;
        c.spawnTime=i->spawnTime; c.lifetime=i->lifetime; return true; }
    case GAME_COMPONENT_COLLIDER: {
        if (inSize < sizeof(GameColliderComponentV1)) return false;
        const auto* i = static_cast<const GameColliderComponentV1*>(in);
        auto& c = registry.add<ColliderComponent>(id);
        c.radius=i->radius; c.height=i->height; return true; }
    case GAME_COMPONENT_BODY: {
        if (inSize < sizeof(GameBodyComponentV1)) return false;
        const auto* i = static_cast<const GameBodyComponentV1*>(in);
        auto& c = registry.add<BodyComponent>(id);
        c.sizeScale=i->sizeScale; c.radius=i->radius; c.height=i->height; return true; }
    case GAME_COMPONENT_RAGDOLL_LIMB: {
        if (inSize < sizeof(GameRagdollLimbComponentV1)) return false;
        const auto* i = static_cast<const GameRagdollLimbComponentV1*>(in);
        auto& c = registry.add<Ragdoll::LimbComponent>(id);
        c.limbIndex=i->limbIndex; c.parentIndex=i->parentIndex;
        c.position=glm::vec3(i->position[0], i->position[1], i->position[2]);
        c.orientation=glm::quat(i->orientation[0], i->orientation[1], i->orientation[2], i->orientation[3]);
        c.linearVelocity=glm::vec3(i->linearVelocity[0], i->linearVelocity[1], i->linearVelocity[2]);
        c.angularVelocity=glm::vec3(i->angularVelocity[0], i->angularVelocity[1], i->angularVelocity[2]);
        c.mass=i->mass; c.radius=i->radius; c.halfHeight=i->halfHeight; c.inverseMass=i->inverseMass;
        return true; }
    case GAME_COMPONENT_RAGDOLL_JOINT: {
        if (inSize < sizeof(GameRagdollJointComponentV1)) return false;
        const auto* i = static_cast<const GameRagdollJointComponentV1*>(in);
        auto& c = registry.add<Ragdoll::JointComponent>(id);
        c.limbIndex=i->limbIndex; c.parentLimb=i->parentLimb;
        c.parentLocalAnchor=glm::vec3(i->parentLocalAnchor[0], i->parentLocalAnchor[1], i->parentLocalAnchor[2]);
        c.childLocalAnchor=glm::vec3(i->childLocalAnchor[0], i->childLocalAnchor[1], i->childLocalAnchor[2]);
        c.restLength=i->restLength; c.maxStretch=i->maxStretch; c.stiffness=i->stiffness;
        c.damping=i->damping; c.positionBeta=i->positionBeta; return true; }
    case GAME_COMPONENT_RAGDOLL_ROOT: {
        if (inSize < sizeof(GameRagdollRootComponentV1)) return false;
        const auto* i = static_cast<const GameRagdollRootComponentV1*>(in);
        auto& c = registry.add<Ragdoll::RagdollRootComponent>(id);
        c.ownerActorId=i->ownerActorId; c.limbCount=i->limbCount; c.solverIterations=i->solverIterations;
        c.gravityScale=i->gravityScale; c.stiffness=i->stiffness; c.damping=i->damping;
        c.alive=i->alive!=0; c.corpse=i->corpse!=0; c.lastSolveTick=i->lastSolveTick; return true; }
    case GAME_COMPONENT_RAGDOLL_GRAB: {
        if (inSize < sizeof(GameRagdollGrabComponentV1)) return false;
        const auto* i = static_cast<const GameRagdollGrabComponentV1*>(in);
        auto& c = registry.add<Ragdoll::GrabComponent>(id);
        c.active=i->active!=0; c.wasActive=i->wasActive!=0; c.hand=i->hand; c.limbEntity=i->limbEntity;
        c.grabPoint=glm::vec3(i->grabPoint[0], i->grabPoint[1], i->grabPoint[2]);
        c.grabNormal=glm::vec3(i->grabNormal[0], i->grabNormal[1], i->grabNormal[2]);
        c.handPosition=glm::vec3(i->handPosition[0], i->handPosition[1], i->handPosition[2]);
        c.handLocalAnchor=glm::vec3(i->handLocalAnchor[0], i->handLocalAnchor[1], i->handLocalAnchor[2]);
        c.targetEntity=i->targetEntity;
        c.targetLocalAnchor=glm::vec3(i->targetLocalAnchor[0], i->targetLocalAnchor[1], i->targetLocalAnchor[2]);
        c.grabbedActorId=i->grabbedActorId; c.strength=i->strength; c.constraintSerial=i->constraintSerial;
        return true; }
    default:
        return false;
    }
}

std::uint32_t MIMITA_GAME_CALL capFindEntities(void*, std::uint32_t domain,
                                               std::uint32_t componentType,
                                               std::uint64_t* out,
                                               std::uint32_t maxOut)
{
    if (!out || maxOut == 0)
        return 0;
    EntityRegistry& registry = EntityRegistry::instance();
    std::uint32_t count = 0;
    for (EntityId id : registry.all()) {
        if (count >= maxOut)
            break;
        if (domain != 0) {
            const EntityIdentity* ident = registry.identity(id);
            if (!ident || (std::uint32_t)ident->domain != domain)
                continue;
        }
        // componentType 0 = any; otherwise filter by presence.
        if (componentType != 0) {
            switch (componentType) {
            case GAME_COMPONENT_TRANSFORM: if (!registry.has<TransformComponent>(id)) continue; break;
            case GAME_COMPONENT_VELOCITY: if (!registry.has<VelocityComponent>(id)) continue; break;
            case GAME_COMPONENT_HEALTH: if (!registry.has<HealthComponent>(id)) continue; break;
            case GAME_COMPONENT_MOVEMENT_INTENT: if (!registry.has<MovementIntentComponent>(id)) continue; break;
            case GAME_COMPONENT_MOVEMENT_RUNTIME_STATE: if (!registry.has<MovementRuntimeStateComponent>(id)) continue; break;
            case GAME_COMPONENT_CONTROL_SOURCE: if (!registry.has<ControlSourceComponent>(id)) continue; break;
            case GAME_COMPONENT_BODY: if (!registry.has<BodyComponent>(id)) continue; break;
            case GAME_COMPONENT_AIM_INTENT: if (!registry.has<AimIntentComponent>(id)) continue; break;
            case GAME_COMPONENT_RAGDOLL_LIMB: if (!registry.has<Ragdoll::LimbComponent>(id)) continue; break;
            case GAME_COMPONENT_RAGDOLL_ROOT: if (!registry.has<Ragdoll::RagdollRootComponent>(id)) continue; break;
            case GAME_COMPONENT_PROJECTILE: if (!registry.has<ProjectileComponent>(id)) continue; break;
            case GAME_COMPONENT_BEHAVIOR_BINDINGS: if (!registry.has<BehaviorBindingsComponent>(id)) continue; break;
            default: break;
            }
        }
        out[count++] = (std::uint64_t)id;
    }
    return count;
}

bool MIMITA_GAME_CALL capQueryWorldRay(void*, const float origin[3],
                                       const float dir[3], float maxDistance,
                                       float* outPoint, float* outNormal,
                                       float* outDistance)
{
    const World* world = static_cast<const World*>(gDispatchWorld);
    if (!world || !origin || !dir)
        return false;
    glm::vec3 o(origin[0], origin[1], origin[2]);
    glm::vec3 d(dir[0], dir[1], dir[2]);
    if (glm::length(d) < 1e-6f)
        return false;
    d = glm::normalize(d);
    const int tri = selectWorldTriangle(*world, o, d);
    if (tri < 0 || tri >= (int)world->collisionMesh.triangles.size())
        return false;
    const CollisionTriangle& t = world->collisionMesh.triangles[tri];
    // Reuse the editor's closest-point convention: plane distance along d.
    const float denom = glm::dot(t.normal, d);
    if (std::fabs(denom) < 1e-6f)
        return false;
    const float dist = glm::dot(t.normal, t.a - o) / denom;
    if (dist < 0.0f || dist > maxDistance)
        return false;
    const glm::vec3 p = o + d * dist;
    if (outPoint) { outPoint[0]=p.x; outPoint[1]=p.y; outPoint[2]=p.z; }
    if (outNormal) { outNormal[0]=t.normal.x; outNormal[1]=t.normal.y; outNormal[2]=t.normal.z; }
    if (outDistance) *outDistance = dist;
    return true;
}

// world.collision: paginated dump of the map collision triangles so hot code can
// build its own spatial index and own the collision/ragdoll algorithms.
void MIMITA_GAME_CALL capWorldCollision(void*, GameWorldCollisionPageV1* page)
{
    if (!page)
        return;
    page->count = 0;
    // Prefer the server's headless world when bound, else the client world. This
    // is the one seam that lets hot collision/ragdoll run on the dedicated
    // server with the same code as the client.
    std::uint32_t total = 0;
    const CollisionTriangle* data = nullptr;
    if (gDispatchHeadlessWorld) {
        const MimitaNet::HeadlessWorld* hw =
            static_cast<const MimitaNet::HeadlessWorld*>(gDispatchHeadlessWorld);
        total = static_cast<std::uint32_t>(hw->triangles.size());
        data = hw->triangles.empty() ? nullptr : hw->triangles.data();
    } else if (gDispatchWorld) {
        const World* world = static_cast<const World*>(gDispatchWorld);
        total = static_cast<std::uint32_t>(world->collisionMesh.triangles.size());
        data = world->collisionMesh.triangles.empty()
                   ? nullptr
                   : world->collisionMesh.triangles.data();
    }
    page->total = total;
    if (!page->out || !data)
        return;
    std::uint32_t n = 0;
    for (std::uint32_t i = page->offset;
         i < total && n < page->maxTriangles; ++i, ++n) {
        const CollisionTriangle& t = data[i];
        GameCollisionTriangleV1& o = page->out[n];
        o.a[0] = t.a.x; o.a[1] = t.a.y; o.a[2] = t.a.z;
        o.b[0] = t.b.x; o.b[1] = t.b.y; o.b[2] = t.b.z;
        o.c[0] = t.c.x; o.c[1] = t.c.y; o.c[2] = t.c.z;
        o.normal[0] = t.normal.x; o.normal[1] = t.normal.y; o.normal[2] = t.normal.z;
    }
    page->count = n;
}

// physics.impulse: add linear/angular velocity to an entity's body.
void MIMITA_GAME_CALL capPhysicsImpulse(void*, GamePhysicsImpulseV1* req)
{
    if (!req)
        return;
    req->applied = 0;
    const EntityId id = (EntityId)req->entity;
    EntityRegistry& registry = EntityRegistry::instance();
    if (id == kInvalidEntityId || !registry.alive(id))
        return;
    const VelocityComponent* v = registry.tryGet<VelocityComponent>(id);
    if (!v)
        return;
    VelocityComponent c = *v;
    c.linear += glm::vec3(req->linear[0], req->linear[1], req->linear[2]);
    registry.add<VelocityComponent>(id, c);
    req->applied = 1;
}

// input.read: copy the current input state into the POD hot code reads.
bool MIMITA_GAME_CALL capInputRead(void*, GameInputStateV1* out)
{
    if (!out || !gDispatchInput)
        return false;
    const InputState* in = static_cast<const InputState*>(gDispatchInput);
    GameInputStateV1 s{};
    s.wishMoveX = in->wishMoveXY.x;
    s.wishMoveY = in->wishMoveXY.y;
    s.camForward[0] = in->camForward.x;
    s.camForward[1] = in->camForward.y;
    s.camForward[2] = in->camForward.z;
    s.movementHeldDuration = in->movementHeldDuration;
    s.jumpHeld = in->jumpHeld ? 1u : 0u;
    s.jumpPressed = in->jumpPressed ? 1u : 0u;
    s.dashPressed = in->dashPressed ? 1u : 0u;
    s.movementPressed = in->movementPressed ? 1u : 0u;
    s.movementJustPressed = in->movementJustPressed ? 1u : 0u;
    s.groundReturnPressed = in->groundReturnPressed ? 1u : 0u;
    s.downDashPressed = in->downDashPressed ? 1u : 0u;
    s.freezeHeld = in->freezeHeld ? 1u : 0u;
    s.freezePressed = in->freezePressed ? 1u : 0u;
    s.ragdollTogglePressed = in->ragdollTogglePressed ? 1u : 0u;
    s.grabLeftHeld = in->grabLeftHeld ? 1u : 0u;
    s.grabRightHeld = in->grabRightHeld ? 1u : 0u;
    s.extendLeftMouse = in->extendLeftMouse ? 1u : 0u;
    s.extendRightMouse = in->extendRightMouse ? 1u : 0u;
    *out = s;
    return true;
}

// camera.read: copy the current camera transform into the POD hot code reads.
bool MIMITA_GAME_CALL capCameraRead(void*, GameCameraStateV1* out)
{
    if (!out || !gDispatchCamera)
        return false;
    const Camera* c = static_cast<const Camera*>(gDispatchCamera);
    GameCameraStateV1 s{};
    s.position[0] = c->pos.x; s.position[1] = c->pos.y; s.position[2] = c->pos.z;
    s.front[0] = c->front.x; s.front[1] = c->front.y; s.front[2] = c->front.z;
    s.up[0] = c->up.x; s.up[1] = c->up.y; s.up[2] = c->up.z;
    s.right[0] = c->right.x; s.right[1] = c->right.y; s.right[2] = c->right.z;
    s.yaw = c->yaw;
    s.pitch = c->pitch;
    s.fov = c->fov;
    s.valid = 1u;
    *out = s;
    return true;
}

// actor.skeleton.write: apply a hot-computed root + node local transforms to the
// typed local actor. The caller owns the math; the kernel owns the storage.
bool MIMITA_GAME_CALL capActorSkeletonWrite(void*, GameActorSkeletonWriteV1* w)
{
    if (!w)
        return false;
    w->applied = 0;
    Player* target = nullptr;
    if (w->ownerActorId != 0) {
        if (!gpMpContext)
            return false;
        auto& map = w->isNpc ? MP_CONTEXT.remoteNpcs : MP_CONTEXT.remotePlayers;
        auto it = map.find(w->ownerActorId);
        if (it == map.end())
            return false;
        target = &it->second;
    } else if (gpPlayer) {
        std::uint64_t local = 0;
        if (GameSharedStateV1* shared =
                MimitaRuntime::GenericRuntime::instance().sharedState())
            local = shared->localPlayerEntity;
        if (w->actorEntity != 0 && local != 0 && w->actorEntity != local)
            return false;
        target = gpPlayer;
    }
    if (!target)
        return false;
    Player& p = *target;
    if (p.perfectPoseSkeleton.nodes.empty())
        return false;

    p.pos = glm::vec3(w->rootPosition[0], w->rootPosition[1], w->rootPosition[2]);
    p.vel = glm::vec3(w->rootVelocity[0], w->rootVelocity[1], w->rootVelocity[2]);
    p.modelRootRotationActive = w->rootRotationActive != 0;
    p.modelRootRotation = glm::quat(w->rootRotation[0], w->rootRotation[1],
                                    w->rootRotation[2], w->rootRotation[3]);

    const int nodeCount = (int)p.perfectPoseSkeleton.nodes.size();
    for (std::uint32_t i = 0; i < w->ancestorCount && i < 8; ++i) {
        const int idx = w->ancestorNodes[i];
        if (idx >= 0 && idx < nodeCount)
            p.perfectPoseSkeleton.nodes[idx].localTransform = glm::mat4(1.0f);
    }
    for (std::uint32_t i = 0; i < w->nodeCount && i < GAME_MAX_SKELETON_NODES; ++i) {
        const int idx = w->nodes[i].nodeIndex;
        if (idx < 0 || idx >= nodeCount)
            continue;
        glm::mat4 m;
        for (int k = 0; k < 16; ++k)
            m[k / 4][k % 4] = w->nodes[i].local[k];
        p.perfectPoseSkeleton.nodes[idx].localTransform = m;
    }
    p.updateModelWorldTransforms();
    w->applied = 1;
    return true;
}

// ragdoll.snapshot: read or apply a per-owner ragdoll limb snapshot. Bridges the
// hot presenter/sender to the network snapshot codec without exposing ragdoll
// internals to hot code.
bool MIMITA_GAME_CALL capRagdollSnapshot(void*, GameRagdollSnapshotV1* req)
{
    if (!req)
        return false;
    Ragdoll::RagdollEntities& entities = Ragdoll::RagdollEntities::instance();
    if (req->op == 1u) {  // write (apply) from hot
        Ragdoll::Snapshot s{};
        s.ownerActorId = req->ownerActorId;
        s.limbCount = req->limbCount;
        s.tick = req->tick;
        const std::uint32_t n =
            req->limbCount < (std::uint32_t)Ragdoll::kMaxSnapshotLimbs
                ? req->limbCount
                : (std::uint32_t)Ragdoll::kMaxSnapshotLimbs;
        for (std::uint32_t i = 0; i < n; ++i) {
            s.limbs[i].limbIndex = req->limbs[i].limbIndex;
            for (int k = 0; k < 3; ++k) s.limbs[i].position[k] = req->limbs[i].position[k];
            for (int k = 0; k < 4; ++k) s.limbs[i].rotation[k] = req->limbs[i].rotation[k];
        }
        for (int g = 0; g < 2; ++g) {
            s.grabs[g].active = req->grabs[g].active;
            s.grabs[g].hand = req->grabs[g].hand;
            s.grabs[g].targetLimb = req->grabs[g].targetLimb;
            s.grabs[g].strength = req->grabs[g].strength;
            for (int k = 0; k < 3; ++k) {
                s.grabs[g].anchor[k] = req->grabs[g].anchor[k];
                s.grabs[g].handLocal[k] = req->grabs[g].handLocal[k];
            }
        }
        return entities.applySnapshot(s);
    }
    // read
    Ragdoll::Snapshot s{};
    if (!entities.writeSnapshot(req->ownerActorId, s))
        return false;
    req->limbCount = s.limbCount;
    req->tick = s.tick;
    const std::uint32_t n =
        s.limbCount < (std::uint32_t)Ragdoll::kMaxSnapshotLimbs
            ? s.limbCount
            : (std::uint32_t)Ragdoll::kMaxSnapshotLimbs;
    for (std::uint32_t i = 0; i < n; ++i) {
        req->limbs[i].limbIndex = s.limbs[i].limbIndex;
        for (int k = 0; k < 3; ++k) req->limbs[i].position[k] = s.limbs[i].position[k];
        for (int k = 0; k < 4; ++k) req->limbs[i].rotation[k] = s.limbs[i].rotation[k];
    }
    for (int g = 0; g < 2; ++g) {
        req->grabs[g].active = s.grabs[g].active;
        req->grabs[g].hand = s.grabs[g].hand;
        req->grabs[g].targetLimb = s.grabs[g].targetLimb;
        req->grabs[g].strength = s.grabs[g].strength;
        for (int k = 0; k < 3; ++k) {
            req->grabs[g].anchor[k] = s.grabs[g].anchor[k];
            req->grabs[g].handLocal[k] = s.grabs[g].handLocal[k];
        }
    }
    return true;
}

// ragdoll.bind: build the ragdoll body template for the local actor so hot code
// can map solved limb transforms onto the typed skeleton.
bool MIMITA_GAME_CALL capRagdollBind(void*, GameRagdollTemplateV1* out)
{
    if (!out)
        return false;
    out->valid = 0;
    if (!gpPlayer)
        return false;
    std::uint64_t local = 0;
    if (GameSharedStateV1* shared =
            MimitaRuntime::GenericRuntime::instance().sharedState())
        local = shared->localPlayerEntity;
    if (out->actorEntity != 0 && local != 0 && out->actorEntity != local)
        return false;

    RagdollBody body;
    Ragdoll::buildBody(THE_PLAYER, RagdollModeConfig::instance().data(), body);
    const std::uint32_t n = (std::uint32_t)std::min<std::size_t>(
        body.parts.size(), GAME_MAX_RAGDOLL_PARTS);
    out->partCount = n;
    out->torsoIndex = body.torsoIndex;
    out->headIndex = body.headIndex;
    out->leftArmIndex = body.leftArmIndex;
    out->rightArmIndex = body.rightArmIndex;
    out->leftLegIndex = body.leftLegIndex;
    out->rightLegIndex = body.rightLegIndex;
    for (int k = 0; k < 3; ++k)
        out->rootOffsetLocal[k] = body.rootOffsetLocal[k];
    const std::uint32_t ancestors = (std::uint32_t)std::min<std::size_t>(
        body.rootAncestorNodes.size(), 8);
    out->ancestorNodeCount = ancestors;
    for (std::uint32_t i = 0; i < ancestors; ++i)
        out->ancestorNodes[i] = body.rootAncestorNodes[i];
    for (std::uint32_t i = 0; i < n; ++i) {
        const RagdollModePart& p = body.parts[i];
        GameRagdollPartV1& o = out->parts[i];
        o.nameHash = gameHash(p.name.c_str());
        o.nodeIndex = p.nodeIndex;
        o.skeletonParentPart = p.skeletonParentPart;
        o.parentIndex = p.parentIndex;
        o.hasRotationLimits = p.hasRotationLimits ? 1u : 0u;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                o.meshLocal[c * 4 + r] = p.meshLocal[c][r];
        for (int k = 0; k < 3; ++k) {
            o.parentLocalAnchor[k] = p.parentLocalAnchor[k];
            o.childLocalAnchor[k] = p.childLocalAnchor[k];
            o.rotMinDeg[k] = p.rotMinDeg[k];
            o.rotMaxDeg[k] = p.rotMaxDeg[k];
        }
        o.restLength = p.restLength;
        o.maxStretch = p.maxStretch;
        o.bindRotation[0] = p.bindRelativeRotation.w;
        o.bindRotation[1] = p.bindRelativeRotation.x;
        o.bindRotation[2] = p.bindRelativeRotation.y;
        o.bindRotation[3] = p.bindRelativeRotation.z;
    }
    out->valid = n > 0 ? 1u : 0u;
    return out->valid != 0;
}

void MIMITA_GAME_CALL capLog(void*, const char* message)
{
    if (!message)
        return;
    std::printf("[HOT] %s\n", message);
    // One authoritative record: hot diagnostics land in events.jsonl.
    ::debug::Event ev;
    ev.category = "HOT";
    ev.name = "hot.log";
    ev.level = ::debug::Level::Debug;
    ev.message = message;
    ev.sourceFile = "live-behavior";
    ev.functionName = "capLog";
    ::debug::logEvent(ev);
}

// log.event: hot modules emit structured events through the ONE generic
// capability; the kernel writes them into the process run's events.jsonl.
void MIMITA_GAME_CALL capLogEvent(void*, const GameLogEventV1* event)
{
    if (!event)
        return;
    static const debug::Level kLevels[6] = {
        debug::Level::Trace, debug::Level::Debug, debug::Level::Info,
        debug::Level::Warn, debug::Level::Error, debug::Level::Fatal};
    const std::uint32_t levelIdx = event->level < 6u ? event->level : 2u;

    debug::Event ev;
    ev.category = event->category[0] ? event->category : "HOT";
    ev.name = event->name[0] ? event->name : "hot.event";
    ev.level = kLevels[levelIdx];
    ev.message = event->message;
    ev.reason = event->reason;
    ev.simulationTick = event->simulationTick;
    ev.frame = event->frame;
    ev.serverTick = event->serverTick;
    ev.clientTick = event->clientTick;
    ev.sourceFile = "hot-module";
    ev.functionName = "capLogEvent";
    if (event->entityId != 0)
        ev.fields["entity_id"] = event->entityId;
    if (event->actorId != 0)
        ev.fields["actor_id"] = event->actorId;
    if (event->actorKind != 0) {
        static const char* kKinds[] = {"none", "player", "npc", "remote", "other"};
        ev.fields["actor_type"] =
            kKinds[event->actorKind < 5u ? event->actorKind : 4u];
    }
    if (event->result[0])
        ev.fields["result"] = event->result;
    debug::logEvent(ev);
}

bool MIMITA_GAME_CALL capDynamicReadComponent(void*, std::uint64_t entity,
                                              std::uint64_t typeId, void* out,
                                              std::uint32_t outSize)
{
    return MimitaRuntime::DynamicComponentStore::instance().read(
        (EntityId)entity, typeId, out, outSize);
}

bool MIMITA_GAME_CALL capDynamicWriteComponent(void*, std::uint64_t entity,
                                               std::uint64_t typeId, const void* in,
                                               std::uint32_t inSize)
{
    return MimitaRuntime::DynamicComponentStore::instance().write(
        (EntityId)entity, typeId, in, inSize);
}

// ── Generic entity / dynamic-component lifecycle capabilities (ABI v6) ──
bool MIMITA_GAME_CALL capEntityCreate(void*, std::uint32_t realm,
                                      std::uint64_t* outEntity)
{
    if (!outEntity)
        return false;
    const std::uint32_t maxRealm = static_cast<std::uint32_t>(EntityRealm::Local);
    const EntityRealm r =
        realm <= maxRealm ? static_cast<EntityRealm>(realm) : EntityRealm::Server;
    const EntityId id = EntityRegistry::instance().createGeneric(r);
    *outEntity = static_cast<std::uint64_t>(id);
    return id != kInvalidEntityId;
}

bool MIMITA_GAME_CALL capEntityDestroy(void*, std::uint64_t entity)
{
    if (entity == 0)
        return false;
    EntityRegistry::instance().destroy(static_cast<EntityId>(entity));
    return true;
}

bool MIMITA_GAME_CALL capDynamicRemoveComponent(void*, std::uint64_t entity,
                                                std::uint64_t typeId)
{
    return MimitaRuntime::DynamicComponentStore::instance().remove(
        static_cast<EntityId>(entity), typeId);
}

std::uint32_t MIMITA_GAME_CALL capDynamicEnumerateComponent(
    void*, std::uint64_t typeId, std::uint64_t* out, std::uint32_t maxOut)
{
    if (!out)
        return 0;
    return static_cast<std::uint32_t>(
        MimitaRuntime::DynamicComponentStore::instance().enumerate(
            typeId, out, maxOut));
}

std::uint32_t MIMITA_GAME_CALL capDynamicComponentsOnEntity(
    void*, std::uint64_t entity, std::uint64_t* out, std::uint32_t maxOut)
{
    if (!out)
        return 0;
    return static_cast<std::uint32_t>(
        MimitaRuntime::DynamicComponentStore::instance().componentsOnEntity(
            static_cast<EntityId>(entity), out, maxOut));
}

bool MIMITA_GAME_CALL capDynamicComponentInfo(void*, std::uint64_t typeId,
                                              GameDynamicComponentInfoV1* out)
{
    if (!out)
        return false;
    const MimitaRuntime::DynamicComponentSchema* s =
        MimitaRuntime::DynamicComponentStore::instance().schema(typeId);
    if (!s)
        return false;
    *out = GameDynamicComponentInfoV1{};
    out->typeId = s->typeId;
    out->schemaHash = s->schemaHash;
    out->version = s->version;
    out->size = s->size;
    out->align = s->align;
    out->copyPolicy = s->copyPolicy;
    out->networkPolicy = s->networkPolicy;
    std::strncpy(out->name, s->name.c_str(), sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';
    return true;
}

bool MIMITA_GAME_CALL capRelationshipAdd(void*, std::uint64_t typeId,
                                         std::uint64_t from, std::uint64_t to,
                                         std::uint64_t value)
{
    return MimitaRuntime::RelationshipStore::instance().add(
        typeId, static_cast<EntityId>(from), static_cast<EntityId>(to), value);
}

bool MIMITA_GAME_CALL capRelationshipRemove(void*, std::uint64_t typeId,
                                            std::uint64_t from, std::uint64_t to)
{
    return MimitaRuntime::RelationshipStore::instance().remove(
        typeId, static_cast<EntityId>(from), static_cast<EntityId>(to));
}

std::uint32_t MIMITA_GAME_CALL capRelationshipQuery(void*, std::uint64_t typeId,
                                                    std::uint64_t from,
                                                    std::uint64_t* outTo,
                                                    std::uint64_t* outValue,
                                                    std::uint32_t maxOut)
{
    return static_cast<std::uint32_t>(
        MimitaRuntime::RelationshipStore::instance().query(
            typeId, static_cast<EntityId>(from), outTo, outValue, maxOut));
}

// ── Generic authoritative match capabilities ────────────────────────────
bool MIMITA_GAME_CALL capMatchCurrent(void*, std::uint64_t* outMatchEntity)
{
    if (!outMatchEntity)
        return false;
    *outMatchEntity = MimitaNet::serverMatchEntity();
    return true;
}

bool MIMITA_GAME_CALL capMatchActorTeamRead(void*, std::uint32_t actorId,
                                            std::int32_t* outTeam)
{
    if (!outTeam)
        return false;
    *outTeam = MimitaNet::serverMatchActorTeam(actorId);
    return true;
}

bool MIMITA_GAME_CALL capMatchFinish(void*, std::uint32_t winnerKind,
                                     std::uint32_t winnerId,
                                     std::uint32_t victoryType)
{
    return MimitaNet::serverMatchFinish(winnerKind, winnerId, victoryType);
}

bool MIMITA_GAME_CALL capMatchSetPhase(void*, std::uint32_t phase)
{
    return MimitaNet::serverMatchSetPhase(phase);
}

bool MIMITA_GAME_CALL capMatchRespawn(void*, std::uint64_t actorEntity)
{
    return MimitaNet::serverMatchRespawn(actorEntity);
}

bool MIMITA_GAME_CALL capMatchSetTeam(void*, std::uint32_t actorId,
                                      std::int32_t team)
{
    return MimitaNet::serverMatchSetTeam(actorId, team);
}

void MIMITA_GAME_CALL capRequestMovementOverride(void*, std::uint32_t flags,
                                                 const float position[3],
                                                 const float velocity[3], float yaw)
{
    MimitaRuntime::GenericRuntime::instance().requestMovementOverride(
        flags, position, velocity, yaw);
}

namespace {

// Headless-server capsule step: integrates the caller-supplied velocity (the
// caller owns gravity via gravityScale < 0), resolves against the HeadlessWorld
// collision, and writes the result back. This is the server counterpart of
// Physics::moveCapsuleStep so hot movement uses one primitive on both sides.
void moveCapsuleStepHeadless(MovementStateV1* s, const MimitaNet::HeadlessWorld* world, float dt)
{
    if (!s || dt <= 0.0f)
        return;
    const float gravityScale = s->gravityScale < 0.0f
        ? 0.0f
        : (s->gravityScale > 0.0f ? s->gravityScale : 1.0f);
    const float radius = s->radius > 0.0f ? s->radius : 0.4f;
    const float halfHeight = s->halfHeight > 0.0f ? s->halfHeight : 0.9f;

    glm::vec3 pos(s->position[0], s->position[1], s->position[2]);
    glm::vec3 vel(s->velocity[0], s->velocity[1], s->velocity[2]);

    vel.z -= 9.81f * gravityScale * dt;

    bool onGround = false;
    bool collided = false;
    const float inPos[3] = {pos.x, pos.y, pos.z};
    const float inVel[3] = {vel.x, vel.y, vel.z};
    float outPos[3] = {pos.x, pos.y, pos.z};
    float outVel[3] = {vel.x, vel.y, vel.z};
    if (LiveBehavior::capsuleMove(inPos, inVel, radius, halfHeight, s->yaw,
                                  s->sizeScale, dt, outPos, outVel, onGround,
                                  collided)) {
        pos = glm::vec3(outPos[0], outPos[1], outPos[2]);
        vel = glm::vec3(outVel[0], outVel[1], outVel[2]);
    } else {
        // Capability absent: integrate deterministically without collision.
        pos += vel * dt;
    }

    s->position[0] = pos.x; s->position[1] = pos.y; s->position[2] = pos.z;
    s->velocity[0] = vel.x; s->velocity[1] = vel.y; s->velocity[2] = vel.z;
    s->grounded = onGround ? 1u : 0u;
    s->collided = collided ? 1u : 0u;
}

} // namespace

namespace {

// Optional hot capsule-solve provider. When a hot package registers
// `physics.capsuleSolve`, it owns the capsule collision solve; otherwise the
// kernel solve runs. This is the seam that makes the collision algorithm hot.
bool tryHotCapsuleSolve(MovementStateV1& s, float dt)
{
    auto* fn = reinterpret_cast<GameCapsuleSolveFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_PHYSICS_CAPSULE_SOLVE));
    if (!fn)
        return false;
    GameCapsuleSolveV1 q{};
    for (int i = 0; i < 3; ++i) {
        q.position[i] = s.position[i];
        q.velocity[i] = s.velocity[i];
    }
    q.yaw = s.yaw;
    q.radius = s.radius;
    q.halfHeight = s.halfHeight;
    q.sizeScale = s.sizeScale;
    q.gravityScale = s.gravityScale;
    q.dt = dt;
    q.collisionFn = &capWorldCollision;
    q.collisionHost = nullptr;
    fn(nullptr, &q);
    if (q.handled == 0)
        return false;
    for (int i = 0; i < 3; ++i) {
        s.position[i] = q.outPosition[i];
        s.velocity[i] = q.outVelocity[i];
    }
    s.grounded = q.grounded;
    s.collided = q.collided;
    return true;
}

} // namespace

namespace {

// physics.move: the kernel-provided generic capsule move. The caller owns
// velocity/gravity policy; this runs the collision pipeline and writes the
// resolved state back. GAME_PHYSICS_MOVE_HEADLESS resolves against the server's
// bound HeadlessWorld. The hot capsule solver (physics.capsuleSolve) takes
// precedence so the collision algorithm stays hot-reloadable.
void MIMITA_GAME_CALL capPhysicsMove(void* /*host*/, MovementStateV1* s, float dt,
                                     std::uint32_t flags)
{
    if (!s || dt <= 0.0f)
        return;
    if ((flags & GAME_PHYSICS_MOVE_HEADLESS) != 0u)
    {
        if (tryHotCapsuleSolve(*s, dt))
            return;
        if (gDispatchHeadlessWorld)
        {
            moveCapsuleStepHeadless(
                s, static_cast<const MimitaNet::HeadlessWorld*>(gDispatchHeadlessWorld),
                dt);
            return;
        }
    }
    // No bound world: advance deterministically without collision.
    for (int i = 0; i < 3; ++i)
        s->position[i] += s->velocity[i] * dt;
    s->collided = 0u;
}

} // namespace


// effect.spawn: ONE generic effect descriptor. Known movement kinds map to the
// existing pooled emitters; every other kind uses the generic pooled path, so
// blood/sparks/smoke/debris/muzzle in the future need no ABI change.
void MIMITA_GAME_CALL capEffectSpawn(void*, const GameEffectSpawnV1* d)
{
    if (!d)
        return;
    const glm::vec3 pos(d->position[0], d->position[1], d->position[2]);
    const float scale = d->scale > 0.0f ? d->scale : 1.0f;
    EffectPartSystem& fx = EffectPartSystem::instance();

    if (d->kind == gameHash("effect.footstep")) { fx.spawnFootstep(pos, scale); return; }
    if (d->kind == gameHash("effect.dash"))     { fx.spawnDash(pos, scale); return; }
    if (d->kind == gameHash("effect.downDash")) { fx.spawnDownDash(pos); return; }
    if (d->kind == gameHash("effect.freeze"))   { fx.spawnFreeze(pos, d->lifetime > 0.0f ? d->lifetime : 5.0f); return; }
    if (d->kind == gameHash("effect.freezeTrail")) { fx.spawnFreezeTrail(pos); return; }

    // Generic dynamic light: hot policy drives the EXISTING cold light manager
    // through this same descriptor. No new light subsystem, no per-weapon slot.
    if (d->kind == gameHash("light.dynamic")) {
        const glm::vec3 color(d->color[0], d->color[1], d->color[2]);
        DynamicLightManager::instance().spawn(
            pos, color,
            d->scale > 0.0f ? d->scale : 1.0f,
            d->endScale > 0.0f ? d->endScale : 5.0f,
            d->lifetime > 0.0f ? d->lifetime : 0.1f);
        return;
    }

    EffectPart e;
    e.position = pos;
    e.velocity = glm::vec3(d->direction[0], d->direction[1], d->direction[2]) * d->speed;
    e.color = glm::vec3(d->color[0], d->color[1], d->color[2]);
    e.alpha = d->color[3] > 0.0f ? d->color[3] : 1.0f;
    e.scale = scale;
    e.endScale = d->endScale > 0.0f ? d->endScale : scale;
    e.lifetime = d->lifetime > 0.0f ? d->lifetime : 0.5f;
    e.maxLifetime = e.lifetime;
    e.affectedByGravity = (d->flags & 1u) != 0;
    fx.spawn(e);
}

// effect.part: expose the EXISTING pooled EffectPart primitive to hot policy.
// The kernel keeps the pool/lifetime/renderer; hot owns the descriptor, so a hot
// recipe reproduces the cold hit/blood/impact look exactly (textured billboards,
// sticky/flat decals, beams, boxes, tick-defined lifetimes).
void MIMITA_GAME_CALL capEffectPart(void*, const GameEffectPartV1* d)
{
    if (!d)
        return;
    EffectPart e;
    e.position = glm::vec3(d->position[0], d->position[1], d->position[2]);
    e.velocity = glm::vec3(d->velocity[0], d->velocity[1], d->velocity[2]);
    e.color = glm::vec3(d->color[0], d->color[1], d->color[2]);
    e.normal = glm::vec3(d->normal[0], d->normal[1], d->normal[2]);
    e.rotation = glm::vec3(d->rotation[0], d->rotation[1], d->rotation[2]);
    e.endPosition = glm::vec3(d->endPosition[0], d->endPosition[1],
                              d->endPosition[2]);
    e.halfSize = glm::vec3(d->halfSize[0], d->halfSize[1], d->halfSize[2]);
    e.scale = d->scale;
    e.endScale = d->endScale > 0.0f ? d->endScale : d->scale;
    e.alpha = d->alpha > 0.0f ? d->alpha : 1.0f;
    e.gravity = d->gravity;
    e.drag = d->drag;
    e.thickness = d->thickness;
    e.endThickness = d->endThickness;
    e.maxLifetime = d->maxLifetime > 0.0f ? d->maxLifetime : 1.0f;
    e.affectedByGravity = d->affectedByGravity != 0;
    e.sticky = d->sticky != 0;
    e.flatDecal = d->flatDecal != 0;
    e.beam = d->beam != 0;
    e.box = d->box != 0;
    e.billboardText = d->billboardText != 0;
    e.meshResourceId = d->meshResourceId;
    e.textureResourceId = d->textureResourceId;
    e.scaleXYZ = glm::vec3(d->scaleXYZ[0] > 0.0f ? d->scaleXYZ[0] : 1.0f,
                          d->scaleXYZ[1] > 0.0f ? d->scaleXYZ[1] : 1.0f,
                          d->scaleXYZ[2] > 0.0f ? d->scaleXYZ[2] : 1.0f);
    if (d->replayType[0] != '\0')
        e.replayType = d->replayType;
    if (d->texturePath[0] != '\0')
        e.texturePath = d->texturePath;
    if (d->label[0] != '\0')
        e.label = d->label;
    EffectPartSystem::instance().spawn(e);
}

// effect.pool: hot policy reads/writes/ages/kills the existing pooled effect
// storage (surface decals, blood particles) and can claim aging so the kernel
// stops aging (one owner). Storage/draw stay in the kernel.
bool MIMITA_GAME_CALL capEffectPool(void*, GameEffectPoolV1* q)
{
    if (!q)
        return false;
    EffectPartSystem& fx = EffectPartSystem::instance();
    if (q->op == GAME_EFFECT_POOL_CLAIM) {
        fx.setEffectAgingClaimed(q->count != 0);
        return true;
    }
    if (q->kind == GAME_EFFECT_POOL_DECALS) {
        const std::uint32_t n = fx.decalPoolCount();
        if (q->op == GAME_EFFECT_POOL_COUNT) {
            q->count = n;
            return true;
        }
        if (q->op == GAME_EFFECT_POOL_KILL) {
            fx.decalPoolKill(q->index);
            return true;
        }
        if (q->op == GAME_EFFECT_POOL_GET) {
            SurfaceDecal d;
            if (!fx.decalPoolGet(q->index, d))
                return false;
            q->position[0] = d.position.x;
            q->position[1] = d.position.y;
            q->position[2] = d.position.z;
            q->normal[0] = d.normal.x;
            q->normal[1] = d.normal.y;
            q->normal[2] = d.normal.z;
            q->axis[0] = d.axis.x;
            q->axis[1] = d.axis.y;
            q->axis[2] = d.axis.z;
            q->color[0] = d.color.x;
            q->color[1] = d.color.y;
            q->color[2] = d.color.z;
            q->alpha = d.alpha;
            q->scale = d.radius;
            q->height = d.height;
            q->age = d.age;
            q->lifetime = d.lifetime;
            q->fadeTime = d.fadeTime;
            q->textureScale = d.textureScale;
            q->decalKind = static_cast<std::uint32_t>(d.kind);
            q->flags = d.generic ? 1u : 0u;
            std::snprintf(q->texturePath, sizeof(q->texturePath), "%s",
                          d.texturePath.c_str());
            q->alive = 1;
            return true;
        }
        if (q->op == GAME_EFFECT_POOL_SET) {
            SurfaceDecal d;
            fx.decalPoolGet(q->index, d);   // preserve colour-over-lifetime fields
            d.position = glm::vec3(q->position[0], q->position[1], q->position[2]);
            d.normal = glm::vec3(q->normal[0], q->normal[1], q->normal[2]);
            d.axis = glm::vec3(q->axis[0], q->axis[1], q->axis[2]);
            d.color = glm::vec3(q->color[0], q->color[1], q->color[2]);
            d.alpha = q->alpha;
            d.radius = q->scale;
            d.height = q->height;
            d.age = q->age;
            d.lifetime = q->lifetime;
            d.fadeTime = q->fadeTime;
            d.textureScale = (q->textureScale > 0.0f) ? q->textureScale : 1.0f;
            d.kind = static_cast<SurfaceDecalKind>(q->decalKind);
            d.generic = (q->flags & 1u) != 0;
            d.texturePath = q->texturePath;
            fx.decalPoolSet(q->index, d);
            return true;
        }
        return false;
    }
    if (q->kind == GAME_EFFECT_POOL_BLOOD) {
        const std::uint32_t n = fx.bloodPoolCount();
        if (q->op == GAME_EFFECT_POOL_COUNT) {
            q->count = n;
            return true;
        }
        if (q->op == GAME_EFFECT_POOL_KILL) {
            fx.bloodPoolKill(q->index);
            return true;
        }
        if (q->op == GAME_EFFECT_POOL_GET) {
            BloodParticle p;
            if (!fx.bloodPoolGet(q->index, p))
                return false;
            q->position[0] = p.position.x;
            q->position[1] = p.position.y;
            q->position[2] = p.position.z;
            q->velocity[0] = p.velocity.x;
            q->velocity[1] = p.velocity.y;
            q->velocity[2] = p.velocity.z;
            q->color[0] = p.color.x;
            q->color[1] = p.color.y;
            q->color[2] = p.color.z;
            q->age = p.age;
            q->lifetime = p.lifetime;
            q->alpha = p.alpha;
            q->rotation = p.rotation;
            q->stretch = p.stretch;
            q->scale = p.size;
            q->alive = 1;
            return true;
        }
        if (q->op == GAME_EFFECT_POOL_SET) {
            BloodParticle p;
            p.position = glm::vec3(q->position[0], q->position[1], q->position[2]);
            p.velocity = glm::vec3(q->velocity[0], q->velocity[1], q->velocity[2]);
            p.color = glm::vec3(q->color[0], q->color[1], q->color[2]);
            p.age = q->age;
            p.lifetime = q->lifetime;
            p.alpha = q->alpha;
            p.rotation = q->rotation;
            p.stretch = q->stretch;
            p.size = q->scale;
            fx.bloodPoolSet(q->index, p);
            return true;
        }
        return false;
    }
    return false;
}

// GamePosePartV1.rotationEuler is the hot boundary unit: radians. The generic
// skeleton mechanism (SkeletonInstances) already consumes radians, so the typed
// body mirror uses the same unit here to keep one canonical convention.
glm::mat4 poseOffsetMatrix(const GamePosePartV1& part)
{
    glm::mat4 m(1.0f);
    m = glm::translate(m, glm::vec3(part.translation[0], part.translation[1],
                                    part.translation[2]));
    m = glm::rotate(m, part.rotationEuler[0], glm::vec3(1, 0, 0));
    m = glm::rotate(m, part.rotationEuler[1], glm::vec3(0, 1, 0));
    m = glm::rotate(m, part.rotationEuler[2], glm::vec3(0, 0, 1));
    return m;
}

// skeleton.apply: generic pose application. The hot caller supplies per-part
// euler offsets by part-name hash; the kernel owns rest pose, hierarchy, node
// mapping, and world-transform update. Any future pose source can use this.
void MIMITA_GAME_CALL capSkeletonApply(void*, const GameSkeletonPoseV1* pose)
{
    if (!pose)
        return;
    ++g_skeletonApplyCount;
    // Generic pose publication: copy the POD pose onto the entity so any
    // presenter (and headless tests) can read it. Never retains hot pointers.
    if (pose->entity != 0) {
        HotPoseStateV1 state{};
        state.version = pose->flags;
        const std::uint32_t n =
            pose->count < HOT_POSE_MAX_PARTS ? pose->count : HOT_POSE_MAX_PARTS;
        state.count = n;
        for (std::uint32_t i = 0; i < n; ++i) {
            state.part[i] = pose->parts[i].part;
            state.translation[i][0] = pose->parts[i].translation[0];
            state.translation[i][1] = pose->parts[i].translation[1];
            state.translation[i][2] = pose->parts[i].translation[2];
            state.rotationEuler[i][0] = pose->parts[i].rotationEuler[0];
            state.rotationEuler[i][1] = pose->parts[i].rotationEuler[1];
            state.rotationEuler[i][2] = pose->parts[i].rotationEuler[2];
        }
        MimitaRuntime::DynamicComponentStore::instance().write(
            static_cast<EntityId>(pose->entity), HOT_POSE_STATE_COMPONENT, &state,
            sizeof(state));
        // Drive the real per-entity skeleton instance (cold mechanism, keyed by
        // EntityId; not by Player/Npc identity).
        SkeletonInstances::applyPose(static_cast<EntityId>(pose->entity), *pose);
    }
    if (!gpPlayer)
        return;
    Player& p = THE_PLAYER;
    // Ragdoll owns the body while active; the hot pose must not overwrite it.
    if (p.ragdollModeActive)
        return;
    if (p.perfectPoseSkeleton.nodes.empty() ||
        p.perfectPoseSkeleton.restLocalTransforms.size() !=
            p.perfectPoseSkeleton.nodes.size())
        return;
    for (std::uint32_t i = 0; i < pose->count && i < GAME_MAX_POSE_PARTS; ++i)
    {
        const GamePosePartV1& pp = pose->parts[i];
        if (pp.part == 0)
            continue;
        for (PhysicalBodyPart& bp : p.physicalBody.parts)
        {
            if (gameHash(bp.name.c_str()) != pp.part)
                continue;
            if (bp.nodeIndex < 0 ||
                bp.nodeIndex >= (int)p.perfectPoseSkeleton.nodes.size())
                break;
            bp.pose.translation =
                glm::vec3(pp.translation[0], pp.translation[1], pp.translation[2]);
            bp.pose.rotationEuler =
                glm::vec3(pp.rotationEuler[0], pp.rotationEuler[1], pp.rotationEuler[2]);
            const glm::mat4 restM =
                p.perfectPoseSkeleton.restLocalTransforms[bp.nodeIndex];
            p.perfectPoseSkeleton.nodes[bp.nodeIndex].localTransform =
                restM * poseOffsetMatrix(pp);
            break;
        }
    }
    p.updateModelWorldTransforms();
}

// skeleton.validate: generic required-part check against the actor's current
// skeleton. Used by model-generation swaps and candidate self-tests; the hot
// side decides which parts are required, the kernel performs the lookup.
bool MIMITA_GAME_CALL capSkeletonValidate(void*, GameSkeletonValidateV1* q)
{
    if (!q)
        return false;
    q->presentMask = 0;
    q->missingCount = 0;
    q->valid = 0;
    if (q->requiredCount == 0) {
        q->valid = 1;
        return true;
    }
    const EntityId entity = static_cast<EntityId>(q->entity);
    const SkeletonInstances::Instance* inst =
        entity != kInvalidEntityId ? SkeletonInstances::get(entity) : nullptr;

    std::uint64_t localEntity = 0;
    if (GameSharedStateV1* shared =
            MimitaRuntime::GenericRuntime::instance().sharedState())
        localEntity = shared->localPlayerEntity;
    const Player* player =
        (gpPlayer && entity != kInvalidEntityId &&
         static_cast<std::uint64_t>(entity) == localEntity)
            ? gpPlayer
            : nullptr;

    const std::uint32_t count = q->requiredCount < GAME_MAX_VALIDATE_PARTS
                                    ? q->requiredCount
                                    : GAME_MAX_VALIDATE_PARTS;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t part = q->requiredParts[i];
        if (part == 0)
            continue;
        bool found = inst && SkeletonInstances::findBone(inst, part) != nullptr;
        if (!found && player) {
            for (const TransformNode& node : player->perfectPoseSkeleton.nodes) {
                if (gameHash(node.name.c_str()) == part) {
                    found = true;
                    break;
                }
            }
        }
        if (found)
            q->presentMask |= (1u << i);
        else
            ++q->missingCount;
    }
    q->valid = q->missingCount == 0 ? 1u : 0u;
    return true;
}

// The single generic resolver. Package providers and kernel primitives live in
// one registry table; the kernel does not switch on any capability's name.
void* MIMITA_GAME_CALL capResolveCapability(void*, std::uint64_t id)
{
    return MimitaRuntime::GenericRuntime::instance().capability(id);
}

bool MIMITA_GAME_CALL capRuntimeInfo(void*, GameRuntimeInfoV1* out)
{
    if (!out)
        return false;
    *out = GameRuntimeInfoV1{};
    out->pid = (std::uint32_t)LiveIdentity::pid();
    out->sessionId = LiveIdentity::sessionId();
    out->clientTick = (std::uint32_t)LiveIdentity::simulationTick();
    out->uptimeMs = GetTickCount64() - gProcessStartMs;
    std::strncpy(out->process, LiveIdentity::process(), sizeof(out->process) - 1);
    std::strncpy(out->eventsPath, debug::eventsPath().c_str(), sizeof(out->eventsPath) - 1);
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    out->activeGeneration = status.activeGeneration;
    out->hotAbiVersion = MIMITA_GAME_API_VERSION;
    if (!status.activeHash.empty())
        out->activeHash = std::strtoull(status.activeHash.c_str(), nullptr, 16);
    char exe[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exe, MAX_PATH) != 0)
        std::strncpy(out->exePath, exe, sizeof(out->exePath) - 1);
    if (gpMpContext) {
        out->serverGeneration = gpMpContext->serverCodeGeneration;
        out->serverTick = gpMpContext->latestServerTick;
        out->serverPhase = gpMpContext->serverCodePhase;
        out->serverHash = gpMpContext->serverCodeHash;
        out->serverLogicalHash = gpMpContext->serverLogicalHash;
        out->serverPlatformHash = gpMpContext->serverPlatformHash;
        std::strncpy(out->roomCode, gpMpContext->currentRoomCode.empty()
            ? gpMpContext->roomCode.c_str() : gpMpContext->currentRoomCode.c_str(),
            sizeof(out->roomCode) - 1);
        std::strncpy(out->serverName, gpMpContext->serverName.c_str(),
                     sizeof(out->serverName) - 1);
    }
    if (!status.lastError.empty())
        std::strncpy(out->lastError, status.lastError.c_str(), sizeof(out->lastError) - 1);
    return true;
}

void MIMITA_GAME_CALL capTerminalOutput(void*, const char* line)
{
    if (line)
        Terminal::instance().addLog(line);
}

// Generic authoritative server-context primitives: spawn a package projectile
// and apply damage by entity id. The server containers stay kernel-owned.
bool MIMITA_GAME_CALL capProjectileSpawn(void*, const GameProjectileSpawnSpecV1* spec,
                                         std::uint64_t* outEntity)
{
    if (!spec)
        return false;
    return MimitaNet::serverSpawnGenericProjectile(*spec, outEntity);
}

bool MIMITA_GAME_CALL capDamageApply(void*, GameDamageApplyV1* request)
{
    if (!request)
        return false;
    return MimitaNet::serverApplyEntityDamage(*request);
}

// hitscan.resolve: run the shared cold consequence pipeline for a hot trace.
void MIMITA_GAME_CALL capHitscanResolve(void*, GameHitscanResolveV1* request)
{
    if (!request)
        return;
    MimitaNet::ServerContextV1* context = MimitaNet::activeServerContext();
    if (!context || !context->players || !context->npcs || !context->tick)
        return;
    auto& players = *static_cast<std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>*>(
        context->players);
    auto& npcs = *static_cast<std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>*>(
        context->npcs);

    const std::uint32_t attackerId =
        entityLegacyId(static_cast<EntityId>(request->attackerEntity));
    auto shooterIt = players.find(attackerId);
    if (shooterIt == players.end())
        return;

    const std::string* weaponId =
        MimitaNet::weaponIdForDefNetworkId(request->weaponDefNetworkId);
    const WeaponDefinition* def =
        weaponId ? WeaponRegistry::instance().get(*weaponId) : nullptr;
    if (!def)
        return;

    WeaponExecution::HitscanTraceResult trace{};
    trace.pelletCount = static_cast<int>(request->pelletCount);
    const std::uint32_t aggregateCount =
        std::min(request->aggregateCount, (std::uint32_t)GAME_MAX_HITSCAN_AGGREGATES);
    for (std::uint32_t i = 0; i < aggregateCount; ++i)
    {
        const GameHitscanAggregateV1& src = request->aggregates[i];
        WeaponExecution::HitscanDamageAggregate agg{};
        agg.targetPlayerId = entityLegacyId(static_cast<EntityId>(src.victimEntity));
        agg.targetSpawnGeneration = src.victimSpawnGeneration;
        agg.damage = src.damage;
        agg.pelletHits = static_cast<int>(src.pelletHits);
        agg.headshot = src.headshot != 0;
        agg.knockback = glm::vec3(src.knockback[0], src.knockback[1], src.knockback[2]);
        agg.hitPosition = glm::vec3(src.hitPosition[0], src.hitPosition[1], src.hitPosition[2]);
        agg.hitNormal = glm::vec3(src.hitNormal[0], src.hitNormal[1], src.hitNormal[2]);
        trace.aggregates.push_back(agg);
    }
    const std::uint32_t pelletWrite =
        std::min(request->pelletCount, (std::uint32_t)GAME_MAX_HITSCAN_PELLETS);
    for (std::uint32_t i = 0; i < pelletWrite; ++i)
    {
        const GameHitscanPelletV1& src = request->pellets[i];
        WeaponExecution::HitscanPelletHit& dst = trace.pellets[i];
        dst.hit = src.hit != 0;
        dst.targetPlayerId = entityLegacyId(static_cast<EntityId>(src.victimEntity));
        dst.hitPosition = glm::vec3(src.hitPosition[0], src.hitPosition[1], src.hitPosition[2]);
        dst.hitNormal = glm::vec3(src.hitNormal[0], src.hitNormal[1], src.hitNormal[2]);
        dst.headshot = src.headshot != 0;
    }

    std::uint64_t totalPacketsLocal = 0;
    std::uint64_t& totalPacketsOut =
        context->totalPacketsOut ? *context->totalPacketsOut : totalPacketsLocal;

    MimitaNet::serverResolveHitscanOutcome(
        static_cast<SOCKET>(context->sock), players, npcs, shooterIt->second, *def, trace,
        glm::vec3(request->origin[0], request->origin[1], request->origin[2]),
        glm::vec3(request->direction[0], request->direction[1], request->direction[2]),
        glm::vec3(request->worldHit[0], request->worldHit[1], request->worldHit[2]),
        glm::vec3(request->worldNormal[0], request->worldNormal[1], request->worldNormal[2]),
        request->maxRange, request->worldBlockDistance,
        request->requestId, request->clientSimulationTick, request->claimedTargetId,
        *context->tick, totalPacketsOut);
    request->resolved = 1;
}

// projectile.event: broadcast a hot-owned projectile lifecycle event.
void MIMITA_GAME_CALL capEventNextId(void*, GameReliableEventTicketV1* out)
{
    MimitaNet::serverEventNextId(out);
}

void MIMITA_GAME_CALL capEventBroadcast(void*, GameEventBroadcastV1* request)
{
    if (!request)
        return;
    MimitaNet::serverEventBroadcast(*request);
    request->resolved = 1;
}

// Test-only observation sink for net.packet-reply when no server is running.
static LiveBehavior::PacketReplySink gPacketReplySink = nullptr;

// net.packet-reply: send caller-built bytes back to one connection. Used by a
// hot net.packet handler to answer a received packet (e.g. a handshake).
void MIMITA_GAME_CALL capHotPacketReply(void*, std::uint32_t connectionId,
                                        const void* bytes, std::uint32_t size)
{
    if (gPacketReplySink)
    {
        gPacketReplySink(connectionId, bytes, size);
        return;
    }
    MimitaNet::serverPacketReply(connectionId, bytes, size);
}

// history.query: generic EXE-owned historical state for lag compensation. The
// kernel exposes raw samples; hot `history.select` owns the decision.
std::uint32_t MIMITA_GAME_CALL capHistoryQuery(void*, MimitaNet::GameHistoryQueryV1* q)
{
    return MimitaNet::serverHistoryQuery(nullptr, q);
}

bool MIMITA_GAME_CALL capDamagePolicy(void*, GameDamagePolicyV1* request)
{
    if (!request)
        return false;
    return MimitaNet::serverDamagePolicyQuery(*request);
}

void MIMITA_GAME_CALL capDamageEvent(void*, GameDamageEventV1* request)
{
    if (!request)
        return;
    MimitaNet::serverApplyDamageEvent(*request);
    request->applied = 1;
}
bool MIMITA_GAME_CALL capWeaponTuning(void*, std::uint32_t weaponDefNetworkId,
                                      GameWeaponTuningV1* out)
{
    return MimitaNet::serverWeaponTuning(weaponDefNetworkId, out);
}

// damage.resolve: kept as a thin compatibility primitive. It now uses the
// shared per-victim damage policy/event primitives; the primary orchestration
// lives in hot modules (damage.resolve is only for callers that cannot build
// packets themselves).
void MIMITA_GAME_CALL capDamageResolve(void*, GameDamageResolveV1* request)
{
    if (!request)
        return;
    MimitaNet::ServerContextV1* context = MimitaNet::activeServerContext();
    if (!context || !context->players || !context->npcs || !context->tick)
        return;
    auto& players = *static_cast<std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>*>(
        context->players);
    auto& npcs = *static_cast<std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>*>(
        context->npcs);

    const MimitaNet::ServerPlayer* attacker = nullptr;
    if (request->attackerEntity != 0)
    {
        const std::uint32_t attackerId =
            entityLegacyId(static_cast<EntityId>(request->attackerEntity));
        auto it = players.find(attackerId);
        if (it != players.end())
            attacker = &it->second;
    }

    const std::string* weaponId =
        MimitaNet::weaponIdForDefNetworkId(request->weaponDefNetworkId);
    const WeaponDefinition* def =
        weaponId ? WeaponRegistry::instance().get(*weaponId) : nullptr;

    MimitaNet::ServerOutcomeVictim victims[GAME_MAX_DAMAGE_VICTIMS];
    const std::uint32_t count =
        std::min(request->victimCount, (std::uint32_t)GAME_MAX_DAMAGE_VICTIMS);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const GameDamageVictimV1& src = request->victims[i];
        MimitaNet::ServerOutcomeVictim& dst = victims[i];
        dst.entity = src.victimEntity;
        dst.spawnGeneration = src.victimSpawnGeneration;
        dst.damage = src.damage;
        dst.knockback[0] = src.knockback[0];
        dst.knockback[1] = src.knockback[1];
        dst.knockback[2] = src.knockback[2];
        dst.hitPosition[0] = src.hitPosition[0];
        dst.hitPosition[1] = src.hitPosition[1];
        dst.hitPosition[2] = src.hitPosition[2];
        dst.hitNormal[0] = src.hitNormal[0];
        dst.hitNormal[1] = src.hitNormal[1];
        dst.hitNormal[2] = src.hitNormal[2];
    }

    std::uint64_t totalPacketsLocal = 0;
    std::uint64_t& totalPacketsOut =
        context->totalPacketsOut ? *context->totalPacketsOut : totalPacketsLocal;

    MimitaNet::serverResolveDamageOutcome(
        static_cast<SOCKET>(context->sock), players, npcs, attacker, def,
        request->sourceKind, request->causeSerial, request->projectileId,
        victims, count, static_cast<std::uint32_t>(*context->tick), totalPacketsOut);
    request->resolved = 1;
}

// Generic round-based match mechanism: a hot mode records one round winner.
bool MIMITA_GAME_CALL capMatchRoundResult(void*, GameMatchRoundResultV1* request)
{
    if (!request)
        return false;
    const bool ok = MimitaNet::serverMatchRecordRoundResult(request->winnerTeam,
                                                            request->reasonHash);
    request->handled = ok ? 1u : 0u;
    request->outMatchOver = MimitaNet::serverGamemodeState().matchOver ? 1u : 0u;
    return ok;
}

// Generic map anchors (kernel-owned projection of map metadata).
std::uint32_t MIMITA_GAME_CALL capMapAnchors(void*, GameMapAnchorV1* out,
                                             std::uint32_t maxOut)
{
    return MimitaNet::serverMapAnchors(out, maxOut);
}

// Generic authoritative actor spawn/reset.
bool MIMITA_GAME_CALL capActorSpawn(void*, GameActorSpawnV1* request)
{
    if (!request)
        return false;
    return MimitaNet::serverSpawnOrResetActor(*request);
}

// Generic presentation command: hot render systems describe geometry; the
// kernel owns the low-level debug draw. No entity/weapon/mode type switch.
void MIMITA_GAME_CALL capRenderDebug(void*, const GameRenderDebugCommandV1* cmd)
{
    if (!cmd)
        return;
    const Camera& camera = THE_CAMERA;
    const glm::vec4 color(cmd->color[0], cmd->color[1], cmd->color[2], cmd->color[3]);
    const glm::vec3 a(cmd->a[0], cmd->a[1], cmd->a[2]);
    const glm::vec3 b(cmd->b[0], cmd->b[1], cmd->b[2]);
    const glm::vec3 half(cmd->half[0], cmd->half[1], cmd->half[2]);
    switch (cmd->shape) {
    case GAME_RENDER_DEBUG_LINE:
        DebugVis::drawLine(camera, a, b, color);
        break;
    case GAME_RENDER_DEBUG_WIRE_BOX:
        DebugVis::drawWireBox(camera, a, half, color);
        break;
    case GAME_RENDER_DEBUG_WIRE_SPHERE:
        DebugVis::drawWireSphere(camera, a, cmd->radius, color);
        break;
    case GAME_RENDER_DEBUG_WORLD_LABEL:
        DebugVis::drawWorldLabel(a, cmd->text, color);
        break;
    case GAME_RENDER_DEBUG_HUD_TEXT:
        uiDrawText(cmd->text, a.x, a.y, cmd->radius > 0.0f ? cmd->radius : 0.3f,
                   color);
        break;
    default:
        break;
    }
}

// Generic mesh presentation: forward the logical-resource command to the cold
// presentation renderer (which resolves the generation-aware handle and draws).
void MIMITA_GAME_CALL capRenderMesh(void*, const GameRenderMeshCommandV1* command)
{
    if (!command)
        return;
    PresentationRender::submitMesh(*command);
}

// Generic world->screen projection: hot overlay/UI policy supplies a world
// position; the kernel projects it through the live camera. No overlay kind is
// known to the kernel.
bool MIMITA_GAME_CALL capWorldProject(void*, GameWorldProjectV1* p)
{
    if (!p)
        return false;
    p->visible = 0;
    p->screenX = p->screenY = 0.0f;
    p->depth = 0.0f;
    if (!gpCamera)
        return false;
    const Camera& cam = THE_CAMERA;
    const float w = p->viewportWidth > 0.0f
                        ? p->viewportWidth
                        : (gRenderer ? (float)gRenderer->width : 1920.0f);
    const float h = p->viewportHeight > 0.0f
                        ? p->viewportHeight
                        : (gRenderer ? (float)gRenderer->height : 1080.0f);
    const glm::vec4 clip =
        cam.getProj(w, h) * cam.getView() *
        glm::vec4(p->worldPosition[0], p->worldPosition[1], p->worldPosition[2],
                  1.0f);
    if (clip.w <= 0.001f)
        return false;
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    p->screenX = (ndc.x * 0.5f + 0.5f) * w;
    p->screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * h;
    p->depth = clip.w;
    p->visible = 1;
    return true;
}

// Generic named-attachment-point query. Composes the entity's canonical
// transform with its current generic skeleton pose (SkeletonInstances) and, when
// the drawn mesh tags that part, its mesh bind. Falls back to the entity
// transform + caller local offset for non-skeletal entities. Resolves the live
// resource generation each call and never stores a pointer.
bool MIMITA_GAME_CALL capSocketQuery(void*, GameSocketQueryV1* q)
{
    if (!q)
        return false;
    q->found = 0;
    q->usedFallback = 0;
    q->valid = 0;

    glm::mat4 base(1.0f);
    bool haveEntity = false;
    const EntityId id = static_cast<EntityId>(q->entity);
    if (id != kInvalidEntityId) {
        const TransformComponent* t =
            EntityRegistry::instance().tryGet<TransformComponent>(id);
        if (t) {
            haveEntity = true;
            base = glm::translate(glm::mat4(1.0f), t->position);
            // TransformComponent.yaw is stored in DEGREES (from player.yaw);
            // building the rotation matrix needs radians or an attached model
            // spins ~57x per camera degree.
            const float yawRad = glm::radians(t->yaw);
            const float cy = std::cos(yawRad), sy = std::sin(yawRad);
            glm::mat4 rz(1.0f);   // world is Z-up; yaw rotates about Z
            rz[0][0] = cy;  rz[0][1] = sy;
            rz[1][0] = -sy; rz[1][1] = cy;
            base *= rz;
        }
    }

    glm::mat4 socket = base;
    if (q->socket != 0 && id != kInvalidEntityId) {
        const SkeletonInstances::BonePose* bone =
            SkeletonInstances::findBone(SkeletonInstances::get(id), q->socket);
        if (bone) {
            socket = base * bone->world;
            q->found = 1;
        }
        float bind16[16];
        if (PresentationRender::meshPartBind(q->entity, q->socket, bind16)) {
            glm::mat4 bind(1.0f);
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    bind[c][r] = bind16[c * 4 + r];
            socket = socket * bind;
            q->found = 1;
        }
    }
    if (q->found == 0)
        q->usedFallback = 1;   // non-skeletal/unknown socket: transform + local

    glm::mat4 local(1.0f);
    local = glm::translate(local, glm::vec3(q->localPosition[0],
                                            q->localPosition[1],
                                            q->localPosition[2]));
    const float rr = q->localRotation[0]*q->localRotation[0] +
                     q->localRotation[1]*q->localRotation[1] +
                     q->localRotation[2]*q->localRotation[2] +
                     q->localRotation[3]*q->localRotation[3];
    if (rr > 1e-6f) {
        const glm::quat lr(q->localRotation[3], q->localRotation[0],
                           q->localRotation[1], q->localRotation[2]);
        local *= glm::mat4_cast(glm::normalize(lr));
    }
    const glm::vec3 ls(q->localScale[0] > 0.0f ? q->localScale[0] : 1.0f,
                       q->localScale[1] > 0.0f ? q->localScale[1] : 1.0f,
                       q->localScale[2] > 0.0f ? q->localScale[2] : 1.0f);
    local *= glm::scale(glm::mat4(1.0f), ls);
    if (!haveEntity && q->found == 0)
        return false;   // no entity and no socket: nothing to resolve, fail safe
    const glm::mat4 out = socket * local;

    q->position[0] = out[3][0];
    q->position[1] = out[3][1];
    q->position[2] = out[3][2];
    const glm::quat rq = glm::quat_cast(out);
    q->rotation[0] = rq.x; q->rotation[1] = rq.y;
    q->rotation[2] = rq.z; q->rotation[3] = rq.w;
    q->scale[0] = glm::length(glm::vec3(out[0]));
    q->scale[1] = glm::length(glm::vec3(out[1]));
    q->scale[2] = glm::length(glm::vec3(out[2]));
    q->valid = 1;
    return true;
}

// socket.raw: the attachment point in the ENTITY-LOCAL frame (skeleton bone pose
// + mesh bind), with NO entity transform/yaw. Hot policy composes the final
// transform itself so units/grip/mount are editable live.
bool MIMITA_GAME_CALL capSocketRaw(void*, GameSocketRawV1* q)
{
    if (!q)
        return false;
    q->found = 0;
    q->valid = 0;
    const EntityId id = static_cast<EntityId>(q->entity);
    if (id == kInvalidEntityId)
        return false;
    glm::mat4 socket(1.0f);
    bool found = false;
    if (q->socket != 0) {
        const SkeletonInstances::BonePose* bone =
            SkeletonInstances::findBone(SkeletonInstances::get(id), q->socket);
        if (bone) {
            socket = bone->world;
            found = true;
        }
        float bind16[16];
        if (PresentationRender::meshPartBind(q->entity, q->socket, bind16)) {
            glm::mat4 bind(1.0f);
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    bind[c][r] = bind16[c * 4 + r];
            socket = socket * bind;
            found = true;
        }
    }
    if (!found)
        return false;
    for (int k = 0; k < 3; ++k)
        q->position[k] = socket[3][k];
    glm::mat3 m3(socket);
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(m3[c]);
        if (len > 1e-6f)
            m3[c] /= len;
    }
    const glm::quat rq = glm::quat_cast(m3);
    q->rotation[0] = rq.x;
    q->rotation[1] = rq.y;
    q->rotation[2] = rq.z;
    q->rotation[3] = rq.w;
    q->found = 1;
    q->valid = 1;
    return true;
}

// mesh.bounds: model-local AABB for a logical mesh, for hot grip/mount policy.
bool MIMITA_GAME_CALL capMeshBounds(void*, GameMeshBoundsV1* q)
{
    if (!q)
        return false;
    q->valid = 0;
    if (!PresentationRender::meshBounds(q->meshResourceId, q->boundsMin,
                                        q->boundsMax))
        return false;
    q->valid = 1;
    return true;
}

bool MIMITA_GAME_CALL capMeshPartBounds(void*, GameMeshPartBoundsV1* q)
{
    if (!q) return false;
    q->valid = 0;
    if (!PresentationRender::meshPartBounds(q->entity, q->part,
                                            q->boundsMin, q->boundsMax))
        return false;
    q->valid = 1;
    return true;
}

// body.parts: the animated physical-body parts of the local typed Player, in
// WORLD space (the exact transforms the renderer draws), plus each part-local
// collider AABB and its previous world position. This restores the afad20a
// per-limb collision source (Player::physicalBody.parts) behind the hot
// boundary. The root and any model scale are already baked into the part world
// transform, so hot policy must NOT apply them again.
bool MIMITA_GAME_CALL capBodyParts(void*, GameBodyPartsV1* q)
{
    if (!q)
        return false;
    q->valid = 0;
    q->count = 0;
    if (!gpPlayer)
        return false;
    std::uint64_t local = 0;
    if (GameSharedStateV1* shared =
            MimitaRuntime::GenericRuntime::instance().sharedState())
        local = shared->localPlayerEntity;
    if (q->entity != 0 && local != 0 && q->entity != local)
        return false;
    Player& p = THE_PLAYER;
    if (p.physicalBody.parts.empty())
        return false;

    const std::uint32_t n = (std::uint32_t)std::min<std::size_t>(
        p.physicalBody.parts.size(), GAME_MAX_BODY_PARTS);
    for (std::uint32_t i = 0; i < n; ++i) {
        const PhysicalBodyPart& part = p.physicalBody.parts[i];
        GameBodyPartV1& out = q->parts[i];
        out.part = gameHash(part.name.c_str());
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                out.worldMatrix[c * 4 + r] = part.worldTransform[c][r];
                out.previousWorldMatrix[c * 4 + r] =
                    part.previousWorldTransform[c][r];
            }
        out.boundsMin[0] = part.collider.localMin.x;
        out.boundsMin[1] = part.collider.localMin.y;
        out.boundsMin[2] = part.collider.localMin.z;
        out.boundsMax[0] = part.collider.localMax.x;
        out.boundsMax[1] = part.collider.localMax.y;
        out.boundsMax[2] = part.collider.localMax.z;
        out.space = 1u;  // matrices above are valid world transforms
        out.reserved = 0u;
    }
    q->count = n;
    q->valid = n > 0u ? 1u : 0u;
    return q->valid != 0u;
}

// Generic setting access seam: hot UI reads/writes real engine settings by
// logical id. The kernel owns the mapping + validity constraints.
// Kernel-provided discrete option lists for option-type settings. Hot code owns
// labels/layout; the kernel owns which values are valid.
const char* const kGraphicsPresets[] = {"Low", "Medium", "High"};
const char* const kResolutions[] = {"1280x960", "1600x900", "1920x1080"};
constexpr std::uint32_t kGraphicsPresetCount = 3;
constexpr std::uint32_t kResolutionCount = 3;

int indexOfOption(const char* const* list, std::uint32_t count,
                  const std::string& value)
{
    for (std::uint32_t i = 0; i < count; ++i)
        if (value == list[i])
            return (int)i;
    return -1;
}

bool MIMITA_GAME_CALL capSettingGet(void*, GameSettingV1* s)
{
    if (!s)
        return false;
    s->ok = 0;
    PlayerSettings& ps = GetPlayerSettings();
    const std::uint64_t id = s->settingId;
    if (id == gameHash("video.graphicsPreset")) {
        int idx = indexOfOption(kGraphicsPresets, kGraphicsPresetCount,
                                ps.graphicsPreset);
        if (idx < 0) idx = 2;
        s->type = GAME_SETTING_OPTION;
        s->intValue = idx;
        s->optionCount = kGraphicsPresetCount;
        std::snprintf(s->optionLabel, sizeof(s->optionLabel), "%s",
                      kGraphicsPresets[idx]);
        s->ok = 1;
        return true;
    }
    if (id == gameHash("video.resolution")) {
        int idx = indexOfOption(kResolutions, kResolutionCount, ps.resolution);
        if (idx < 0) idx = 0;
        s->type = GAME_SETTING_OPTION;
        s->intValue = idx;
        s->optionCount = kResolutionCount;
        std::snprintf(s->optionLabel, sizeof(s->optionLabel), "%s",
                      kResolutions[idx]);
        s->ok = 1;
        return true;
    }
    if (id == gameHash("video.fov")) {
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.fov; s->ok = 1;
    } else if (id == gameHash("audio.master")) {
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.masterVolume; s->ok = 1;
    } else if (id == gameHash("audio.music")) {
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.musicVolume; s->ok = 1;
    } else if (id == gameHash("audio.sfx")) {
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.sfxVolume; s->ok = 1;
    } else if (id == gameHash("input.sensitivity")) {
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.sensitivity; s->ok = 1;
    } else if (id == gameHash("audio.muted")) {
        s->type = GAME_SETTING_BOOL; s->intValue = ps.musicMuted ? 1 : 0; s->ok = 1;
    }
    return s->ok != 0;
}

bool MIMITA_GAME_CALL capSettingSet(void*, GameSettingV1* s)
{
    if (!s)
        return false;
    s->ok = 0;
    PlayerSettings& ps = GetPlayerSettings();
    const std::uint64_t id = s->settingId;
    auto clampf = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    if (id == gameHash("video.graphicsPreset")) {
        int idx = s->intValue;
        idx = idx < 0 ? 0 : (idx >= (int)kGraphicsPresetCount
                                 ? (int)kGraphicsPresetCount - 1 : idx);
        ps.graphicsPreset = kGraphicsPresets[idx];
        s->type = GAME_SETTING_OPTION;
        s->intValue = idx;
        s->optionCount = kGraphicsPresetCount;
        std::snprintf(s->optionLabel, sizeof(s->optionLabel), "%s",
                      kGraphicsPresets[idx]);
        s->ok = 1;
        return true;
    }
    if (id == gameHash("video.resolution")) {
        int idx = s->intValue;
        idx = idx < 0 ? 0
                      : (idx >= (int)kResolutionCount ? (int)kResolutionCount - 1
                                                      : idx);
        ps.resolution = kResolutions[idx];
        s->type = GAME_SETTING_OPTION;
        s->intValue = idx;
        s->optionCount = kResolutionCount;
        std::snprintf(s->optionLabel, sizeof(s->optionLabel), "%s",
                      kResolutions[idx]);
        s->ok = 1;
        return true;
    }
    if (id == gameHash("video.fov")) {
        ps.fov = clampf(s->floatValue, 60.0f, 140.0f);
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.fov; s->ok = 1;
    } else if (id == gameHash("audio.master")) {
        ps.masterVolume = clampf(s->floatValue, 0.0f, 1.0f);
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.masterVolume; s->ok = 1;
    } else if (id == gameHash("audio.music")) {
        ps.musicVolume = clampf(s->floatValue, 0.0f, 1.0f);
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.musicVolume; s->ok = 1;
    } else if (id == gameHash("audio.sfx")) {
        ps.sfxVolume = clampf(s->floatValue, 0.0f, 1.0f);
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.sfxVolume; s->ok = 1;
    } else if (id == gameHash("input.sensitivity")) {
        ps.sensitivity = clampf(s->floatValue, 0.01f, 1.0f);
        s->type = GAME_SETTING_FLOAT; s->floatValue = ps.sensitivity; s->ok = 1;
    } else if (id == gameHash("audio.muted")) {
        ps.musicMuted = s->intValue != 0;
        s->type = GAME_SETTING_BOOL; s->intValue = ps.musicMuted ? 1 : 0; s->ok = 1;
    }
    return s->ok != 0;
}

// Generic resource registration: hot code registers an arbitrary logical mesh or
// texture id backed by a path; the kernel owns parse/validate/generation swap.
bool MIMITA_GAME_CALL capResourceRegister(void*, GameResourceRegisterV1* req)
{
    if (!req || req->logicalId == 0 || req->path[0] == '\0')
        return false;
    req->ok = 0;
    req->generation = 0;
    const bool ok = PresentationRender::registerLogicalResource(
        req->logicalId, req->kind, req->path, req->applyNow != 0,
        &req->generation);
    // `ok` means the registration was accepted; `generation == 0` (with
    // applyNow) is how a caller detects that no handle was produced (load
    // failure, last-good preserved). Keeping them separate preserves the
    // existing "registration accepted" contract.
    req->ok = ok ? 1u : 0u;
    return ok;
}

// Generic HUD/UI: hot ui.frame systems emit widgets; the kernel draws them.
void MIMITA_GAME_CALL capRenderUi(void*, const GameUiCommandV1* command)
{
    if (!command)
        return;
    LiveUi::submit(*command);
}

// Generic surface effect: hot policy describes a mark; the kernel owns
// projection/geometry/storage/draw and never interprets a feature kind.
void MIMITA_GAME_CALL capSurfaceEffect(void*, const GameSurfaceEffectV1* request)
{
    if (!request)
        return;
    ++g_surfaceEffectCount;
    SurfaceDecal d;
    d.position = glm::vec3(request->position[0], request->position[1],
                           request->position[2]);
    d.normal = glm::vec3(request->normal[0], request->normal[1], request->normal[2]);
    d.axis = glm::vec3(request->axis[0], request->axis[1], request->axis[2]);
    d.color = glm::vec3(request->color[0], request->color[1], request->color[2]);
    d.alpha = request->color[3] > 0.0f ? request->color[3] : 1.0f;
    d.baseAlpha = d.alpha;
    d.radius = request->radius > 0.0f ? request->radius : 0.05f;
    d.height = request->height > 0.0f ? request->height : d.radius;
    d.lifetime = request->lifetime > 0.0f ? request->lifetime : 30.0f;
    d.fadeTime = request->fadeTime > 0.0f ? request->fadeTime : 5.0f;
    switch (request->decalKind) {
    case 2: d.kind = SurfaceDecalKind::BulletHole; break;
    case 3: d.kind = SurfaceDecalKind::Crack; break;
    default: d.kind = SurfaceDecalKind::Blood; break;
    }
    if (request->texture[0] != '\0') {
        // Hot owns the decal texture (bullet holes, cracks, blood splats); the
        // kernel still owns projection/storage/draw. Falls back to the
        // kind-based JSON texture in the renderer only when untextured.
        d.texturePath = request->texture;
        d.textureScale = request->textureScale > 0.0f ? request->textureScale : 1.0f;
        d.generic = false;
    } else {
        d.generic = true;
    }
    EffectPartSystem::instance().spawnGenericSurfaceDecal(d);
}

// Generic camera effect: hot policy decides amplitude/falloff; the kernel applies
// a temporary camera perturbation. No feature branch.
void MIMITA_GAME_CALL capCameraEffect(void*, const GameCameraEffectV1* effect)
{
    if (!effect)
        return;
    ++g_cameraEffectCount;  // count the command even when no camera exists (tests)
    if (!gpCamera)
        return;
    float atten = 1.0f;
    if (effect->falloffDistance > 0.0f) {
        atten = 1.0f - effect->distance / effect->falloffDistance;
        atten = atten < 0.0f ? 0.0f : (atten > 1.0f ? 1.0f : atten);
    }
    THE_CAMERA.addPunch(effect->pitch * atten, effect->yaw * atten);
}

// Generic persistent audio slots: desired state keyed by (ownerEntity, slotId);
// the cold side owns the physical voice (idempotent SET, safe STOP, cleanup on
// entity death). No raw voice handle crosses the hot boundary.
struct AudioSlotVoice {
    std::uint64_t owner;
    std::uint64_t slot;
    unsigned int synth;
    std::string sound;
};
static std::vector<AudioSlotVoice> g_audioSlots;
static unsigned int g_nextSynthOwner = 0x40000000u;

// Generic audio: hot policy emits a logical sound command; the kernel plays it.
// The command is the stable ABI surface: this function validates the envelope,
// routes the op, and owns the (owner,slot) voice table. It performs no sound
// selection policy, so editing hot policy changes behavior without an EXE link.
void MIMITA_GAME_CALL capAudioPlay(void*, const GameAudioCommandV1* command)
{
    if (!command)
        return;

    // ABI/version gate. A legacy zero-initialized caller leaves
    // commandVersion/structSize at 0 and is treated as v1. Any other unknown
    // version, or a structSize that is not this build's, is rejected with no
    // side effects (invalid commands never crash or play).
    const bool legacy = command->commandVersion == 0 && command->structSize == 0;
    if (!legacy) {
        if (command->commandVersion != GAME_AUDIO_COMMAND_VERSION ||
            (command->structSize != 0 &&
             command->structSize != sizeof(GameAudioCommandV1))) {
            ++g_audioRejectCount;
            return;
        }
    }
    auto* out = const_cast<GameAudioCommandV1*>(command);

    // Generic status query: no sound required. Reports live voice/resource
    // counts so a hot `audio status` command needs no cold call site.
    if (command->op == GAME_AUDIO_QUERY_STATUS) {
        out->ok = 1u;
        out->activeVoices = AudioManager::instance().activeVoiceCount();
        out->loadedResources = AudioManager::instance().cachedSoundCount();
        return;
    }
    // Listener update: position + velocity only; no sound required.
    if (command->op == GAME_AUDIO_SET_LISTENER) {
        setAudioListener(glm::vec3(command->position[0], command->position[1],
                                   command->position[2]),
                         glm::vec3(command->velocity[0], command->velocity[1],
                                   command->velocity[2]));
        out->ok = 1u;
        return;
    }
    // Resource generation ops land with the resource-provider phase. Accept the
    // command shape, report "not applied", and never touch the device.
    if (command->op == GAME_AUDIO_RELOAD_RESOURCE ||
        command->op == GAME_AUDIO_INVALIDATE_RESOURCE) {
        out->ok = 0u;
        return;
    }
    // Every remaining op resolves a logical sound id.
    if (command->sound[0] == '\0') {
        ++g_audioRejectCount;
        return;
    }
    const std::string name(command->sound,
                           strnlen(command->sound, sizeof(command->sound)));

    if (command->slotId != 0) {
        EntityRegistry& reg = EntityRegistry::instance();
        // Entity-death cleanup: terminate slots whose owner no longer exists.
        for (auto it = g_audioSlots.begin(); it != g_audioSlots.end();) {
            if (it->owner != 0 &&
                !reg.alive(static_cast<EntityId>(it->owner))) {
                AudioManager::instance().stopOwner(it->synth);
                it = g_audioSlots.erase(it);
            } else {
                ++it;
            }
        }
        if (command->op == GAME_AUDIO_STOP_SLOT) {
            for (auto it = g_audioSlots.begin(); it != g_audioSlots.end(); ++it) {
                if (it->owner == command->ownerEntity &&
                    it->slot == command->slotId) {
                    AudioManager::instance().stopOwner(it->synth);
                    g_audioSlots.erase(it);
                    break;
                }
            }
            return;
        }
        // Pause/resume a persistent slot voice in place (no restart, cursor
        // retained). A missing slot is a safe no-op.
        if (command->op == GAME_AUDIO_PAUSE_SLOT ||
            command->op == GAME_AUDIO_RESUME_SLOT) {
            const bool paused = command->op == GAME_AUDIO_PAUSE_SLOT;
            for (AudioSlotVoice& v : g_audioSlots) {
                if (v.owner == command->ownerEntity &&
                    v.slot == command->slotId) {
                    AudioManager::instance().setOwnerPaused(v.synth, paused);
                    break;
                }
            }
            return;
        }
        // Legacy zero-op slot commands are SET_SLOT.
        if (command->op != GAME_AUDIO_SET_SLOT &&
            command->op != GAME_AUDIO_PLAY_ONESHOT) {
            ++g_audioRejectCount;
            return;
        }
        // SET_SLOT (idempotent): same desired sound => no-op; else replace.
        for (AudioSlotVoice& v : g_audioSlots) {
            if (v.owner != command->ownerEntity || v.slot != command->slotId)
                continue;
            if (v.sound == name)
                return;   // already playing this desired state
            AudioManager::instance().stopOwner(v.synth);
            v.sound = name;
            v.synth = ++g_nextSynthOwner;
            AudioEvent e;
            e.name = name;
            e.world = command->spatial != 0;
            e.position = glm::vec3(command->position[0], command->position[1],
                                   command->position[2]);
            e.volume = command->volume > 0.0f ? command->volume : 1.0f;
            e.pitch = command->pitch > 0.0f ? command->pitch : 1.0f;
            e.maxDistance = command->maxDistance > 0.0f ? command->maxDistance : 50.0f;
            e.ownerId = v.synth;
            e.loop = command->loop != 0;
            AudioManager::instance().play(e);
            ++g_audioPlayCount;   // replaced voice
            return;
        }
        const unsigned int synth = ++g_nextSynthOwner;
        AudioEvent e;
        e.name = name;
        e.world = command->spatial != 0;
        e.position = glm::vec3(command->position[0], command->position[1],
                               command->position[2]);
        e.volume = command->volume > 0.0f ? command->volume : 1.0f;
        e.pitch = command->pitch > 0.0f ? command->pitch : 1.0f;
        e.maxDistance = command->maxDistance > 0.0f ? command->maxDistance : 50.0f;
        e.ownerId = synth;
        e.loop = command->loop != 0;
        AudioManager::instance().play(e);
        g_audioSlots.push_back(
            {command->ownerEntity, command->slotId, synth, name});
        ++g_audioPlayCount;   // started voice
        return;
    }
    // One-shot playback. Any other op with no slot is an unknown command.
    if (command->op != GAME_AUDIO_PLAY_ONESHOT) {
        ++g_audioRejectCount;
        return;
    }
    ++g_audioPlayCount;

    if (command->spatial != 0) {
        playWorldSound(name, glm::vec3(command->position[0], command->position[1],
                                       command->position[2]),
                       command->volume > 0.0f ? command->volume : 1.0f,
                       command->pitch > 0.0f ? command->pitch : 1.0f,
                       command->maxDistance > 0.0f ? command->maxDistance : 50.0f);
    } else {
        playSoundPitched(name, command->volume > 0.0f ? command->volume : 1.0f,
                         command->pitch > 0.0f ? command->pitch : 1.0f);
    }
}



// Kernel primitives are registered as ordinary capability entries with a
// signature. The mechanism is identical to a hot package provider; the only
// difference is providerPackage == 0 (kernel).
struct KernelCapabilityInit {
    KernelCapabilityInit()
    {
        MimitaRuntime::GenericRuntime& rt = MimitaRuntime::GenericRuntime::instance();
        rt.registerKernelCapability(GAME_CAP_PROJECTILE_SPAWN,
                                    gameHash("sig.projectile.spawn.v1"), 0,
                                    reinterpret_cast<void*>(&capProjectileSpawn),
                                    "projectile.spawn");
        rt.registerKernelCapability(GAME_CAP_DAMAGE_APPLY,
                                    gameHash("sig.damage.apply.v1"), 0,
                                    reinterpret_cast<void*>(&capDamageApply),
                                    "damage.apply");
        rt.registerKernelCapability(GAME_CAP_HITSCAN_RESOLVE,
                                    gameHash("sig.hitscan.resolve.v1"), 0,
                                    reinterpret_cast<void*>(&capHitscanResolve),
                                    "hitscan.resolve");
        rt.registerKernelCapability(GAME_CAP_WEAPON_TUNING,
                                    gameHash("sig.weapon.tuning.v1"), 0,
                                    reinterpret_cast<void*>(&capWeaponTuning),
                                    "weapon.tuning");
        rt.registerKernelCapability(GAME_CAP_DAMAGE_RESOLVE,
                                    gameHash("sig.damage.resolve.v1"), 0,
                                    reinterpret_cast<void*>(&capDamageResolve),
                                    "damage.resolve");
        rt.registerKernelCapability(GAME_CAP_EVENT_NEXT_ID,
                                    gameHash("sig.event.next-id.v1"), 0,
                                    reinterpret_cast<void*>(&capEventNextId),
                                    "event.next-id");
        rt.registerKernelCapability(GAME_CAP_EVENT_BROADCAST,
                                    gameHash("sig.event.broadcast.v1"), 0,
                                    reinterpret_cast<void*>(&capEventBroadcast),
                                    "event.broadcast");
        rt.registerKernelCapability(GAME_CAP_DAMAGE_POLICY,
                                    gameHash("sig.damage.policy.v1"), 0,
                                    reinterpret_cast<void*>(&capDamagePolicy),
                                    "damage.policy");
        rt.registerKernelCapability(GAME_CAP_DAMAGE_EVENT,
                                    gameHash("sig.damage.event.v1"), 0,
                                    reinterpret_cast<void*>(&capDamageEvent),
                                    "damage.event");
        rt.registerKernelCapability(GAME_CAP_MATCH_ROUND_RESULT,
                                    gameHash("sig.match.round-result.v1"), 0,
                                    reinterpret_cast<void*>(&capMatchRoundResult),
                                    "match.round-result");
        rt.registerKernelCapability(GAME_CAP_MAP_ANCHORS,
                                    gameHash("sig.map.anchors.v1"), 0,
                                    reinterpret_cast<void*>(&capMapAnchors),
                                    "map.anchors");
        rt.registerKernelCapability(GAME_CAP_ACTOR_SPAWN,
                                    gameHash("sig.actor.spawn.v1"), 0,
                                    reinterpret_cast<void*>(&capActorSpawn),
                                    "actor.spawn");
        rt.registerKernelCapability(GAME_CAP_LOG_EVENT,
                                    gameHash("sig.log.event.v1"), 0,
                                    reinterpret_cast<void*>(&capLogEvent),
                                    "log.event");
        rt.registerKernelCapability(MimitaNet::GAME_CAP_HOT_PACKET_REPLY,
                                    gameHash("sig.net.packet-reply.v1"), 0,
                                    reinterpret_cast<void*>(&capHotPacketReply),
                                    "net.packet-reply");
        rt.registerKernelCapability(MimitaNet::GAME_CAP_HISTORY_QUERY,
                                    gameHash("sig.history.query.v1"), 0,
                                    reinterpret_cast<void*>(&capHistoryQuery),
                                    "history.query");
        rt.registerKernelCapability(GAME_CAP_RUNTIME_INFO,
                                    gameHash("sig.runtime.info.v1"), 0,
                                    reinterpret_cast<void*>(&capRuntimeInfo),
                                    "runtime.info");
        rt.registerKernelCapability(GAME_CAP_TERMINAL_OUTPUT,
                                    gameHash("sig.terminal.output.v1"), 0,
                                    reinterpret_cast<void*>(&capTerminalOutput),
                                    "terminal.output");
        rt.registerKernelCapability(GAME_CAP_RENDER_DEBUG,
                                    gameHash("sig.render.debug.v1"), 0,
                                    reinterpret_cast<void*>(&capRenderDebug),
                                    "render.debug");
        rt.registerKernelCapability(GAME_CAP_RENDER_MESH,
                                    gameHash("sig.render.mesh.v1"), 0,
                                    reinterpret_cast<void*>(&capRenderMesh),
                                    "render.mesh");
        rt.registerKernelCapability(GAME_CAP_RENDER_UI,
                                    gameHash("sig.render.ui.v1"), 0,
                                    reinterpret_cast<void*>(&capRenderUi),
                                    "render.ui");
        rt.registerKernelCapability(GAME_CAP_AUDIO_PLAY,
                                    gameHash("sig.audio.play.v2"), 0,
                                    reinterpret_cast<void*>(&capAudioPlay),
                                    "audio.play");
        rt.registerKernelCapability(GAME_CAP_SURFACE_EFFECT,
                                    gameHash("sig.surface.effect.v1"), 0,
                                    reinterpret_cast<void*>(&capSurfaceEffect),
                                    "surface.effect");
        rt.registerKernelCapability(GAME_CAP_CAMERA_EFFECT,
                                    gameHash("sig.camera.effect.v1"), 0,
                                    reinterpret_cast<void*>(&capCameraEffect),
                                    "camera.effect");
        rt.registerKernelCapability(GAME_CAP_SOCKET_QUERY,
                                    gameHash("sig.socket.query.v1"), 0,
                                    reinterpret_cast<void*>(&capSocketQuery),
                                    "socket.query");
        rt.registerKernelCapability(GAME_CAP_SOCKET_RAW,
                                    gameHash("sig.socket.raw.v1"), 0,
                                    reinterpret_cast<void*>(&capSocketRaw),
                                    "socket.raw");
        rt.registerKernelCapability(GAME_CAP_MESH_BOUNDS,
                                    gameHash("sig.mesh.bounds.v1"), 0,
                                    reinterpret_cast<void*>(&capMeshBounds),
                                    "mesh.bounds");
        rt.registerKernelCapability(GAME_CAP_MESH_PART_BOUNDS,
                                    gameHash("sig.mesh.part-bounds.v1"), 0,
                                    reinterpret_cast<void*>(&capMeshPartBounds),
                                    "mesh.part-bounds");
        rt.registerKernelCapability(GAME_CAP_BODY_PARTS,
                                    gameHash("sig.body.parts.v1"), 0,
                                    reinterpret_cast<void*>(&capBodyParts),
                                    "body.parts");
        rt.registerKernelCapability(GAME_CAP_WORLD_PROJECT,
                                    gameHash("sig.world.project.v1"), 0,
                                    reinterpret_cast<void*>(&capWorldProject),
                                    "world.project");
        rt.registerKernelCapability(GAME_CAP_SETTING_GET,
                                    gameHash("sig.setting.get.v1"), 0,
                                    reinterpret_cast<void*>(&capSettingGet),
                                    "setting.get");
        rt.registerKernelCapability(GAME_CAP_SETTING_SET,
                                    gameHash("sig.setting.set.v1"), 0,
                                    reinterpret_cast<void*>(&capSettingSet),
                                    "setting.set");
        rt.registerKernelCapability(GAME_CAP_RESOURCE_REGISTER,
                                    gameHash("sig.resource.register.v1"), 0,
                                    reinterpret_cast<void*>(&capResourceRegister),
                                    "resource.register");
        rt.registerKernelCapability(GAME_CAP_EFFECT_SPAWN,
                                    gameHash("sig.effect.spawn.v1"), 0,
                                    reinterpret_cast<void*>(&capEffectSpawn),
                                    "effect.spawn");
        rt.registerKernelCapability(GAME_CAP_EFFECT_PART,
                                    gameHash("sig.effect.part.v1"), 0,
                                    reinterpret_cast<void*>(&capEffectPart),
                                    "effect.part");
        rt.registerKernelCapability(GAME_CAP_EFFECT_POOL,
                                    gameHash("sig.effect.pool.v1"), 0,
                                    reinterpret_cast<void*>(&capEffectPool),
                                    "effect.pool");
        rt.registerKernelCapability(GAME_CAP_SKELETON_APPLY,
                                    gameHash("sig.skeleton.apply.v1"), 0,
                                    reinterpret_cast<void*>(&capSkeletonApply),
                                    "skeleton.apply");
        rt.registerKernelCapability(GAME_CAP_SKELETON_VALIDATE,
                                    gameHash("sig.skeleton.validate.v1"), 0,
                                    reinterpret_cast<void*>(&capSkeletonValidate),
                                    "skeleton.validate");
        rt.registerKernelCapability(GAME_CAP_WORLD_COLLISION,
                                    gameHash("sig.world.collision.v1"), 0,
                                    reinterpret_cast<void*>(&capWorldCollision),
                                    "world.collision");
        rt.registerKernelCapability(GAME_CAP_PHYSICS_IMPULSE,
                                    gameHash("sig.physics.impulse.v1"), 0,
                                    reinterpret_cast<void*>(&capPhysicsImpulse),
                                    "physics.impulse");
        rt.registerKernelCapability(GAME_CAP_PHYSICS_MOVE,
                                    gameHash("sig.physics.move.v1"), 0,
                                    reinterpret_cast<void*>(&capPhysicsMove),
                                    "physics.move");
        rt.registerKernelCapability(GAME_CAP_INPUT_READ,
                                    gameHash("sig.input.read.v1"), 0,
                                    reinterpret_cast<void*>(&capInputRead),
                                    "input.read");
        rt.registerKernelCapability(GAME_CAP_CAMERA_READ,
                                    gameHash("sig.camera.read.v1"), 0,
                                    reinterpret_cast<void*>(&capCameraRead),
                                    "camera.read");
        rt.registerKernelCapability(GAME_CAP_ACTOR_SKELETON_WRITE,
                                    gameHash("sig.actor.skeleton.write.v1"), 0,
                                    reinterpret_cast<void*>(&capActorSkeletonWrite),
                                    "actor.skeleton.write");
        rt.registerKernelCapability(GAME_CAP_RAGDOLL_SNAPSHOT,
                                    gameHash("sig.ragdoll.snapshot.v1"), 0,
                                    reinterpret_cast<void*>(&capRagdollSnapshot),
                                    "ragdoll.snapshot");
        rt.registerKernelCapability(GAME_CAP_RAGDOLL_BIND,
                                    gameHash("sig.ragdoll.bind.v1"), 0,
                                    reinterpret_cast<void*>(&capRagdollBind),
                                    "ragdoll.bind");
    }
};
const KernelCapabilityInit s_kernelCapabilities{};

GameplayContextV1 makeContext(std::uint64_t tick)
{
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    GameplayContextV1 context{};
    context.abiVersion = MIMITA_GAME_API_VERSION;
    context.structSize = sizeof(GameplayContextV1);
    context.host = nullptr;
    context.tick = tick;
    context.generation = status.activeGeneration;
    context.codeHash = 0;
    context.emitEvent = reinterpret_cast<void*>(&kernelEmitEvent);
    context.readComponent = &capReadComponent;
    context.writeComponent = &capWriteComponent;
    context.findEntities = &capFindEntities;
    context.queryWorldRay = &capQueryWorldRay;
    context.log = &capLog;
    context.dynamicReadComponent = &capDynamicReadComponent;
    context.dynamicWriteComponent = &capDynamicWriteComponent;
    context.requestMovementOverride = &capRequestMovementOverride;
    context.entityCreate = &capEntityCreate;
    context.entityDestroy = &capEntityDestroy;
    context.dynamicRemoveComponent = &capDynamicRemoveComponent;
    context.dynamicEnumerateComponent = &capDynamicEnumerateComponent;
    context.dynamicComponentsOnEntity = &capDynamicComponentsOnEntity;
    context.dynamicComponentInfo = &capDynamicComponentInfo;
    context.relationshipAdd = &capRelationshipAdd;
    context.relationshipRemove = &capRelationshipRemove;
    context.relationshipQuery = &capRelationshipQuery;
    context.matchCurrent = &capMatchCurrent;
    context.matchActorTeamRead = &capMatchActorTeamRead;
    context.matchFinish = &capMatchFinish;
    context.matchSetPhase = &capMatchSetPhase;
    context.matchRespawn = &capMatchRespawn;
    context.matchSetTeam = &capMatchSetTeam;
    context.resolveCapability = &capResolveCapability;
    GameMemory& memory = HotReloadSystem::instance().gameMemory();
    context.permanentStorage = memory.permanentStorage;
    context.permanentStorageSize = memory.permanentStorageSize;
    return context;
}

} // namespace

namespace LiveBehavior {

bool available()
{
    const GameGameplayModuleV1* module = gameplayModule();
    return module && module->onEvent != nullptr;
}

GameplayContextV1* hostContext(std::uint64_t tick)
{
    static thread_local GameplayContextV1 context;
    context = makeContext(tick);
    return &context;
}

void enqueueEvent(const GameEventV1& event)
{
    if (gCount >= kMaxQueuedEvents)
        return;
    QueuedEvent& slot = gQueue[(gHead + gCount) % kMaxQueuedEvents];
    slot.header = event;
    if (event.payload && event.payloadSize > 0 && event.payloadSize <= kMaxPayloadBytes) {
        std::memcpy(slot.payload, event.payload, event.payloadSize);
        slot.header.payload = slot.payload;
    } else {
        slot.header.payload = nullptr;
        slot.header.payloadSize = 0;
    }
    ++gCount;
}

bool dispatchEvent(const GameEventV1& event, std::uint64_t tick)
{
    // Generic runtime subscribers first (runtime-registered event types are the
    // forward path); the legacy gameplay module is kept as a fallback. Handlers
    // receive a capability context so queued events can perform real work.
    GameplayContextV1 context = makeContext(tick);
    const bool generic = MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return generic;
    module->onEvent(&event, &context);
    return true;
}

bool dispatchActorKilled(GameActorKilledV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = gameHash("actor.killed");
    event.schemaHash = gameHash("actor.killed.v1");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(GameActorKilledV1);
    event.sourceEntity = payload.killerEntity;
    event.targetEntity = payload.victimEntity;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    return payload.handled != 0;
}

bool dispatchMatchEvaluate(GameMatchEvaluateV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = gameHash("match.evaluate");
    event.schemaHash = gameHash("match.evaluate.v1");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(GameMatchEvaluateV1);
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    return payload.handled != 0;
}

bool dispatchActorLifecycle(ActorLifecycleStateV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = GAME_EVENT_ACTOR_LIFECYCLE;
    event.schemaHash = gameHash("actor.lifecycle.v1");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(ActorLifecycleStateV1);
    event.sourceEntity = payload.entityId;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    const GameGameplayModuleV1* module = gameplayModule();
    if (module && module->onEvent)
        module->onEvent(&event, &context);
    return payload.handled != 0;
}

bool dispatchActorAvatarPolicy(ActorAvatarPolicyV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = GAME_EVENT_ACTOR_AVATAR_POLICY;
    event.schemaHash = gameHash("actor.avatar-policy.v1");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(ActorAvatarPolicyV1);
    event.sourceEntity = payload.entityId;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    const GameGameplayModuleV1* module = gameplayModule();
    if (module && module->onEvent)
        module->onEvent(&event, &context);
    return payload.handled != 0;
}

bool dispatchProjectileImpact(ProjectileImpactPolicyV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = gameHash("projectile.impact");
    event.schemaHash = gameHash("projectile.impact.v1");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(ProjectileImpactPolicyV1);
    event.sourceEntity = payload.ownerEntity;
    event.targetEntity = payload.victimEntity;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    return payload.handled != 0;
}

bool dispatchToolUse(ToolUsePolicyV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    // Per-entity behavior bindings take priority over the global keyed router:
    // the equipped tool entity declares which behavior owns its use.
    if (payload.toolEntity != 0) {
        const std::uint32_t primaryUse =
            static_cast<std::uint32_t>(gameHash("on.primary-use"));
        if (runBehaviorBindings(payload.toolEntity, primaryUse, &payload,
                                sizeof(payload), tick))
            return payload.handled != 0;
    }
    GameEventV1 event{};
    event.typeId = payload.kind == 1 ? gameHash("tool.alt-use") : gameHash("tool.primary-use");
    event.schemaHash = gameHash("tool.use.v1");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(ToolUsePolicyV1);
    event.sourceEntity = payload.userEntity;
    event.targetEntity = payload.toolEntity;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    return payload.handled != 0;
}

bool dispatchEffectRequest(EffectRequestV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = gameHash("effect.request");
    event.schemaHash = gameHash("effect.request.v3");
    event.payloadVersion = 1;
    event.payloadSize = sizeof(EffectRequestV1);
    event.sourceEntity = payload.sourceEntity;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    return payload.handled != 0;
}

bool dispatchPayload(std::uint32_t typeId, void* payload,
                     std::uint32_t payloadSize, std::uint64_t tick,
                     std::uint64_t sourceEntity,
                     std::uint64_t targetEntity,
                     std::uint64_t projectileEntity)
{
    if (!payload || payloadSize == 0)
        return false;

    GameEventV1 event{};
    event.typeId = typeId;
    event.payloadVersion = GAMEPLAY_EVENT_VERSION;
    event.payloadSize = payloadSize;
    event.flags = 0;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.projectileEntity = projectileEntity;
    event.tick = tick;
    event.payload = payload;
    GameplayContextV1 context = makeContext(tick);
    bool handled = MimitaRuntime::GenericRuntime::instance().dispatchEvent(event,
                                                                            &context);

    const GameGameplayModuleV1* module = gameplayModule();
    if (module && module->onEvent) {
        module->onEvent(&event, &context);
        handled = true;
    }
    drainEvents(16);
    return handled;
}

bool dispatchGameplayEvent64(std::uint64_t typeId, void* payload,
                             std::uint32_t payloadSize, std::uint64_t tick,
                             std::uint64_t sourceEntity,
                             std::uint64_t targetEntity)
{
    GameEventV1 event{};
    event.typeId = typeId;
    event.schemaHash = 0;
    event.payloadVersion = 1;
    event.payloadSize = payloadSize;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.tick = tick;
    event.payload = payload;
    GameplayContextV1 context = makeContext(tick);
    bool handled = MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    const GameGameplayModuleV1* module = gameplayModule();
    if (module && module->onEvent) {
        module->onEvent(&event, &context);
        handled = true;
    }
    return handled;
}

bool runBehaviorBindings(std::uint64_t entity, std::uint32_t eventType,
                         void* payload, std::uint32_t payloadSize,
                         std::uint64_t tick)
{
    if (entity == 0 || !payload || payloadSize == 0)
        return false;
    const BehaviorBindingsComponent* bindings =
        EntityRegistry::instance().tryGet<BehaviorBindingsComponent>(
            static_cast<EntityId>(entity));
    if (!bindings || bindings->count <= 0)
        return false;
    bool ran = false;
    for (int i = 0; i < bindings->count; ++i) {
        const BehaviorBinding& binding = bindings->bindings[i];
        if (binding.eventType != eventType || binding.behaviorId == 0)
            continue;
        dispatchGameplayEvent64(binding.behaviorId, payload, payloadSize, tick,
                                entity, 0);
        ran = true;
    }
    return ran;
}

void setPacketReplySink(LiveBehavior::PacketReplySink sink)
{
    gPacketReplySink = sink;
}

void clearPacketReplySink()
{
    gPacketReplySink = nullptr;
}

void setDispatchWorld(const void* world)
{
    gDispatchWorld = world;
}

void setDispatchHeadlessWorld(const void* world)
{
    gDispatchHeadlessWorld = world;
}

void setDispatchInput(const void* input)
{
    gDispatchInput = input;
}

void setDispatchCamera(const void* camera)
{
    gDispatchCamera = camera;
}

void flushRenderDebug()
{
    ::flushDebugLines(THE_CAMERA);
}

std::uint64_t skeletonApplyCount()
{
    return g_skeletonApplyCount;
}

std::uint64_t animationUpdateCount()
{
    return g_animationUpdateCount;
}

std::uint64_t audioPlayCount()
{
    return g_audioPlayCount;
}

std::uint64_t audioCommandRejectCount()
{
    return g_audioRejectCount;
}

std::uint64_t surfaceEffectCount()
{
    return g_surfaceEffectCount;
}

std::uint64_t cameraEffectCount()
{
    return g_cameraEffectCount;
}

int drainEvents(int maxEvents)
{
    if (gDraining)
        return 0;
    gDraining = true;
    int processed = 0;
    while (gCount > 0 && processed < maxEvents) {
        QueuedEvent slot = gQueue[gHead];
        gHead = (gHead + 1) % kMaxQueuedEvents;
        --gCount;
        // Re-point the payload back into the live slot copy before dispatch.
        GameEventV1 event = slot.header;
        if (event.payload)
            event.payload = slot.payload;
        dispatchEvent(event, event.tick);
        ++processed;
    }
    gDraining = false;
    return processed;
}

bool dispatchDamagePolicy(DamagePolicyV1& payload, std::uint64_t tick)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return false;

    payload.handled = 0;

    GameEventV1 event{};
    event.typeId = GAME_EVENT_DAMAGE_POLICY;
    event.payloadVersion = GAMEPLAY_EVENT_VERSION;
    event.payloadSize = sizeof(DamagePolicyV1);
    event.flags = 0;
    event.sourceEntity = payload.attackerEntity;
    event.targetEntity = payload.victimEntity;
    event.projectileEntity = payload.projectileEntity;
    event.tick = tick;
    event.payload = &payload;

    GameplayContextV1 context = makeContext(tick);
    module->onEvent(&event, &context);

    // Behaviors may have emitted nested events; process them FIFO.
    drainEvents(16);
    return payload.handled != 0;
}

bool dispatchFireIntent(FireIntentPolicyV1& payload, std::uint64_t tick)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return false;

    payload.handled = 0;

    GameEventV1 event{};
    event.typeId = GAME_EVENT_FIRE_INTENT;
    event.payloadVersion = GAMEPLAY_EVENT_VERSION;
    event.payloadSize = sizeof(FireIntentPolicyV1);
    event.flags = 0;
    event.sourceEntity = payload.entity;
    event.targetEntity = 0;
    event.projectileEntity = 0;
    event.tick = tick;
    event.payload = &payload;

    GameplayContextV1 context = makeContext(tick);
    module->onEvent(&event, &context);
    drainEvents(16);
    return payload.handled != 0;
}

bool dispatchAttackPolicy(AttackPolicyV1& payload, std::uint64_t tick)
{
    payload.handled = 0;
    GameEventV1 event{};
    event.typeId = GAME_EVENT_HASH_ATTACK_POLICY;
    event.schemaHash = GAME_EVENT_HASH_ATTACK_POLICY;
    event.payloadVersion = GAMEPLAY_EVENT_VERSION;
    event.payloadSize = sizeof(AttackPolicyV1);
    event.sourceEntity = payload.shooterEntity;
    event.targetEntity = payload.claimedTargetId;
    event.projectileEntity = 0;
    event.tick = tick;
    event.payload = &payload;
    GameplayContextV1 context = makeContext(tick);
    MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, &context);
    drainEvents(16);
    return payload.handled != 0;
}

bool dispatchRagdollPolicy(RagdollPolicyV1& payload, std::uint64_t tick)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return false;

    payload.handled = 0;

    GameEventV1 event{};
    event.typeId = GAME_EVENT_RAGDOLL_SOLVE;
    event.payloadVersion = GAMEPLAY_EVENT_VERSION;
    event.payloadSize = sizeof(RagdollPolicyV1);
    event.flags = 0;
    event.sourceEntity = payload.ownerActor;
    event.targetEntity = 0;
    event.projectileEntity = 0;
    event.tick = tick;
    event.payload = &payload;

    GameplayContextV1 context = makeContext(tick);
    module->onEvent(&event, &context);
    drainEvents(16);
    return payload.handled != 0;
}

bool capsuleMove(const float inPos[3], const float inVel[3], float radius,
                 float halfHeight, float yaw, float sizeScale, float dt,
                 float outPos[3], float outVel[3], bool& grounded,
                 bool& collided)
{
    auto* fn = reinterpret_cast<GameCapsuleMoveFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_CAPSULE_MOVE));
    if (!fn)
        return false;
    GameCapsuleMoveV1 m{};
    m.structSize = sizeof(GameCapsuleMoveV1);
    for (int i = 0; i < 3; ++i) {
        m.position[i] = inPos[i];
        m.velocity[i] = inVel[i];
    }
    m.radius = radius;
    m.halfHeight = halfHeight;
    m.yaw = yaw;
    m.sizeScale = sizeScale;
    m.dt = dt;
    fn(hostContext(0), &m);
    if (m.handled == 0u)
        return false;
    for (int i = 0; i < 3; ++i) {
        outPos[i] = m.outPosition[i];
        outVel[i] = m.outVelocity[i];
    }
    grounded = m.grounded != 0u;
    collided = m.collided != 0u;
    return true;
}

} // namespace LiveBehavior

namespace {

void MIMITA_GAME_CALL kernelEmitEvent(GameplayContextV1*, const GameEventV1* event)
{
    if (event)
        LiveBehavior::enqueueEvent(*event);
}

} // namespace
