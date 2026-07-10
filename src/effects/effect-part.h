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
        label.clear();
        replayType = "effect";
        texturePath.clear();
        materialName.clear();
        sourceActorId.clear();
        targetActorId.clear();
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
    EffectPart* spawnFootstep(glm::vec3 position, float sizeScale = 1.0f);
    EffectPart* spawnDash(glm::vec3 position, float sizeScale = 1.0f);
    EffectPart* spawnPerfectDash(glm::vec3 position, float sizeScale = 1.0f);
    EffectPart* spawnFreeze(glm::vec3 position, float freezeDuration);
    EffectPart* spawnImpact(glm::vec3 position, glm::vec3 color, const char* label);
    EffectPart* spawnDamage(glm::vec3 position, const std::string& victim, int damage);
    EffectPart* spawnDamageImpactSphere(glm::vec3 position, glm::vec3 direction, const std::string& victim);
    void spawnBloodEffect(glm::vec3 hitPoint, glm::vec3 sprayDirection, float damage,
                          const std::string& sourceActorId, const std::string& targetActorId);
    EffectPart* spawnEntityImpact(glm::vec3 position, glm::vec3 normal,
                                  const std::string& sourceActorId, const std::string& targetActorId,
                                  float sizeScale = 1.0f);
    EffectPart* spawnWorldImpact(glm::vec3 position, glm::vec3 normal, float sizeScale = 1.0f);
    EffectPart* spawnMuzzleFlash(glm::vec3 position, const std::string& sourceActorId = {}, float sizeScale = 1.0f);
    EffectPart* spawnTracer(glm::vec3 start, glm::vec3 end, const std::string& sourceActorId = {}, float sizeScale = 1.0f);
    EffectPart* spawnBulletImpact(glm::vec3 position, float sizeScale = 1.0f);
    EffectPart* spawnDeathEllipsoid(glm::vec3 position, glm::vec3 direction, float length = 8.0f,
                                    float radius = 1.5f, float lifetime = 3.0f, float sizeScale = 1.0f);
    EffectPart* spawnFreezeTrail(glm::vec3 position);
    EffectPart* spawnDownDash(glm::vec3 position);
    void spawnWorldDebris(glm::vec3 position, glm::vec3 normal, float force = 1.0f, float sizeScale = 1.0f);
    void queueWorldHit(glm::vec3 position, glm::vec3 normal, glm::vec3 direction,
                       float debrisForce, const std::string& attacker,
                       const std::string& weaponSource);
    void drainPendingWorldHits(int maxCount);
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

    void updateBloodParticles(float dt);
    void updateBloodDecals(float dt);

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

    struct PendingWorldHit {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 direction;
        float debrisForce = 1.0f;
        std::string attacker;
        std::string weaponSource;
    };
    static constexpr int MAX_PENDING_HITS = 16;
    PendingWorldHit mPendingHits[MAX_PENDING_HITS];
    int mPendingHead = 0;
    int mPendingTail = 0;

    const class World* mWorld = nullptr;
    std::array<EffectPart, POOL_SIZE> mPool{};
    std::vector<BloodParticle> mBloodParticles;
    std::vector<BloodDecal> mBloodDecals;
    std::array<BloodDebugSegment, MAX_BLOOD_DEBUG_SEGMENTS> mBloodDebugSegments{};
    unsigned int mActiveCount = 0;
    unsigned int mBloodDebugSegmentCount = 0;
    unsigned int mSpawnCursor = 0;
};
