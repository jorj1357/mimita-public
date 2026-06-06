#pragma once

#include <string>
#include <vector>

class Player;

class OutfitAtlas {
public:
    static OutfitAtlas& instance();

    bool apply(Player& player, const std::string& path, bool reloadTexture = false);
    void printDebug() const;
    const std::string& path() const { return mPath; }

private:
    std::string mPath;
    std::vector<std::string> mMappings;
};
