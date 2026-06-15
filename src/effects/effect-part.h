#pragma once

#include <array>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct EffectPart
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 angularVelocity{0.0f};
    glm::vec3 endPosition{0.0f};
    glm::vec3 halfSize{0.1f};
    float lifetime = 0.0f;
    float maxLifetime = 1.0f;
    std::string label;
    std::string replayType = "effect";
    std::string texturePath;
    std::string materialName;
    bool billboardText = true;
    float scale = 1.0f;
    float endScale = 1.0f;
    float alpha = 1.0f;
    float gravity = 0.0f;
    float thickness = 0.0f;
    float endThickness = 0.0f;
    std::string sourceActorId;
    std::string targetActorId;
    unsigned int ownerId = 0;
    bool affectedByGravity = false;
    bool sticky = false;
    bool flatDecal = false;
    bool debugVisual = false;
    bool alive = false;
    bool beam = false;
    bool box = false;

    void resetStrings() {
        label.clear(); label.shrink_to_fit();
        replayType = "effect";
        texturePath.clear(); texturePath.shrink_to_fit();
        materialName.clear(); materialName.shrink_to_fit();
        sourceActorId.clear(); sourceActorId.shrink_to_fit();
        targetActorId.clear(); targetActorId.shrink_to_fit();
    }
};

struct BloodParticle
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float size = 0.05f;
    float age = 0.0f;
    float lifetime = 0.5f;
    float alpha = 1.0f;
    float rotation = 0.0f;
    float stretch = 1.0f;
};

struct BloodDecal
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    float radius = 0.25f;
    float age = 0.0f;
    float lifetime = 30.0f;
    float rotation = 0.0f;
    float stretch = 1.0f;
    float alpha = 1.0f;
};

class EffectPartSystem
{
public:
    static EffectPartSystem& instance();
    
    void init();
    void update(float dt);
    void render(const class Camera& camera) const;
    
    EffectPart* spawn(const EffectPart& effect);
    EffectPart* spawnFootstep(glm::vec3 position);
    EffectPart* spawnDash(glm::vec3 position);
    EffectPart* spawnFreeze(glm::vec3 position, float freezeDuration);
    EffectPart* spawnImpact(glm::vec3 position, glm::vec3 color, const char* label);
    EffectPart* spawnDamage(glm::vec3 position, const std::string& victim, int damage);
    void spawnBloodEffect(glm::vec3 hitPoint, glm::vec3 sprayDirection, float damage,
                          const std::string& sourceActorId, const std::string& targetActorId);
    EffectPart* spawnEntityImpact(glm::vec3 position, glm::vec3 normal,
                                  const std::string& sourceActorId, const std::string& targetActorId);
    EffectPart* spawnWorldImpact(glm::vec3 position, glm::vec3 normal);
    EffectPart* spawnMuzzleFlash(glm::vec3 position, const std::string& sourceActorId = {});
    EffectPart* spawnTracer(glm::vec3 start, glm::vec3 end, const std::string& sourceActorId = {});
    EffectPart* spawnBulletImpact(glm::vec3 position);
    void spawnWorldDebris(glm::vec3 position, glm::vec3 normal, float force = 1.0f);
    void destroyOwner(unsigned int ownerId);
    EffectPart* spawnCustom(glm::vec3 position, glm::vec3 color, float lifetime, const char* label);
    
    void setWorld(const class World& world) { mWorld = &world; }
    void clear();
    
    unsigned int activeCount() const { return mActiveCount; }

    // Legacy API for code that still uses getEffects()
    const EffectPart* poolData() const { return mPool.data(); }

    struct PartSnapshot {
        glm::vec3 position;
        float scale;
        float alpha;
    };
    int collectAlive(PartSnapshot* out, int maxCount, float minAlpha) const;

private:
    EffectPartSystem() = default;

    static constexpr unsigned int POOL_SIZE = 4096;
    static constexpr unsigned int MAX_BLOOD_PARTICLES = 512;
    static constexpr unsigned int MAX_BLOOD_DECALS = 256;
    static constexpr unsigned int MAX_BLOOD_DEBUG_SEGMENTS = 256;

    struct BloodDebugSegment {
        glm::vec3 from{0.0f};
        glm::vec3 to{0.0f};
        glm::vec3 normal{0.0f};
        bool hit = false;
    };

    const class World* mWorld = nullptr;
    std::array<EffectPart, POOL_SIZE> mPool{};
    std::vector<BloodParticle> mBloodParticles;
    std::vector<BloodDecal> mBloodDecals;
    std::array<BloodDebugSegment, MAX_BLOOD_DEBUG_SEGMENTS> mBloodDebugSegments{};
    unsigned int mActiveCount = 0;
    unsigned int mBloodDebugSegmentCount = 0;
};
