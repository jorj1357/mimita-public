// 09 14 2026
/* purpose
* Live falsification probe for the generic dynamic entity/component lifecycle:
* a component schema unknown when mimita.exe started (BananaComponent), created
* and consumed entirely through generic kernel capabilities with no EXE edit and
* no new enum. A hot system creates a generic entity, attaches the component,
* reads/modifies it, enumerates holders, and emits a package event.
* Does NOT own gameplay state beyond the demonstration entity.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdio>
#include <cstdint>

namespace {

const std::uint64_t kBananaComponent = gameHash("BananaComponent");
const std::uint64_t kBananaChangedEvent = gameHash("banana.component.changed");
const std::uint64_t kBananaRelationship = gameHash("relationship.banana.owner");

struct BananaComponent {
    float ripeness;
    std::uint32_t peeled;
};

struct BananaDemoState {
    std::uint64_t entity = 0;
};

BananaDemoState gBanana;

void MIMITA_GAME_CALL bananaChangedDispatch(void* /*host*/, const GameEventV1* event)
{
    std::printf("[HOT_EVENT] banana.component.changed tick=%llu source=%llu\n",
                event ? (unsigned long long)event->tick : 0ull,
                event ? (unsigned long long)event->sourceEntity : 0ull);
}

void MIMITA_GAME_CALL bananaLifecycleTick(void* host, std::uint64_t tick, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->entityCreate || !ctx->dynamicWriteComponent ||
        !ctx->dynamicReadComponent || !ctx->dynamicEnumerateComponent)
        return;

    BananaComponent current{};
    const bool present =
        gBanana.entity != 0 &&
        ctx->dynamicReadComponent(ctx->host, gBanana.entity, kBananaComponent,
                                  &current, sizeof(current));
    if (!present) {
        std::uint64_t entity = 0;
        if (!ctx->entityCreate(ctx->host,
                               static_cast<std::uint32_t>(0),  // EntityRealm::Server
                               &entity) ||
            entity == 0)
            return;
        gBanana.entity = entity;
        BananaComponent fresh{0.0f, 0u};
        ctx->dynamicWriteComponent(ctx->host, gBanana.entity, kBananaComponent,
                                   &fresh, sizeof(fresh));
        if (ctx->relationshipAdd)
            ctx->relationshipAdd(ctx->host, kBananaRelationship, gBanana.entity,
                                 gBanana.entity, 1u);
        std::printf("[HOT_SYSTEM] banana.lifecycle created entity=%llu component=BananaComponent\n",
                    (unsigned long long)gBanana.entity);
    }

    if (tick % 120 != 0)
        return;

    if (!ctx->dynamicReadComponent(ctx->host, gBanana.entity, kBananaComponent,
                                   &current, sizeof(current)))
        return;
    current.ripeness = current.ripeness >= 1.0f ? 1.0f : current.ripeness + 0.1f;
    current.peeled = current.ripeness >= 0.5f ? 1u : 0u;
    ctx->dynamicWriteComponent(ctx->host, gBanana.entity, kBananaComponent,
                               &current, sizeof(current));

    std::uint64_t holders[8]{};
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, kBananaComponent, holders, 8);
    std::printf("[HOT_SYSTEM] banana.lifecycle entity=%llu ripeness=%.2f peeled=%u holders=%u\n",
                (unsigned long long)gBanana.entity, current.ripeness,
                (unsigned)current.peeled, (unsigned)count);

    if (ctx->emitEvent) {
        GameEventV1 event{};
        event.typeId = kBananaChangedEvent;
        event.schemaHash = gameHash("banana.component.changed.v1");
        event.payloadVersion = 1;
        event.tick = tick;
        event.sourceEntity = gBanana.entity;
        using EmitFn = void (MIMITA_GAME_CALL *)(GameplayContextV1*, const GameEventV1*);
        reinterpret_cast<EmitFn>(ctx->emitEvent)(ctx, &event);
    }
}

void MIMITA_GAME_CALL bananaCommand(void* host, const char* /*args*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicComponentInfo) {
        std::printf("[BANANA] lifecycle capabilities unavailable\n");
        return;
    }
    GameDynamicComponentInfoV1 info{};
    if (ctx->dynamicComponentInfo(ctx->host, kBananaComponent, &info)) {
        std::printf("[BANANA] schema=%s version=%u size=%u copyPolicy=%u entity=%llu\n",
                    info.name, (unsigned)info.version, (unsigned)info.size,
                    (unsigned)info.copyPolicy, (unsigned long long)gBanana.entity);
    } else {
        std::printf("[BANANA] schema not registered\n");
    }
}

const MimitaHotPackage::SystemRegistrar s_bananaLifecycle{
    {gameHash("banana.lifecycle"), GAME_DOMAIN_GAMEPLAY, 400, 0,
     bananaLifecycleTick, "banana.lifecycle"}};
const MimitaHotPackage::EventRegistrar s_bananaChanged{
    {kBananaChangedEvent, gameHash("banana.component.changed.v1"), 0,
     bananaChangedDispatch, "banana.component.changed"}};
const MimitaHotPackage::SchemaRegistrar s_bananaComponent{
    {kBananaComponent, gameHash("BananaComponent.v1"), sizeof(BananaComponent), 4,
     GAME_COPY_AUTHORING, 0, "BananaComponent", 1, 0}};
const MimitaHotPackage::CommandRegistrar s_bananaCommand{
    {"banana.component", "banana.component - print the hot dynamic component state",
     0, bananaCommand}};

} // namespace

#endif
