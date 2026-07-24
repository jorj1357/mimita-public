#include "character-registry.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "avatar/character-manifest.h"

CharacterRegistry& CharacterRegistry::instance()
{
    static CharacterRegistry sInstance;
    return sInstance;
}

void CharacterRegistry::scanAll(const std::string& charactersDir)
{
    mCharacters.clear();

    if (!std::filesystem::exists(charactersDir))
    {
        printf("[CHARACTER] directory not found: %s\n", charactersDir.c_str());
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(charactersDir))
    {
        if (!entry.is_directory())
            continue;

        std::string dirName = entry.path().filename().string();
        std::string manifestPath = (entry.path() / "manifest.json").string();

        if (dirName.empty() || dirName[0] == '.')
            continue;

        if (!std::filesystem::exists(manifestPath))
        {
            printf("[CHARACTER] %s: missing manifest.json\n", dirName.c_str());
            continue;
        }

        CharacterManifest manifest;
        if (!manifest.load(manifestPath))
        {
            printf("[CHARACTER] %s: failed to parse manifest\n", dirName.c_str());
            continue;
        }

        std::string modelPath = (entry.path() / manifest.model).string();
        if (!std::filesystem::exists(modelPath))
        {
            printf("[CHARACTER] %s: model file '%s' not found\n",
                   dirName.c_str(), manifest.model.c_str());
            continue;
        }

        if (!manifest.isValid())
        {
            printf("[CHARACTER] %s: invalid manifest: %s\n",
                   dirName.c_str(), manifest.validationError().c_str());
            continue;
        }

        mCharacters.push_back(manifest);
        printf("[CHARACTER] registered: %s\n", manifest.name.c_str());
    }

    std::sort(mCharacters.begin(), mCharacters.end(),
        [](const CharacterManifest& a, const CharacterManifest& b) {
            if (a.hidden != b.hidden) return !a.hidden;
            return a.name < b.name;
        });
}

void CharacterRegistry::ensureScanned() const
{
    if (!mScanned) {
        const_cast<CharacterRegistry*>(this)->scanAll();
        const_cast<CharacterRegistry*>(this)->mScanned = true;
    }
}

const CharacterManifest* CharacterRegistry::get(const std::string& name)
{
    ensureScanned();
    for (const auto& c : mCharacters)
    {
        if (c.name == name)
            return &c;
    }
    return nullptr;
}

std::vector<std::string> CharacterRegistry::names() const
{
    ensureScanned();
    std::vector<std::string> result;
    for (const auto& c : mCharacters)
    {
        if (!c.hidden)
            result.push_back(c.name);
    }
    return result;
}

const CharacterManifest* CharacterRegistry::firstAvailable() const
{
    ensureScanned();
    for (const auto& c : mCharacters)
    {
        if (!c.hidden)
            return &c;
    }
    return nullptr;
}
