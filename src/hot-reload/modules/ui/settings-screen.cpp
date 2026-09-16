// 09 15 2026
/* purpose
* Hot settings screen. Composes controls through render.ui and reads/writes real
* engine settings through the generic `setting.get`/`setting.set` seam. Hot owns
* which settings appear, labels, ranges, widget types, navigation and action
* meaning; the kernel owns applying values + validation. No SettingsManager*.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-ui.h"

#include <cstdint>
#include <cstdio>

namespace {

const std::uint64_t kScreenSettings = gameHash("screen.settings");
const std::uint64_t kScreenMain = gameHash("screen.main-menu");
const std::uint64_t kMenuBack = gameHash("menu.back");

using RenderUiFn = void (MIMITA_GAME_CALL *)(void*, const GameUiCommandV1*);
using SettingGetFn = bool (MIMITA_GAME_CALL *)(void*, GameSettingV1*);
using SettingSetFn = bool (MIMITA_GAME_CALL *)(void*, GameSettingV1*);

struct NavStateV1 {
    std::uint64_t screenId;
    std::uint64_t previousScreenId;
    std::uint64_t modalId;
    std::uint64_t focusId;
    std::uint32_t flags;
    std::uint32_t reserved;
};

struct SettingRow {
    std::uint64_t id;
    const char* label;
    std::uint32_t type;   // GAME_SETTING_*
    float minValue;
    float maxValue;
    float step;
};
const SettingRow kRows[] = {
    {gameHash("video.fov"), "Field of View", GAME_SETTING_FLOAT, 60.0f, 140.0f, 1.0f},
    {gameHash("audio.master"), "Master Volume", GAME_SETTING_FLOAT, 0.0f, 1.0f, 0.05f},
    {gameHash("audio.music"), "Music Volume", GAME_SETTING_FLOAT, 0.0f, 1.0f, 0.05f},
    {gameHash("audio.sfx"), "SFX Volume", GAME_SETTING_FLOAT, 0.0f, 1.0f, 0.05f},
    {gameHash("input.sensitivity"), "Sensitivity", GAME_SETTING_FLOAT, 0.01f, 1.0f, 0.01f},
    {gameHash("audio.muted"), "Mute Music", GAME_SETTING_BOOL, 0.0f, 1.0f, 1.0f},
    {gameHash("video.resolution"), "Resolution", GAME_SETTING_OPTION, 0.0f, 0.0f, 0.0f},
    {gameHash("video.graphicsPreset"), "Graphics", GAME_SETTING_OPTION, 0.0f, 0.0f, 0.0f},
};

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

std::uint64_t readScreen(GameplayContextV1* ctx)
{
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
    if (e == 0 || !ctx->dynamicReadComponent)
        return 0;
    NavStateV1 nav{};
    if (!ctx->dynamicReadComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                                   sizeof(nav)))
        return 0;
    return nav.screenId;
}

void writeScreen(GameplayContextV1* ctx, std::uint64_t screenId)
{
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
    if (e == 0 || !ctx->dynamicWriteComponent)
        return;
    NavStateV1 nav{};
    if (ctx->dynamicReadComponent)
        ctx->dynamicReadComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                                  sizeof(nav));
    nav.previousScreenId = nav.screenId;
    nav.screenId = screenId;
    ctx->dynamicWriteComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                               sizeof(nav));
}

void emitText(RenderUiFn ui, void* host, const char* text, float x, float y,
              float scale, float r, float g, float b)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_TEXT;
    c.x = x; c.y = y; c.scale = scale;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = 1.0f;
    std::snprintf(c.text, sizeof(c.text), "%s", text);
    ui(host, &c);
}

void MIMITA_GAME_CALL settingsTick(void* host, std::uint64_t /*tick*/,
                                   float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability ||
        !ctx->dynamicReadComponent || !ctx->dynamicWriteComponent)
        return;
    if (readScreen(ctx) != kScreenSettings)
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    auto get = reinterpret_cast<SettingGetFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SETTING_GET));
    if (!ui || !get)
        return;

    GameUiCommandV1 bg{};
    bg.kind = GAME_UI_PANEL;
    bg.x = 0.0f; bg.y = 0.0f; bg.w = 1280.0f; bg.h = 720.0f;
    bg.color[0] = 0.05f; bg.color[1] = 0.06f; bg.color[2] = 0.09f; bg.color[3] = 1.0f;
    ui(ctx->host, &bg);
    emitText(ui, ctx->host, "SETTINGS", 520.0f, 40.0f, 0.9f, 1.0f, 1.0f, 1.0f);

    float y = 130.0f;
    for (const SettingRow& row : kRows) {
        GameSettingV1 s{};
        s.settingId = row.id;
        s.type = row.type;
        if (!get(ctx->host, &s) || !s.ok) {
            y += 60.0f;
            continue;
        }
        GameUiCommandV1 c{};
        c.elementId = row.id;
        c.x = 360.0f; c.y = y; c.w = 420.0f; c.h = 16.0f;
        c.scale = 0.32f;
        c.color[0] = 0.25f; c.color[1] = 0.6f; c.color[2] = 0.95f;
        c.color[3] = 1.0f;
        std::snprintf(c.text, sizeof(c.text), "%s", row.label);
        if (row.type == GAME_SETTING_BOOL) {
            c.kind = GAME_UI_TOGGLE;
            c.value = (float)s.intValue;
        } else if (row.type == GAME_SETTING_OPTION) {
            c.kind = GAME_UI_SELECT;
            c.value = (float)s.intValue;
            c.maxValue = (float)(s.optionCount > 0 ? s.optionCount - 1 : 0);
            std::snprintf(c.text, sizeof(c.text), "%s", s.optionLabel);
        } else {
            c.kind = GAME_UI_SLIDER;
            c.value = s.floatValue;
            c.minValue = row.minValue;
            c.maxValue = row.maxValue;
            c.step = row.step;
        }
        ui(ctx->host, &c);
        y += 60.0f;
    }

    GameUiCommandV1 back{};
    back.kind = GAME_UI_BUTTON;
    back.elementId = kMenuBack;
    back.x = 40.0f; back.y = 620.0f; back.w = 160.0f; back.h = 44.0f;
    back.scale = 0.4f;
    back.color[0] = 0.5f; back.color[1] = 0.3f; back.color[2] = 0.3f; back.color[3] = 1.0f;
    std::snprintf(back.text, sizeof(back.text), "BACK");
    ui(ctx->host, &back);

    // Ownership claim for screen.settings (cold settings yields).
    {
        GameSharedStateV1* shared = sharedState(ctx);
        const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
        if (e != 0) {
            HotUiClaimV1 claim{};
            claim.screenId = kScreenSettings;
            claim.owned = 1;
            ctx->dynamicWriteComponent(ctx->host, e, HOT_UI_CLAIM_COMPONENT,
                                       &claim, sizeof(claim));
        }
    }
}

const MimitaHotPackage::SystemRegistrar s_settingsSystem{
    {gameHash("hot.settings-screen"), GAME_DOMAIN_UI, 13, 0, settingsTick,
     "hot.settings-screen"}};

} // namespace

#endif
