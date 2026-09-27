#include "hot-reload/game-api.h"

#include <algorithm>

void MIMITA_GAME_CALL gameUpdateQuickHitDebugVisual(
    GameMemory* memory,
    GameQuickHitDebugVisualState* visual)
{
    if (!memory || !visual || memory->apiVersion != MIMITA_GAME_API_VERSION)
        return;

    visual->enabled = visual->enabled ? 1 : 0;
    visual->wireframe = 0;
    visual->color[0] = std::clamp(visual->color[0], 0.0f, 1.0f);
    visual->color[1] = std::clamp(visual->color[1], 0.0f, 1.0f);
    visual->color[2] = std::clamp(visual->color[2], 0.0f, 1.0f);
    visual->color[3] = std::clamp(visual->color[3], 0.15f, 1.0f);
    visual->radius = std::max(0.01f, visual->radius);
}
