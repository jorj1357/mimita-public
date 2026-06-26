#include "avatar-picker.h"
#include "character-registry.h"

#include <cstdio>
#include <string>
#include <vector>

#include "gui/ui-system.h"
#include "gui/gui-coord.h"

void drawAvatarPicker(AvatarPickerResult& result)
{
    result.changed = false;

    const CharacterRegistry& registry = CharacterRegistry::instance();
    const auto& characters = registry.all();

    if (characters.empty())
    {
        uiDrawText("No characters found in Characters/", 50, 50, 0.3f, {1, 1, 1, 1});
        return;
    }

    float x = 50.0f;
    float y = 100.0f;
    float cardW = 160.0f;
    float cardH = 200.0f;
    float gap = 16.0f;

    uiDrawText("SELECT CHARACTER", 50, 60, 0.35f, {0.3f, 1.0f, 0.5f, 1});

    int col = 0;
    int row = 0;

    for (const CharacterManifest& manifest : characters)
    {
        if (manifest.hidden)
            continue;

        float cx = x + col * (cardW + gap);
        float cy = y + row * (cardH + gap);

        UIRect cardRect = {cx, cy, cardW, cardH};
        uiDrawRect(cardRect, {0.15f, 0.15f, 0.2f, 0.9f}, "char-card");

        UIRect previewRect = {cx + 8, cy + 8, cardW - 16, cardH - 48};
        uiDrawRect(previewRect, {0.1f, 0.1f, 0.15f, 1.0f}, "char-preview");

        uiDrawText(manifest.name.c_str(), cx + 8, cy + cardH - 32, 0.22f, {1, 1, 1, 1});

        col++;
        if (cx + cardW + gap > 1920.0f - 80.0f)
        {
            col = 0;
            row++;
        }
    }
}
