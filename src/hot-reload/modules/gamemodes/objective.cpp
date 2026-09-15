// 09 14 2026
/* purpose
* Generic hot objective behavior. An objective is an entity + package-private
* dynamic components + relationships; the kernel never knows what it means.
* Two independent behaviors (carry-to-site, hold-area) use the SAME primitives:
* entity create, dynamic components, relationships, the generic objective
* interaction/state-change events, and generic match mechanisms. It signals
* ObjectiveOwnership so the cold bomb state machine is bypassed.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "network/objective-events.h"

#include <algorithm>
#include <cstdint>
#include <vector>

using MimitaNet::GameObjectiveInteractV1;
using MimitaNet::GameObjectiveStateV1;

namespace {

// Package-private schemas (no kernel ObjectiveType / BombManager).
const std::uint64_t kObjectiveState = gameHash("ObjectiveState");
const std::uint64_t kObjectiveProgress = gameHash("ObjectiveProgress");
const std::uint64_t kObjectiveEventLog = gameHash("ObjectiveEventLog");

// Package-private relationship types.
const std::uint64_t kCarriedBy = gameHash("objective.carried-by");
const std::uint64_t kAtSite = gameHash("objective.at-site");

// Generic runtime event ids and behavior domains.
const std::uint64_t kInteractEvent = gameHash("objective.interact");
const std::uint64_t kStateChangedEvent = gameHash("objective.state-changed");
const std::uint64_t kCarryDomain = gameHash("objective.carry");
const std::uint64_t kHoldDomain = gameHash("objective.hold");
const std::uint64_t kOwnership = gameHash("ObjectiveOwnership");

const std::uint64_t kHealthState = gameHash("ActorHealthState");

// kind values are package-private policy, not a kernel enum.
constexpr std::uint32_t kKindCarry = 0;
constexpr std::uint32_t kKindHold = 1;

constexpr std::uint32_t kStateIdle = 0;
constexpr std::uint32_t kStateCarried = 1;
constexpr std::uint32_t kStateComplete = 2;

constexpr float kSiteX = 10.0f;
constexpr float kSiteY = 0.0f;
constexpr float kSiteZ = 0.0f;
constexpr float kReachRadius = 2.5f;

struct ObjectiveStateV1 {
    std::uint32_t kind;
    std::uint32_t state;
    std::uint32_t ownerTeam;
    std::uint32_t flags;
};
struct ObjectiveProgressV1 {
    float progress;
    float goal;
    float ratePerTick;
    std::uint32_t reserved;
};
struct ObjectiveEventLogV1 {
    std::uint32_t lastState;
    std::uint32_t count;
    std::uint32_t tick;
    std::uint32_t reserved;
};
struct ActorHealthMirror {
    std::int32_t current;
    std::int32_t max;
    std::uint32_t dead;
    std::uint32_t reserved;
};

std::uint64_t currentMatch(GameplayContextV1* ctx)
{
    std::uint64_t match = 0;
    if (ctx && ctx->matchCurrent)
        ctx->matchCurrent(ctx->host, &match);
    return match;
}

bool entityPos(GameplayContextV1* ctx, std::uint64_t entity, float* out)
{
    GameTransformComponentV1 t{};
    if (!ctx || !ctx->readComponent ||
        !ctx->readComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &t, sizeof(t)))
        return false;
    out[0] = t.position[0];
    out[1] = t.position[1];
    out[2] = t.position[2];
    return true;
}

void setEntityPos(GameplayContextV1* ctx, std::uint64_t entity, float x, float y, float z)
{
    if (!ctx || !ctx->writeComponent)
        return;
    GameTransformComponentV1 t{};
    t.position[0] = x;
    t.position[1] = y;
    t.position[2] = z;
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &t, sizeof(t));
}

bool actorAlive(GameplayContextV1* ctx, std::uint64_t entity)
{
    ActorHealthMirror h{};
    if (!ctx || !ctx->dynamicReadComponent)
        return false;
    if (!ctx->dynamicReadComponent(ctx->host, entity, kHealthState, &h, sizeof(h)))
        return false;
    return h.dead == 0 && h.current > 0;
}

bool readObjState(GameplayContextV1* ctx, std::uint64_t objective, ObjectiveStateV1& out)
{
    return ctx && ctx->dynamicReadComponent &&
           ctx->dynamicReadComponent(ctx->host, objective, kObjectiveState, &out,
                                     sizeof(out));
}

void writeObjState(GameplayContextV1* ctx, std::uint64_t objective,
                   const ObjectiveStateV1& s)
{
    if (ctx && ctx->dynamicWriteComponent)
        ctx->dynamicWriteComponent(ctx->host, objective, kObjectiveState, &s,
                                   sizeof(s));
}

void writeProgress(GameplayContextV1* ctx, std::uint64_t objective,
                   const ObjectiveProgressV1& p)
{
    if (ctx && ctx->dynamicWriteComponent)
        ctx->dynamicWriteComponent(ctx->host, objective, kObjectiveProgress, &p,
                                   sizeof(p));
}

void emitStateChanged(GameplayContextV1* ctx, std::uint64_t objective,
                      std::uint64_t actor, std::uint64_t site, std::uint32_t state,
                      std::uint32_t tick)
{
    if (!ctx || !ctx->emitEvent)
        return;
    GameObjectiveStateV1 p{};
    p.objectiveEntity = objective;
    p.actorEntity = actor;
    p.siteEntity = site;
    p.state = state;
    p.tick = tick;
    GameEventV1 e{};
    e.typeId = kStateChangedEvent;
    e.payloadVersion = 1;
    e.payloadSize = sizeof(p);
    e.tick = tick;
    e.payload = &p;
    reinterpret_cast<GameEmitEventFn>(ctx->emitEvent)(ctx, &e);
}

// Lazy-init the objective + site entities for a kind the first time it ticks.
std::uint64_t ensureObjective(GameplayContextV1* ctx, std::uint32_t kind)
{
    if (!ctx || !ctx->dynamicEnumerateComponent)
        return 0;
    std::uint64_t found[16] = {0};
    const std::uint32_t n =
        ctx->dynamicEnumerateComponent(ctx->host, kObjectiveState, found, 16);
    for (std::uint32_t i = 0; i < n; ++i) {
        ObjectiveStateV1 s{};
        if (readObjState(ctx, found[i], s) && s.kind == kind)
            return found[i];
    }
    // Create the objective entity (kind-generic, no ObjectiveType).
    std::uint64_t objective = 0;
    if (!ctx->entityCreate || !ctx->entityCreate(ctx->host, 0, &objective))
        return 0;
    ObjectiveStateV1 s{};
    s.kind = kind;
    s.state = kStateIdle;
    s.ownerTeam = 0;
    writeObjState(ctx, objective, s);
    ObjectiveProgressV1 p{};
    p.goal = 5.0f;
    p.ratePerTick = 1.0f;
    writeProgress(ctx, objective, p);
    setEntityPos(ctx, objective, 0.0f, 0.0f, 0.0f);

    // Site is an entity too, linked by a relationship (no hardcoded site slot).
    std::uint64_t site = 0;
    if (ctx->entityCreate && ctx->entityCreate(ctx->host, 0, &site)) {
        setEntityPos(ctx, site, kSiteX, kSiteY, kSiteZ);
        if (ctx->relationshipAdd)
            ctx->relationshipAdd(ctx->host, kAtSite, objective, site, 0);
    }

    // Claim objective ownership so the cold bomb branch is bypassed.
    const std::uint64_t match = currentMatch(ctx);
    if (match != 0 && ctx->dynamicWriteComponent) {
        std::uint32_t owned = 1;
        ctx->dynamicWriteComponent(ctx->host, match, kOwnership, &owned,
                                   sizeof(owned));
    }
    return objective;
}

std::uint64_t carrierOf(GameplayContextV1* ctx, std::uint64_t objective)
{
    if (!ctx || !ctx->relationshipQuery)
        return 0;
    std::uint64_t to = 0;
    std::uint64_t value = 0;
    if (ctx->relationshipQuery(ctx->host, kCarriedBy, objective, &to, &value, 1) > 0)
        return to;
    return 0;
}

std::uint64_t siteOf(GameplayContextV1* ctx, std::uint64_t objective)
{
    if (!ctx || !ctx->relationshipQuery)
        return 0;
    std::uint64_t to = 0;
    std::uint64_t value = 0;
    if (ctx->relationshipQuery(ctx->host, kAtSite, objective, &to, &value, 1) > 0)
        return to;
    return 0;
}

void MIMITA_GAME_CALL carryTick(void* host, std::uint64_t tick, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx)
        return;
    const std::uint64_t objective = ensureObjective(ctx, kKindCarry);
    if (objective == 0)
        return;
    ObjectiveStateV1 state{};
    if (!readObjState(ctx, objective, state))
        return;

    const std::uint64_t carrier = carrierOf(ctx, objective);
    if (carrier != 0 && !actorAlive(ctx, carrier)) {
        // Stale carrier: drop via the generic relationship, keep the same entity.
        if (ctx->relationshipRemove)
            ctx->relationshipRemove(ctx->host, kCarriedBy, objective, carrier);
        state.state = kStateIdle;
        writeObjState(ctx, objective, state);
        emitStateChanged(ctx, objective, 0, siteOf(ctx, objective), kStateIdle,
                         (std::uint32_t)tick);
        return;
    }

    if (carrier != 0 && state.state == kStateCarried) {
        const std::uint64_t site = siteOf(ctx, objective);
        float cpos[3] = {0};
        float spos[3] = {0};
        if (entityPos(ctx, carrier, cpos) && site != 0 && entityPos(ctx, site, spos)) {
            const float dx = cpos[0] - spos[0];
            const float dy = cpos[1] - spos[1];
            const float dz = cpos[2] - spos[2];
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 <= kReachRadius * kReachRadius) {
                ObjectiveProgressV1 p{};
                if (ctx->dynamicReadComponent)
                    ctx->dynamicReadComponent(ctx->host, objective, kObjectiveProgress,
                                              &p, sizeof(p));
                if (p.goal <= 0.0f)
                    p.goal = 5.0f;
                p.progress += p.ratePerTick > 0.0f ? p.ratePerTick : 1.0f;
                if (p.progress >= p.goal) {
                    p.progress = p.goal;
                    writeProgress(ctx, objective, p);
                    state.state = kStateComplete;
                    writeObjState(ctx, objective, state);
                    emitStateChanged(ctx, objective, carrier, site, kStateComplete,
                                     (std::uint32_t)tick);
                    if (ctx->matchFinish)
                        ctx->matchFinish(ctx->host, 2 /*team*/, 0 /*red*/, 0 /*score*/);
                    return;
                }
                writeProgress(ctx, objective, p);
            }
        }
    } else if (carrier == 0 && state.state == kStateCarried) {
        // Relationship vanished out from under us; reconcile to idle.
        state.state = kStateIdle;
        writeObjState(ctx, objective, state);
    }
}

