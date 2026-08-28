// 08 27 2026, 14 00
/* purpose
* Replay scene data structures for recording and playback.
* Records actor state for Blender export and in-engine playback.
* fill in 3rd line
* fill in what this file DOES NOT do
* fill in 2nd line
* fill in 3rd line
*/

#pragma once

#include <array>
#include <string>
#include <vector>
#include <cstdint>

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

    float sizeScale = 1.0f;

    std::string weaponName;
    static constexpr int MAX_BODY_PARTS = 6;
    std::array<ReplayBodyPartState, MAX_BODY_PARTS> bodyParts{};
    uint8_t bodyPartCount = 0;
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

// ── Actor identity table: stores constant strings once per lifetime ──
// Identity changes only on spawn, respawn, avatar change, weapon change, etc.
// Per-tick recording references identity by stable uint32_t ID.
enum class ReplayActorType : uint8_t {
    Player = 0,
    Npc,
    RemotePlayer,
    RemoteNpc,
    Corpse,
    Count
};

static const char* kReplayActorTypeNames[] = {
    "player", "npc", "remote_player", "remote_npc", "corpse"
};

struct ReplayActorIdentity {
    uint32_t id = 0;                     // stable actor ID (e.g., npc.id, remote entity id)
    ReplayActorType type = ReplayActorType::Player;
    std::string idString;                 // "player", "npc_5", "remote_3"
    std::string name;
    std::string modelPath;
    std::string outfitPath;
    std::string characterName;
    std::string avatarName;
    std::string weaponName;
    std::string weaponModelPath;
    uint32_t generation = 0;              // increments on respawn/identity change
};

// ── Compact body part pose (no strings) ──
enum class ReplayBodyPartId : uint8_t {
    Head = 0,
    Torso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Count
};

static const char* kReplayBodyPartNames[] = {
    "head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"
};

struct ReplayBodyPartPose {
    glm::vec3 position{};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

static constexpr int REPLAY_MAX_BODY_PARTS = 6;

// ── Per-tick compact actor state (no strings, no identity) ──
struct ReplayActorTickState {
    uint32_t actorId = 0;                 // references identity table
    glm::vec3 position{};
    glm::vec3 rotation{};
    glm::vec3 velocity{};
    int16_t health = 100;
    int16_t maxHealth = 100;
    uint16_t currentAmmo = 0;
    uint16_t reserveAmmo = 0;
    bool dead = false;
    bool shooting = false;
    bool reloading = false;
    bool grounded = false;
    float sizeScale = 1.0f;
    std::array<ReplayBodyPartPose, REPLAY_MAX_BODY_PARTS> bodyParts{};
    uint8_t bodyPartCount = 0;
};

// ── Delta detection: dirty mask for field-level change tracking ──
enum ReplayDirtyBits : uint32_t {
    ReplayDirtyNone     = 0,
    ReplayDirtyPosition = 1 << 0,
    ReplayDirtyRotation = 1 << 1,
    ReplayDirtyVelocity = 1 << 2,
    ReplayDirtyHealth   = 1 << 3,
    ReplayDirtyWeapon   = 1 << 4,
    ReplayDirtyPose     = 1 << 5,
    ReplayDirtyFlags    = 1 << 6,
    ReplayDirtyAll      = 0xFFFFFFFF
};

struct ReplaySceneFrameCompact {
    int tick = 0;
    float time = 0.0f;
    ReplayCameraState camera;
    std::vector<ReplayActorTickState> actors;       // only dirty actors
    std::vector<ReplayEffectEvent> effects;
    // Identity changes that occurred this tick (actor spawned, respawned, etc.)
    std::vector<uint32_t> identityChangeIds;
};
