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

#include "gui/ui-system.h"
#include "project/presentation-resource.h"

namespace LiveUi {
namespace {

constexpr std::size_t kMaxCommands = 256;

std::vector<GameUiCommandV1> g_commands;
bool g_hotOwnsHud = false;

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
}

void submit(const GameUiCommandV1& command)
{
    if (g_commands.size() >= kMaxCommands)
        return;
    g_commands.push_back(command);
}

void endFrameAndDraw()
{
    for (const GameUiCommandV1& c : g_commands) {
        switch (c.kind) {
        case GAME_UI_TEXT: drawText(c); break;
        case GAME_UI_PANEL: drawPanel(c); break;
        case GAME_UI_BAR: drawBar(c); break;
        case GAME_UI_IMAGE: drawImage(c); break;
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