void MIMITA_GAME_CALL holdTick(void* host, std::uint64_t tick, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent)
        return;
    const std::uint64_t hold = ensureObjective(ctx, kKindHold);
    if (hold == 0)
        return;
    ObjectiveStateV1 state{};
    if (!readObjState(ctx, hold, state))
        return;
    const std::uint64_t site = siteOf(ctx, hold);
    float spos[3] = {kSiteX, kSiteY, kSiteZ};
    entityPos(ctx, site, spos);

    // Count actors inside the radius (deterministic sorted order).
    std::uint64_t actors[64] = {0};
    const std::uint32_t n =
        ctx->dynamicEnumerateComponent(ctx->host, kHealthState, actors, 64);
    std::vector<std::uint64_t> ordered(actors, actors + n);
    std::sort(ordered.begin(), ordered.end());
    std::uint32_t inside = 0;
    for (std::uint64_t a : ordered) {
        float p[3] = {0};
        if (!entityPos(ctx, a, p))
            continue;
        const float dx = p[0] - spos[0];
        const float dy = p[1] - spos[1];
        const float dz = p[2] - spos[2];
        if (dx * dx + dy * dy + dz * dz <= kReachRadius * kReachRadius)
            ++inside;
    }
    if (inside == 0)
        return;

    ObjectiveProgressV1 p{};
    if (ctx->dynamicReadComponent)
        ctx->dynamicReadComponent(ctx->host, hold, kObjectiveProgress, &p, sizeof(p));
    if (p.goal <= 0.0f)
        p.goal = 5.0f;
    p.progress += 1.0f;
    if (p.progress >= p.goal) {
        p.progress = p.goal;
        writeProgress(ctx, hold, p);
        state.state = kStateComplete;
        state.ownerTeam = 1;
        writeObjState(ctx, hold, state);
        emitStateChanged(ctx, hold, 0, site, kStateComplete, (std::uint32_t)tick);
        if (ctx->matchFinish)
            ctx->matchFinish(ctx->host, 2 /*team*/, 1 /*blue*/, 0 /*score*/);
        return;
    }
    writeProgress(ctx, hold, p);
}

