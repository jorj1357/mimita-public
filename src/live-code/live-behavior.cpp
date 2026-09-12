// 09 12 2026
/* purpose
* Implements the generic behavior event bridge and the kernel event queue used
* by the emitEvent capability.
* Does NOT own gameplay policy or state.
*/
#include "live-code/live-behavior.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"

#include <cstring>

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

void MIMITA_GAME_CALL kernelEmitEvent(GameplayContextV1*, const GameEventV1* event);

GameplayContextV1 makeContext(std::uint64_t tick)
{
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    GameplayContextV1 context{};
    context.host = nullptr;
    context.tick = tick;
    context.generation = status.activeGeneration;
    context.codeHash = 0;
    context.emitEvent = reinterpret_cast<void*>(&kernelEmitEvent);
    context.findEntities = nullptr;
    return context;
}

} // namespace

namespace LiveBehavior {

bool available()
{
    const GameGameplayModuleV1* module = gameplayModule();
    return module && module->onEvent != nullptr;
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
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return false;
    GameplayContextV1 context = makeContext(tick);
    module->onEvent(&event, &context);
    return true;
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
