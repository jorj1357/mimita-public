// 09 14 2026
/* purpose
* Implements the kernel-side generic HUD/UI command buffer and its draw through
* the immediate-mode UI backend. Hot code owns composition; this file only draws
* primitives. Does NOT own gameplay state or font rasterization.
*/
#include "live-code/live-ui.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include <cstdint>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-types.h"
#include "gui/ui-system.h"
#include "hot-reload/hot-ui.h"
#include "live-code/live-behavior.h"
#include "project/presentation-resource.h"

namespace LiveUi {
namespace {

constexpr std::size_t kMaxCommands = 256;

std::vector<GameUiCommandV1> g_commands;
bool g_hotOwnsHud = false;

// Interactive widgets from the last completed frame: logical ids + rects only
// (never hot function pointers), so this is generation-safe.
struct WidgetHit {
    std::uint64_t elementId;
    std::uint32_t kind;
    float x, y, w, h;
    float value, minValue, maxValue, step;
};
std::vector<WidgetHit> g_buttons;

glm::vec4 colorOf(const GameUiCommandV1& c)
{
    return glm::vec4(c.color[0], c.color[1], c.color[2], c.color[3]);
}

void drawText(const GameUiCommandV1& c)
{
    if (c.text[0] == '\0')
        return;
    uiDrawText(c.text, c.x, c.y, c.scale > 0.0f ? c.scale : 0.3f, colorOf(c));
}

void drawPanel(const GameUiCommandV1& c)
{
    UIRect r{c.x, c.y, c.w, c.h};
    uiDrawRect(r, colorOf(c), "hotui.panel");
}

void drawBar(const GameUiCommandV1& c)
{
    const float frac = std::clamp(c.value, 0.0f, 1.0f);
    UIRect bg{c.x, c.y, c.w, c.h};
    glm::vec4 bgColor = colorOf(c);
    bgColor.a *= 0.35f;
    uiDrawRect(bg, bgColor, "hotui.bar.bg");
    UIRect fill{c.x, c.y, c.w * frac, c.h};
    uiDrawRect(fill, colorOf(c), "hotui.bar.fill");
}

void drawButton(const GameUiCommandV1& c)
{
    UIRect r{c.x, c.y, c.w, c.h};
    uiDrawRect(r, colorOf(c), "hotui.button");
    uiDrawRectOutline(r, {1.0f, 1.0f, 1.0f, 0.85f}, "hotui.button.border");
    if (c.text[0] != '\0') {
        const float scale = c.scale > 0.0f ? c.scale : 0.4f;
        const float tw = uiMeasureText(c.text, scale);
        uiDrawText(c.text, c.x + (c.w - tw) * 0.5f, c.y + c.h * 0.5f - 10.0f,
                   scale, {1.0f, 1.0f, 1.0f, 1.0f});
    }
}

void drawSlider(const GameUiCommandV1& c)
{
    UIRect track{c.x, c.y, c.w, c.h};
    uiDrawRect(track, {0.12f, 0.14f, 0.18f, 1.0f}, "hotui.slider.track");
    const float span = (c.maxValue - c.minValue) != 0.0f
                           ? (c.maxValue - c.minValue) : 1.0f;
    float frac = (c.value - c.minValue) / span;
    frac = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);
    uiDrawRect({c.x, c.y, c.w * frac, c.h}, colorOf(c), "hotui.slider.fill");
    if (c.text[0] != '\0') {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s %.2f", c.text, c.value);
        uiDrawText(buf, c.x, c.y - 26.0f, c.scale > 0.0f ? c.scale : 0.34f,
                   {0.9f, 0.9f, 0.9f, 1.0f});
    }
}

void drawToggle(const GameUiCommandV1& c)
{
    const bool on = c.value > 0.5f;
    UIRect r{c.x, c.y, c.h, c.h};
    uiDrawRect(r, on ? glm::vec4(0.2f, 0.7f, 0.3f, 1.0f)
                     : glm::vec4(0.35f, 0.35f, 0.35f, 1.0f),
               "hotui.toggle");
    if (c.text[0] != '\0')
        uiDrawText(c.text, c.x + c.h + 10.0f, c.y, 0.34f,
                   {0.9f, 0.9f, 0.9f, 1.0f});
}

void drawSelect(const GameUiCommandV1& c)
{
    UIRect r{c.x, c.y, c.w, c.h};
    uiDrawRect(r, colorOf(c), "hotui.select");
    uiDrawRectOutline(r, {1.0f, 1.0f, 1.0f, 0.7f}, "hotui.select.border");
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s >", c.text[0] ? c.text : "?");
    uiDrawText(buf, c.x + 8.0f, c.y + c.h * 0.5f - 8.0f,
               c.scale > 0.0f ? c.scale : 0.34f, {1.0f, 1.0f, 1.0f, 1.0f});
}

void drawImage(const GameUiCommandV1& c)
{
    // Prefer a generation-aware logical resource: the provider resolves the
    // current texture handle; hot code never holds a GPU handle.
    if (c.resourceId != 0) {
        void* handle = MimitaRuntime::PresentationResourceProvider::instance()
                           .handleOf(c.resourceId);
        if (handle) {
            uiDrawTexture(static_cast<unsigned int>(
                              reinterpret_cast<std::uintptr_t>(handle)),
                          UIRect{c.x, c.y, c.w, c.h}, colorOf(c));
            return;
        }
    }
    if (c.text[0] == '\0')
        return;
    uiDrawImage(c.text, UIRect{c.x, c.y, c.w, c.h}, colorOf(c));
}

} // namespace

