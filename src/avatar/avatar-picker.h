#pragma once
#include <string>
#include <vector>

#include "character-manifest.h"

struct AvatarPickerResult {
    bool changed = false;
    std::string selectedCharacter;
};

void drawAvatarPicker(AvatarPickerResult& result);

void drawCharacterPreview(const CharacterManifest& manifest, float x, float y, float size, bool selected);
