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
#include "live-code/live-ui.h"
#include "hot-reload/hot-pose.h"
#include "render/skeleton-instances.h"
#include "network/server-context.h"
#include "network/server-gamemode.h"
#include "network/server.h"
#include "network/server.h"
#include "physics/movement/move-capsule.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "world/world.h"
#include "entities/player.h"
#include "camera.h"
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

#include <glm/gtc/quaternion.hpp>
#include "physics/ray-utils.h"
#include "ragdoll/ragdoll-components.h"
#include "world/world.h"

namespace {

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
std::uint64_t g_skeletonApplyCount = 0;
std::uint64_t g_animationUpdateCount = 0;
std::uint64_t g_audioPlayCount = 0;
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
        o->dashCooldownSeconds=c->dashCooldownSeconds;
        o->jumpIntentSeconds=c->jumpIntentSeconds;
        o->dashGraceSeconds=c->dashGraceSeconds;
        o->freezePreviously=c->freezePreviously?1u:0u;
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
        c.dashCooldownSeconds=i->dashCooldownSeconds;
        c.jumpIntentSeconds=i->jumpIntentSeconds;
        c.dashGraceSeconds=i->dashGraceSeconds;
        c.freezePreviously=i->freezePreviously!=0;
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

void MIMITA_GAME_CALL capLog(void*, const char* message)
{
    if (!message)
        return;
    std::printf("[HOT] %s\n", message);
    // File-backed sink so hot diagnostics are saved with the rest of the
    // categorized logs (logs/<date>/...), not just printed.
    ::StructuredLogger::Entry e;
    e.category = ::StructuredCategory::Network;
    e.level = ::StructuredLevel::Verbose;
    e.eventId = "hot.log";
    e.reason = "hot";
    e.sourceFile = "live-behavior";
    e.sourceLine = 0;
    e.functionName = "capLog";
    e.message = message;
    ::StructuredLogger::instance().write(e);
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
    pos += vel * dt;

    bool onGround = false;
    if (world)
        MimitaNet::resolveCapsuleCollisionAgainstWorld(
            *world, pos, vel, radius, halfHeight * 2.0f, onGround);

    s->position[0] = pos.x; s->position[1] = pos.y; s->position[2] = pos.z;
    s->velocity[0] = vel.x; s->velocity[1] = vel.y; s->velocity[2] = vel.z;
    s->grounded = onGround ? 1u : 0u;
    s->collided = 1u;
}

} // namespace

void MIMITA_GAME_CALL capMoveCapsule(void*, MovementStateV1* state, float dt)
{
    if (!state)
        return;
    Physics::moveCapsuleStep(*state, static_cast<const World*>(gDispatchWorld), dt);
}

// ── Generic kernel primitives (ABI v8), resolved by id ──────────────────

// physics.move: low-level, policy-free. The caller supplies capsule size,
// velocity, and gravity scale; the kernel runs the shared built-in collision
// pipeline (sweep/slide, step-up, floor recovery, contact-grounded) on the real
// local player and writes the resolved state back. No movement-policy logic.
void MIMITA_GAME_CALL capPhysicsMove(void*, MovementStateV1* s, float dt,
                                     std::uint32_t flags)
{
    if (!s || dt <= 0.0f)
        return;

    // Explicit headless-world selection: hot code sets this flag for server
    // actors so the same primitive resolves against the authoritative server
    // collision. Never implicit, so a listen host's client path is unaffected.
    if ((flags & GAME_PHYSICS_MOVE_HEADLESS) && gDispatchHeadlessWorld)
    {
        moveCapsuleStepHeadless(s, static_cast<const MimitaNet::HeadlessWorld*>(gDispatchHeadlessWorld), dt);
        return;
    }

    const World* world = static_cast<const World*>(gDispatchWorld);

    // The full pipeline operates on the real local player. When there is no
    // local player (headless selftests, dedicated-server contexts), fall back to
    // the generic capsule solve so the primitive is still usable and safe.
    if (!gpPlayer)
    {
        Physics::moveCapsuleStep(*s, world, dt);
        return;
    }

    Player& p = THE_PLAYER;

    if (s->radius > 0.0f)
        p.movementCapsule.radius = s->radius;
    if (s->halfHeight > 0.0f)
        p.movementCapsule.height = s->halfHeight * 2.0f;
    p.pos = glm::vec3(s->position[0], s->position[1], s->position[2]);
    p.movementCapsule.position = p.pos;
    p.vel = glm::vec3(s->velocity[0], s->velocity[1], s->velocity[2]);
    p.externalImpulse = glm::vec3(0.0f);

    if (s->gravityScale > 0.0f)
        p.vel.z -= 9.81f * s->gravityScale * dt;

    bool grounded = false;
    if (world)
    {
        setCollisionEntityContext("Player", 0, false);
        p.movementContacts.clear();
        doCollisions(p, *world, grounded, dt);
        clearCollisionEntityContext();
    }

    s->position[0] = p.pos.x;
    s->position[1] = p.pos.y;
    s->position[2] = p.pos.z;
    s->velocity[0] = p.vel.x;
    s->velocity[1] = p.vel.y;
    s->velocity[2] = p.vel.z;
    s->grounded = grounded ? 1u : 0u;
    // Any real world contact (ground, wall, ceiling, prop) — used by hot
    // movement to reset abilities on touch, per the movement spec.
    s->collided = (grounded || !p.movementContacts.empty()) ? 1u : 0u;
}

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
void MIMITA_GAME_CALL capAudioPlay(void*, const GameAudioCommandV1* command)
{
    if (!command || command->sound[0] == '\0')
        return;
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
    // One-shot playback.
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
                                    gameHash("sig.audio.play.v1"), 0,
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
        rt.registerKernelCapability(GAME_CAP_PHYSICS_MOVE,
                                    gameHash("sig.physics.move.v1"), 0,
                                    reinterpret_cast<void*>(&capPhysicsMove),
                                    "physics.move");
        rt.registerKernelCapability(GAME_CAP_EFFECT_SPAWN,
                                    gameHash("sig.effect.spawn.v1"), 0,
                                    reinterpret_cast<void*>(&capEffectSpawn),
                                    "effect.spawn");
        rt.registerKernelCapability(GAME_CAP_EFFECT_PART,
                                    gameHash("sig.effect.part.v1"), 0,
                                    reinterpret_cast<void*>(&capEffectPart),
                                    "effect.part");
        rt.registerKernelCapability(GAME_CAP_SKELETON_APPLY,
                                    gameHash("sig.skeleton.apply.v1"), 0,
                                    reinterpret_cast<void*>(&capSkeletonApply),
                                    "skeleton.apply");
        rt.registerKernelCapability(GAME_CAP_SKELETON_VALIDATE,
                                    gameHash("sig.skeleton.validate.v1"), 0,
                                    reinterpret_cast<void*>(&capSkeletonValidate),
                                    "skeleton.validate");
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
    context.moveCapsule = &capMoveCapsule;
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

void setDispatchWorld(const void* world)
{
    gDispatchWorld = world;
}

void setDispatchHeadlessWorld(const void* world)
{
    gDispatchHeadlessWorld = world;
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

} // namespace LiveBehavior

namespace {

void MIMITA_GAME_CALL kernelEmitEvent(GameplayContextV1*, const GameEventV1* event)
{
    if (event)
        LiveBehavior::enqueueEvent(*event);
}

} // namespace
