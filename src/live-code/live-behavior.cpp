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

#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/generic-runtime.h"
#include "ecs/dynamic-components.h"
#include "live-code/live-modules.h"
#include "physics/movement/move-capsule.h"
#include "world/world.h"

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
            case GAME_COMPONENT_HEALTH: if (!registry.has<HealthComponent>(id)) continue; break;
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
    if (message)
        std::printf("[HOT] %s\n", message);
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

void MIMITA_GAME_CALL capRequestMovementOverride(void*, std::uint32_t flags,
                                                 const float position[3],
                                                 const float velocity[3], float yaw)
{
    MimitaRuntime::GenericRuntime::instance().requestMovementOverride(
        flags, position, velocity, yaw);
}

void MIMITA_GAME_CALL capMoveCapsule(void*, MovementStateV1* state, float dt)
{
    if (!state)
        return;
    Physics::moveCapsuleStep(*state, static_cast<const World*>(gDispatchWorld), dt);
}

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
    // forward path); the legacy gameplay module is kept as a fallback.
    const bool generic = MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, nullptr);
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return generic;
    GameplayContextV1 context = makeContext(tick);
    module->onEvent(&event, &context);
    return true;
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

    bool handled = MimitaRuntime::GenericRuntime::instance().dispatchEvent(event, nullptr);

    const GameGameplayModuleV1* module = gameplayModule();
    if (module && module->onEvent) {
        GameplayContextV1 context = makeContext(tick);
        module->onEvent(&event, &context);
        handled = true;
    }
    drainEvents(16);
    return handled;
}

void setDispatchWorld(const void* world)
{
    gDispatchWorld = world;
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
