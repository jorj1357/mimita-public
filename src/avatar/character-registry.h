#pragma once
#include <string>
#include <vector>

#include "character-manifest.h"

class CharacterRegistry {
public:
    static CharacterRegistry& instance();

    void scanAll(const std::string& charactersDir = "Characters");
    const CharacterManifest* get(const std::string& name) const;
    const std::vector<CharacterManifest>& all() const { return mCharacters; }
    int count() const { return (int)mCharacters.size(); }
    std::vector<std::string> names() const;

    const CharacterManifest* firstAvailable() const;

private:
    CharacterRegistry() = default;
    std::vector<CharacterManifest> mCharacters;
};
