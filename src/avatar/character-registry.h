#pragma once
#include <string>
#include <vector>

#include "character-manifest.h"

class CharacterRegistry {
public:
    static CharacterRegistry& instance();

    void scanAll(const std::string& charactersDir = "Characters");
    const CharacterManifest* get(const std::string& name);
    const std::vector<CharacterManifest>& all() const { ensureScanned(); return mCharacters; }
    int count() const { ensureScanned(); return (int)mCharacters.size(); }
    std::vector<std::string> names() const;

    const CharacterManifest* firstAvailable() const;

private:
    CharacterRegistry() = default;
    void ensureScanned() const;
    mutable bool mScanned = false;
    std::vector<CharacterManifest> mCharacters;
};
