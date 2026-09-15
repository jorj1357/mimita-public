// 09 15 2026
/* purpose
* Hot actor overlays (nameplates / health bars). A `ui.frame` system enumerates
* generic actor entities (PresentationState + health + optional identity) and
* composes overlays through the generic `world.project` + `render.ui`
* capabilities. Hot owns text/health formatting/color/distance/size/visibility;
* the kernel owns world->screen projection and final UI draw. No typed player or
* npc pointer is used.
* Ownership gating: the cold healthbar/nameplate path is still the live owner
* until per-actor ownership + remote identity coverage is wired, so this system
* is off by default and toggled with `hotoverlays 1|0` (avoids duplicate owner).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const std::uint64_t kIdentityState = gameHash("ActorIdentityState");
const std::uint64_t kTeamState = gameHash("ActorTeamState");

struct ActorIdentityStateV1 {
    char name[32];
    std::uint32_t reserved;
};
struct ActorTeamStateV1 {
    std::int32_t team;
    std::uint32_t reserved;
};

using RenderUiFn = void (MIMITA_GAME_CALL *)(void*, const GameUiCommandV1*);
using WorldProjectFn = bool (MIMITA_GAME_CALL *)(void*, GameWorldProjectV1*);

bool g_overlaysEnabled = true;   // real shipping owner; per-actor cold yield

void MIMITA_GAME_CALL hotOverlaysCommand(void* /*host*/, const char* args)
{
    g_overlaysEnabled = args && args[0] == '1';
    std::printf("[OVERLAY] actor overlays = %d\n", (int)g_overlaysEnabled);
}

void emitText(RenderUiFn ui, void* host, const char* text, float x, float y,
              float scale, float r, float g, float b, float a)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_TEXT;
    c.x = x;
    c.y = y;
    c.scale = scale;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    std::snprintf(c.text, sizeof(c.text), "%s", text);
    ui(host, &c);
}

void emitBar(RenderUiFn ui, void* host, float x, float y, float w, float h,
             float value, float r, float g, float b)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_BAR;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.value = value;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = 1.0f;
    ui(host, &c);
}

void MIMITA_GAME_CALL actorOverlayTick(void* host, std::uint64_t /*tick*/,
                                       float /*dt*/)
{
    if (!g_overlaysEnabled)
        return;
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->readComponent || !ctx->resolveCapability)
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    auto project = reinterpret_cast<WorldProjectFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_WORLD_PROJECT));
    if (!ui || !project)
        return;

    // One generic enumeration: any actor-like entity carrying a generic
    // identity is a candidate. No per-player/per-NPC loop.
    std::uint64_t entities[128];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, kIdentityState, entities, 128);
    for (std::uint32_t i = 0; i < count; ++i) {
        ActorIdentityStateV1 id{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i], kIdentityState,
                                       &id, sizeof(id)) ||
            id.name[0] == '\0')
            continue;
        GameTransformComponentV1 tf{};
        if (!ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_TRANSFORM,
                                &tf, sizeof(tf)))
            continue;
        GameHealthComponentV1 hp{};
        const bool haveHp = ctx->readComponent(
            ctx->host, entities[i], GAME_COMPONENT_HEALTH, &hp, sizeof(hp));
        if (haveHp && hp.dead)
            continue;   // dead actors: hot policy hides the overlay

        GameWorldProjectV1 proj{};
        proj.worldPosition[0] = tf.position[0];
        proj.worldPosition[1] = tf.position[1];
        proj.worldPosition[2] = tf.position[2] + 1.9f;  // above the head
        if (!project(ctx->host, &proj) || !proj.visible || proj.depth > 60.0f)
            continue;

        // Team colour (generic team state), red default / blue team 1.
        float br = 0.9f, bg = 0.2f, bb = 0.2f;
        ActorTeamStateV1 team{};
        if (ctx->dynamicReadComponent(ctx->host, entities[i], kTeamState, &team,
                                      sizeof(team)) &&
            team.team == 1) {
            br = 0.30f; bg = 0.50f; bb = 1.0f;
        }

        if (haveHp) {
            const float frac = hp.max > 0
                                   ? (float)hp.current / (float)hp.max
                                   : 0.0f;
            const float f = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);
            emitBar(ui, ctx->host, proj.screenX - 30.0f, proj.screenY, 60.0f,
                    6.0f, f, br, bg, bb);
            char hpText[64];
            std::snprintf(hpText, sizeof(hpText), "%d", hp.current);
            emitText(ui, ctx->host, hpText, proj.screenX + 34.0f,
                     proj.screenY - 6.0f, 0.4f, 1.0f, 1.0f, 1.0f, 0.9f);
        }
        emitText(ui, ctx->host, id.name, proj.screenX - 40.0f,
                 proj.screenY - 26.0f, 0.45f, 1.0f, 1.0f, 1.0f, 0.95f);

        // Per-actor ownership claim: the cold nameplate/healthbar path yields
        // for this actor only.
        HotOverlayClaimV1 claim{};
        claim.owned = 1;
        ctx->dynamicWriteComponent(ctx->host, entities[i],
                                   HOT_OVERLAY_CLAIM_COMPONENT, &claim,
                                   sizeof(claim));
    }
}

const MimitaHotPackage::SchemaRegistrar s_overlayClaimSchema{
    {HOT_OVERLAY_CLAIM_COMPONENT, gameHash("ActorOverlayClaim.v1"),
     sizeof(HotOverlayClaimV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ActorOverlayClaim", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_actorOverlaySystem{
    {gameHash("hot.actor-overlays"), GAME_DOMAIN_UI, 15, 0, actorOverlayTick,
     "hot.actor-overlays"}};
const MimitaHotPackage::CommandRegistrar s_hotOverlaysCommand{
    {"hotoverlays", "hotoverlays 1|0 - enable hot actor overlays", 0,
     hotOverlaysCommand}};

} // namespace

#endif
