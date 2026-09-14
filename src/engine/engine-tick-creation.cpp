// 09 13 2026
/* purpose
* Implements the creation-mode tick hook and overlay.
* Does NOT own selection state or raycasting.
*/
#include "engine/engine-tick-creation.h"

#include <string>

#include "camera.h"
#include "editor/creation-mode.h"
#include "gui/ui-system.h"
#include "live-code/live-editor.h"
#include "world/world.h"

void engineTickCreationUpdate(const World& world, const Camera& camera)
{
    Editor::CreationMode::instance().updateTick(world, camera.pos, camera.front);
}

void engineRenderCreationOverlay(const Camera& camera)
{
    (void)camera;
    Editor::CreationMode& mode = Editor::CreationMode::instance();
    if (!mode.enabled())
        return;

    // The hot editor module owns the overlay layout when present.
    if (LiveEditor::drawOverlay())
        return;

    float y = 120.0f;
    uiDrawText("[CREATE MODE] looking at:", 24.0f, y, 0.4f, {0.4f, 1.0f, 0.6f, 1.0f});
    y += 20.0f;

    const std::string& text = mode.overlayText();
    std::string line;
    for (char ch : text) {
        if (ch == '\n') {
            uiDrawText(line.c_str(), 24.0f, y, 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});
            y += 16.0f;
            line.clear();
        } else {
            line.push_back(ch);
        }
    }
    if (!line.empty())
        uiDrawText(line.c_str(), 24.0f, y, 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});
}
