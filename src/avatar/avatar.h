#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Player;

struct FaceTransform {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scale = 1.0f;
    float rotation = 0.0f;
    int stretchMode = 0; // 0=stretch,1=fit,2=fill,3=crop,4=tile
    float hue = 0.0f;
    float saturation = 1.0f;
    float brightness = 1.0f;
    float contrast = 1.0f;
};

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

struct PartColors {
    glm::vec3 head     = glm::vec3(1.0f);
    glm::vec3 torso    = glm::vec3(1.0f);
    glm::vec3 leftArm  = glm::vec3(1.0f);
    glm::vec3 rightArm = glm::vec3(1.0f);
    glm::vec3 leftLeg  = glm::vec3(1.0f);
    glm::vec3 rightLeg = glm::vec3(1.0f);
};

struct FaceSettings {
    FaceTransform transform;
    std::string texture;
};

struct PartFaceSettings {
    FaceSettings front;
    FaceSettings back;
    FaceSettings left;
    FaceSettings right;
    FaceSettings top;
    FaceSettings bottom;

    FaceSettings& byName(const std::string& name);
    const FaceSettings& byName(const std::string& name) const;
};

struct SimpleAvatar {
    std::string face;
    std::string shirt;
    std::string pants;
    std::string skin;
};

struct CosmeticSlot {
    std::string slot;    // e.g. "head", "torso", "arms", "legs"
    std::string choice;  // e.g. "halo", "horns", "none"
};

struct AvatarPreset {
    std::string name;
    std::string description;
    PartColors colors;
    // Full face settings are stored per-preset in a separate file
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

    PartColors colors;
    std::vector<CosmeticSlot> cosmetics;
    std::string activePreset;

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

    void setPartColor(const std::string& part, const glm::vec3& color);
    glm::vec3 partColor(const std::string& part) const;

    bool savePreset(const std::string& presetName);
    bool loadPreset(const std::string& presetName);
    std::vector<std::string> listPresets() const;

    // Apply a single PNG texture to all body parts (replaces OutfitAtlas)
    static bool applySingleTexture(class Player& player, const std::string& texturePath, bool reloadTexture = false);

    // File operations
    static std::string avatarPath(const std::string& name);
    bool importPng(const std::string& sourcePath);
    bool createOutfit(const std::string& name);
    bool renameOutfit(const std::string& oldName, const std::string& newName);
    bool duplicateOutfit(const std::string& sourceName, const std::string& destName);
    bool deleteOutfit(const std::string& name);
    bool saveCurrentOutfit(const std::string& outfitName);

    // Clipboard for copy/paste within the editor session
    FaceSettings clipboardFace;
    PartFaceSettings clipboardPart;
    bool hasClipboardFace = false;
    bool hasClipboardPart = false;

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
