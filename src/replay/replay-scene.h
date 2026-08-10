// C:\important\mimita-priv-v8\src\replay\replay-scene.h
// 6 7 2026
/**
 * purpose
 * record ALL of it, gun model, all playerrs, all effect parts, all lighting changes, 
 * all text, all UI, all in game GUI, all names, all healthbars
 * all parts have their textures on them, lighting works, its all to scale,
 * so that i can export to a json or another format
 * and make frag movies in blender using mimita replay, and i can move camera
 * around a lot in belnder 
 */

#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct ReplayMaterialReference {
    std::string materialName;
    std::string texturePath;
    std::string shaderName;
};

struct ReplayAsset {
    std::string id;
    std::string type;
    std::string path;
    std::vector<ReplayMaterialReference> materials;
    std::string shaderName;
    std::string source;
};

struct ReplayWorldMetadata {
    std::string mapAssetId;
    std::string mapPath;
    std::vector<ReplayMaterialReference> materials;
};

struct ReplayLightingState {
    glm::vec3 directionalLight{0.0f, 0.0f, -1.0f};
    float ambientStrength = 0.0f;
    float diffuseStrength = 0.0f;
    float edgeDarkness = 0.0f;
    float edgeWidth = 0.0f;
    float aoDarkness = 0.0f;
    float aoContrast = 0.0f;
    float textureContrast = 1.0f;
    float textureBrightness = 1.0f;
};

struct ReplaySoundEvent {
    int tick = 0;
    std::string soundPath;
    bool world = false;
    glm::vec3 position{};
    float volume = 1.0f;
    float pitch = 1.0f;
    float maxDistance = 0.0f;
    glm::vec3 listenerPosition{};
    glm::vec3 listenerForward{0.0f, 1.0f, 0.0f};
    bool listenerValid = false;
};

struct ReplayBodyPartState {
    std::string name;
    glm::vec3 position{};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct ReplayActorState {
    std::string id;
    std::string name;
    std::string type;

    std::string modelPath;
    std::string weaponModelPath;
    std::string outfitPath;
    std::string characterName;
    std::string avatarName;

    glm::vec3 position {};
    glm::vec3 rotation {};
    glm::vec3 velocity {};

    int health = 100;
    int maxHealth = 100;
    int currentAmmo = 0;
    int reserveAmmo = 0;
    bool dead = false;

    bool shooting = false;
    bool reloading = false;
    bool grounded = false;
    bool collidable = true;

    float fade = 0.0f;
    float blackness = 0.0f;
    float sizeScale = 1.0f;

    std::string weaponName;
    std::string animationState;
    std::vector<ReplayBodyPartState> bodyParts;
};

struct ReplayEffectEvent {
    std::string type;
    std::string label;

    glm::vec3 position {};
    glm::vec3 direction {};
    glm::vec3 from {};
    glm::vec3 to {};

    glm::vec3 rotation {};
    glm::vec3 scale {1.0f};
    glm::vec3 endScale {1.0f};
    glm::vec4 color {1.0f};
    glm::vec3 velocity {};
    glm::vec3 normal {0.0f, 0.0f, 1.0f};

    int spawnTick = 0;
    float spawnTime = 0.0f;
    float startDelay = 0.0f;
    float lifetime = 0.0f;
    float alpha = 1.0f;
    float radius = 0.0f;
    float thickness = 0.0f;
    float endThickness = 0.0f;
    float gravity = 0.0f;
    std::string assetId;
    std::string assetPath;
    std::string soundPath;
    std::string sourceActorId;
    std::string targetActorId;
    std::string texturePath;
    std::string materialName;
    bool billboardText = false;
    bool beam = false;
};

struct ReplayKillfeedEvent {
    int tick = 0;
    std::string killerId;
    std::string killerName;
    std::string victimId;
    std::string victimName;
    std::string weaponName;
};

struct ReplayCameraState {
    glm::vec3 position {};
    glm::vec3 rotation {};

    float fov = 70.0f;
};

struct ReplaySceneFrame {
    int tick = 0;
    float time = 0.0f;

    ReplayCameraState camera;

    std::vector<ReplayActorState> actors;
    std::vector<ReplayEffectEvent> effects;
};
