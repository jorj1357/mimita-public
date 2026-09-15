// 09 14 2026
/* purpose
* Implements the kernel-side generic HUD/UI command buffer and its draw through
* the immediate-mode UI backend. Hot code owns composition; this file only draws
* primitives. Does NOT own gameplay state or font rasterization.
*/
#include "live-code/live-ui.h"

#include <algorithm>
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
struct ButtonHit {
    std::uint64_t elementId;
    float x, y, w, h;
};
std::vector<ButtonHit> g_buttons;

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
    if (command.kind == GAME_UI_BUTTON && command.elementId != 0)
        g_buttons.push_back({command.elementId, command.x, command.y, command.w,
                             command.h});
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
        action.actionType = GAME_UI_ACTION_CLICK;
        action.pointerX = x;
        action.pointerY = y;
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
