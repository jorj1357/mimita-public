// 09 14 2026
/* purpose
* Hot match HUD composition. A `ui.frame` system reads the generic replicated
* MatchHudState (timer/scores/phase) and emits generic UI commands (text/panel/
* bar). The kernel draws primitives; this file owns the HUD's layout, labels,
* ordering, and formatting. No FfaHud/TdmHud/CounterStrikeHud type; editing this
* file and saving changes the running client's HUD.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-ui.h"

#include <cstdio>
#include <cstdint>

namespace {

using RenderUiFn = void (MIMITA_GAME_CALL *)(void*, const GameUiCommandV1*);

RenderUiFn resolveRenderUi(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->resolveCapability)
        return nullptr;
    return reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
}

void submitText(RenderUiFn render, void* host, const char* text, float x, float y,
                float scale, float r, float g, float b, float a)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_TEXT;
    c.x = x;
    c.y = y;
    c.scale = scale;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    std::snprintf(c.text, sizeof(c.text), "%s", text);
    render(host, &c);
}

void submitPanel(RenderUiFn render, void* host, float x, float y, float w, float h,
                 float r, float g, float b, float a)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_PANEL;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    render(host, &c);
}

void submitBar(RenderUiFn render, void* host, float x, float y, float w, float h,
               float value, float r, float g, float b, float a)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_BAR;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.value = value;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    render(host, &c);
}

// ui.frame system: owned by this hot module.
void MIMITA_GAME_CALL matchHudTick(void* host, std::uint64_t /*tick*/, float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicReadComponent || !ctx->dynamicEnumerateComponent)
        return;
    RenderUiFn render = resolveRenderUi(ctx);
    if (!render)
        return;

    // Find the generic match HUD state (data-driven; no match-entity plumbing).
    std::uint64_t owners[4] = {0};
    const std::uint32_t ownerCount = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_MATCH_HUD_COMPONENT, owners, 4);
    if (ownerCount == 0)
        return;  // fails safe: no state -> emit nothing; cold fallback owns HUD
    HotMatchHudStateV1 hud{};
    if (!ctx->dynamicReadComponent(ctx->host, owners[0], HOT_MATCH_HUD_COMPONENT,
                                   &hud, sizeof(hud)))
        return;

    // One owner: only compose when the generic claim says hot covers this mode's
    // HUD. Otherwise the cold client HUD remains the owner.
    HotModeHudClaimV1 claim{};
    if (!ctx->dynamicReadComponent(ctx->host, owners[0],
                                   HOT_MODE_HUD_CLAIM_COMPONENT, &claim,
                                   sizeof(claim)) ||
        claim.owned != 1)
        return;

    // Timer (top center).
    char timer[32];
    const int total = hud.timerSeconds > 0.0f ? (int)(hud.timerSeconds + 0.5f) : 0;
    std::snprintf(timer, sizeof(timer), "%d:%02d", total / 60, total % 60);
    submitPanel(render, ctx->host, 540.0f, 12.0f, 200.0f, 44.0f, 0.0f, 0.0f, 0.0f, 0.55f);
    submitText(render, ctx->host, timer, 600.0f, 20.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f);
    if (hud.phaseText[0] != '\0')
        submitText(render, ctx->host, hud.phaseText, 560.0f, 62.0f, 0.28f, 0.9f, 0.9f, 0.6f, 1.0f);

    // Generic objective line (bomb/plant/defuse/capture). Hot interprets the
    // state hash; the kernel knows no objective kind.
    HotObjectiveStateV1 obj{};
    if (ctx->dynamicReadComponent(ctx->host, owners[0], HOT_OBJECTIVE_COMPONENT,
                                  &obj, sizeof(obj)) &&
        obj.flags != 0) {
        const char* state = obj.stateHash == gameHash("bomb.defusing") ? "DEFUSING"
                            : obj.stateHash == gameHash("bomb.planted") ? "PLANTED"
                            : obj.stateHash == gameHash("bomb.carried") ? "CARRIED"
                                                                       : "OBJECTIVE";
        char line[64];
        std::snprintf(line, sizeof(line), "%s %d%% %.1fs", state,
                      (int)(obj.progress * 100.0f + 0.5f), obj.timer);
        submitText(render, ctx->host, line, 560.0f, 92.0f, 0.34f, 1.0f, 0.8f,
                   0.3f, 1.0f);
        submitBar(render, ctx->host, 560.0f, 112.0f, 160.0f, 6.0f, obj.progress,
                  0.9f, 0.6f, 0.2f, 1.0f);
    }

    // Score panel (top left).
    submitPanel(render, ctx->host, 12.0f, 12.0f, 220.0f, 72.0f, 0.0f, 0.0f, 0.0f, 0.45f);
    char lineA[64];
    char lineB[64];
    std::snprintf(lineA, sizeof(lineA), "%s   %d", hud.labelA[0] ? hud.labelA : "RED", hud.scoreA);
    std::snprintf(lineB, sizeof(lineB), "%s   %d", hud.labelB[0] ? hud.labelB : "BLUE", hud.scoreB);
    submitText(render, ctx->host, lineA, 24.0f, 22.0f, 0.32f, 1.0f, 0.4f, 0.4f, 1.0f);
    submitText(render, ctx->host, lineB, 24.0f, 50.0f, 0.32f, 0.4f, 0.6f, 1.0f, 1.0f);

    // Round progress bar (deterministic ordering after the panels).
    submitBar(render, ctx->host, 540.0f, 58.0f, 200.0f, 6.0f,
              hud.timerSeconds / 120.0f, 0.9f, 0.8f, 0.2f, 1.0f);
}

const MimitaHotPackage::SchemaRegistrar s_matchHudSchema{
    {HOT_MATCH_HUD_COMPONENT, gameHash("MatchHudState.v1"),
     sizeof(HotMatchHudStateV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "MatchHudState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_objectiveSchema{
    {HOT_OBJECTIVE_COMPONENT, gameHash("ObjectivePresentationState.v1"),
     sizeof(HotObjectiveStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ObjectivePresentationState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_modeHudClaimSchema{
    {HOT_MODE_HUD_CLAIM_COMPONENT, gameHash("ModeHudClaim.v1"),
     sizeof(HotModeHudClaimV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ModeHudClaim", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_matchHudSystem{
    {gameHash("hot.match-hud"), GAME_DOMAIN_UI, 10, 0, matchHudTick,
     "hot.match-hud"}};

} // namespace

#endif
