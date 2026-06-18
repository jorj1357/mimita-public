#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Player;

struct AvatarPartFaces {
    std::string front;
    std::string back;
    std::string left;
    std::string right;
    std::string top;
    std::string bottom;

    std::string& byName(const std::string& name);
    const std::string& byName(const std::string& name) const;
};

struct SimpleAvatar {
    std::string face;
    std::string shirt;
    std::string pants;
    std::string skin;
};

struct AvatarDefinition {
    std::string name;
    std::string basePath;
    SimpleAvatar simple;
    bool advancedMode = false;
    AvatarPartFaces head;
    AvatarPartFaces torso;
    AvatarPartFaces leftArm;
    AvatarPartFaces rightArm;
    AvatarPartFaces leftLeg;
    AvatarPartFaces rightLeg;

    std::string resolve(const std::string& part, const std::string& face) const;
    void expandSimple();
    void clear();
};

class AvatarSystem {
public:
    static AvatarSystem& instance();

    bool loadAvatar(const std::string& avatarName);
    bool applyToPlayer(Player& player, bool reloadTextures = false);
    void pollHotReload();

    const AvatarDefinition& current() const { return mAvatar; }
    bool hasAvatar() const { return mHasAvatar; }
    const std::string& currentName() const { return mAvatarName; }

    bool saveSimple(const std::string& avatarName, const SimpleAvatar& simple);
    bool saveAdvanced(const std::string& avatarName, const AvatarDefinition& def);
    std::vector<std::string> listAvatars() const;
    std::vector<std::string> listPngs(const std::string& avatarName) const;

    void setSimple(const SimpleAvatar& simple) { mAvatar.simple = simple; mAvatar.expandSimple(); }
    void setPartFace(const std::string& part, const std::string& face, const std::string& texturePath);
    void setAdvancedMode(bool v) { mAvatar.advancedMode = v; }

    static std::string avatarPath(const std::string& name);

private:
    AvatarSystem() = default;

    bool buildAtlas(Player& player, bool reloadTextures);
    bool applyAtlasToPlayer(Player& player);
    std::string resolvePath(const std::string& relativePath) const;

    std::string mAvatarName;
    std::string mBasePath;
    AvatarDefinition mAvatar;
    bool mHasAvatar = false;
    GLuint mAtlasTexture = 0;

    std::filesystem::file_time_type mLastWriteTime;
    std::chrono::steady_clock::time_point mLastCheckTime;
    float mPollInterval = 0.25f;
};