void MIMITA_GAME_CALL onObjectiveInteract(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* in = event ? static_cast<GameObjectiveInteractV1*>(event->payload) : nullptr;
    if (!ctx || !in || in->objectiveEntity == 0)
        return;
    ObjectiveStateV1 state{};
    if (!readObjState(ctx, in->objectiveEntity, state))
        return;
    if (state.kind != kKindCarry)
        return;  // other behaviors handle their own interactions

    const std::uint64_t carrier = carrierOf(ctx, in->objectiveEntity);
    if (carrier == in->actorEntity) {
        if (ctx->relationshipRemove)
            ctx->relationshipRemove(ctx->host, kCarriedBy, in->objectiveEntity,
                                    in->actorEntity);
        state.state = kStateIdle;
    } else if (carrier == 0) {
        if (ctx->relationshipAdd)
            ctx->relationshipAdd(ctx->host, kCarriedBy, in->objectiveEntity,
                                 in->actorEntity, 0);
        state.state = kStateCarried;
    } else {
        return;  // already carried by someone else
    }
    writeObjState(ctx, in->objectiveEntity, state);
    emitStateChanged(ctx, in->objectiveEntity, in->actorEntity, siteOf(ctx, in->objectiveEntity),
                     state.state, in->tick);
    in->handled = 1;
}