void beginFrame()
{
    g_commands.clear();
    g_buttons.clear();
    // Generation-safe claim: clear any hot screen claim at frame start so the
    // active generation must re-assert ownership this frame. If a new generation
    // does not claim (removed/renamed screen), the cold owner recovers instead of
    // a blank UI. The nav state itself persists in its own component.
    const EntityId entity = Ecs::ensureLocalPlayerEntity();
    if (entity != kInvalidEntityId)
        MimitaRuntime::DynamicComponentStore::instance().remove(
            entity, HOT_UI_CLAIM_COMPONENT);
}

void submit(const GameUiCommandV1& command)
{
    if (g_commands.size() >= kMaxCommands)
        return;
    if ((command.kind == GAME_UI_BUTTON || command.kind == GAME_UI_SLIDER ||
         command.kind == GAME_UI_TOGGLE || command.kind == GAME_UI_SELECT) &&
        command.elementId != 0)
        g_buttons.push_back({command.elementId, command.kind, command.x,
                             command.y, command.w, command.h, command.value,
                             command.minValue, command.maxValue, command.step});
    g_commands.push_back(command);
}

bool handlePointerClick(float x, float y, std::uint64_t tick)
{
    // Topmost widget wins (later commands draw on top).
    for (auto it = g_buttons.rbegin(); it != g_buttons.rend(); ++it) {
        if (x < it->x || y < it->y || x > it->x + it->w || y > it->y + it->h)
            continue;
        GameUiActionV1 action{};
        action.elementId = it->elementId;
        action.pointerX = x;
        action.pointerY = y;
        if (it->kind == GAME_UI_SLIDER) {
            const float span = (it->maxValue - it->minValue) != 0.0f
                                   ? (it->maxValue - it->minValue) : 1.0f;
            float v = it->minValue +
                      ((x - it->x) / (it->w != 0.0f ? it->w : 1.0f)) * span;
            if (it->step > 0.0f)
                v = it->minValue +
                    std::round((v - it->minValue) / it->step) * it->step;
            v = std::clamp(v, it->minValue, it->maxValue);
            action.actionType = GAME_UI_ACTION_VALUE_CHANGED;
            action.value = v;
        } else if (it->kind == GAME_UI_TOGGLE) {
            action.actionType = GAME_UI_ACTION_VALUE_CHANGED;
            action.value = it->value > 0.5f ? 0.0f : 1.0f;
        } else if (it->kind == GAME_UI_SELECT) {
            const int count = (int)it->maxValue + 1;
            const int next = count > 0 ? ((int)it->value + 1) % count : 0;
            action.actionType = GAME_UI_ACTION_VALUE_CHANGED;
            action.value = (float)next;   // option index
        } else {
            action.actionType = GAME_UI_ACTION_CLICK;
        }
        LiveBehavior::dispatchGameplayEvent64(gameHash("ui.action"), &action,
                                              sizeof(action), tick);
        return action.handled != 0;
    }
    return false;
}

std::size_t buttonCount()
{
    return g_buttons.size();
}

bool hotOwnsScreen(std::uint64_t screenId)
{
    const EntityId entity = Ecs::ensureLocalPlayerEntity();
    if (entity == kInvalidEntityId)
        return false;
    HotUiClaimV1 claim{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            entity, HOT_UI_CLAIM_COMPONENT, &claim, sizeof(claim)))
        return false;
    if (claim.owned != 1)
        return false;
    return screenId == 0 || claim.screenId == screenId;
}

void endFrameAndDraw()
{
    for (const GameUiCommandV1& c : g_commands) {
        switch (c.kind) {
        case GAME_UI_TEXT: drawText(c); break;
        case GAME_UI_PANEL: drawPanel(c); break;
        case GAME_UI_BAR: drawBar(c); break;
        case GAME_UI_IMAGE: drawImage(c); break;
        case GAME_UI_BUTTON: drawButton(c); break;
        case GAME_UI_SLIDER: drawSlider(c); break;
        case GAME_UI_TOGGLE: drawToggle(c); break;
        case GAME_UI_SELECT: drawSelect(c); break;
        default: break;
        }
    }
    g_hotOwnsHud = !g_commands.empty();
    g_commands.clear();
}

std::size_t commandCount()
{
    return g_commands.size();
}

bool hotOwnsHud()
{
    return g_hotOwnsHud;
}

std::uint64_t resolveUiImageHandle(std::uint64_t resourceId)
{
    if (resourceId == 0)
        return 0;
    void* handle = MimitaRuntime::PresentationResourceProvider::instance().handleOf(
        resourceId);
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

} // namespace LiveUi