// Observation handler: proves the emit -> generic dispatch round trip.
void MIMITA_GAME_CALL onObjectiveStateChanged(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* in = event ? static_cast<GameObjectiveStateV1*>(event->payload) : nullptr;
    if (!ctx || !in || !ctx->dynamicReadComponent || !ctx->dynamicWriteComponent)
        return;
    ObjectiveEventLogV1 log{};
    ctx->dynamicReadComponent(ctx->host, in->objectiveEntity, kObjectiveEventLog, &log,
                              sizeof(log));
    log.lastState = in->state;
    log.count += 1;
    log.tick = in->tick;
    ctx->dynamicWriteComponent(ctx->host, in->objectiveEntity, kObjectiveEventLog, &log,
                               sizeof(log));
    const_cast<GameObjectiveStateV1*>(in)->handled = 1;
}

} // namespace

const MimitaHotPackage::SchemaRegistrar s_objStateSchema{
    {kObjectiveState, gameHash("ObjectiveState.v1"), sizeof(ObjectiveStateV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "ObjectiveState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_objProgressSchema{
    {kObjectiveProgress, gameHash("ObjectiveProgress.v1"), sizeof(ObjectiveProgressV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "ObjectiveProgress", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_objEventLogSchema{
    {kObjectiveEventLog, gameHash("ObjectiveEventLog.v1"), sizeof(ObjectiveEventLogV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_SERVER_ONLY, "ObjectiveEventLog", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_objOwnershipSchema{
    {kOwnership, gameHash("ObjectiveOwnership.v1"), 4, 4, GAME_COPY_AUTHORING,
     GAME_NET_SERVER_ONLY, "ObjectiveOwnership", 1, 0}};
// Registered as runtime modes so their domains are mode domains: the kernel
// only runs them while the mode is active (runRegisteredDomains skips mode
// domains). Two independent objective behaviors, same primitives.
const MimitaHotPackage::ModeRegistrar s_objCarryMode{
    {kCarryDomain, kCarryDomain, kObjectiveState, gameHash("ObjectiveState.v1"),
     "Objective Carry"}};
const MimitaHotPackage::ModeRegistrar s_objHoldMode{
    {kHoldDomain, kHoldDomain, kObjectiveState, gameHash("ObjectiveState.v1"),
     "Objective Hold"}};
const MimitaHotPackage::SystemRegistrar s_objCarrySystem{
    {gameHash("objective.carry-tick"), kCarryDomain, 0, 0, carryTick, "objective.carry-tick"}};
const MimitaHotPackage::SystemRegistrar s_objHoldSystem{
    {gameHash("objective.hold-tick"), kHoldDomain, 0, 0, holdTick, "objective.hold-tick"}};
const MimitaHotPackage::EventRegistrar s_objInteract{
    {kInteractEvent, 0, kCarryDomain, onObjectiveInteract, "objective.interact"}};
const MimitaHotPackage::EventRegistrar s_objStateChanged{
    {kStateChangedEvent, 0, 0, onObjectiveStateChanged, "objective.state-changed"}};

#endif
