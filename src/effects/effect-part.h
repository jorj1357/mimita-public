// 07 31 2026, 14 50
/* purpose
* Declares the EffectPart particle/effect pool and the EffectPartSystem spawn API.
* Owns effect spawn helpers for tracers, muzzle flashes, impacts, debris, and tick spheres.
* Does NOT own server weapon authority, packet send/receive, or damage validation.
* Does NOT render effects or run the fixed-step simulation tick.
*/
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
    std::string assetId;
    bool billboardText = true;
    float scale = 1.0f;
    float endScale = 1.0f;
    float alpha = 1.0f;
    float gravity = 0.0f;
    float drag = 0.0f;
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
    // Config-driven debris chunk count for debris_batch effects (0 = use default).
    int debrisCount = 0;

    void resetStrings() {
        label.clear();
        replayType = "effect";
        texturePath.clear();
        materialName.clear();
        assetId.clear();
        sourceActorId.clear();
        targetActorId.clear();
    }
};

struct BloodParticle
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 color{0.92f, 0.015f, 0.025f};
    float size = 0.05f;
    float age = 0.0f;
    float lifetime = 0.5f;
    float alpha = 1.0f;
    float rotation = 0.0f;
    float stretch = 1.0f;
};

enum class SurfaceDecalKind { Blood, BulletHole, Crack };

struct SurfaceDecal
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 axis{0.0f, 0.0f, 1.0f};
    glm::vec3 color{0.75f, 0.01f, 0.02f};
    SurfaceDecalKind kind = SurfaceDecalKind::Blood;
    float radius = 0.025f;
    float height = 0.05f;
    float age = 0.0f;
    float lifetime = 30.0f;
    float fadeTime = 5.0f;
    float alpha = 1.0f;
    float baseAlpha = 1.0f;
    glm::vec3 colorStart{0.75f, 0.01f, 0.02f};
    glm::vec3 colorEnd{0.18f, 0.0f, 0.0f};
    float darkenStartSeconds = 1.0f;
    float darkenEndSeconds = 12.0f;
    bool darkenOverLifetime = false;
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
                          const std::string& sourceActorId, const std::string& targetActorId,
                          float directness = 1.0f, float hitDistance = -1.0f);
    EffectPart* spawnEntityImpact(glm::vec3 position, glm::vec3 normal,
                                  const std::string& sourceActorId, const std::string& targetActorId,
                                  float sizeScale = 1.0f);
    EffectPart* spawnWorldImpact(glm::vec3 position, glm::vec3 normal, float sizeScale = 1.0f, glm::vec3 direction = {});
    EffectPart* spawnMuzzleFlash(glm::vec3 position, const std::string& sourceActorId = {}, float sizeScale = 1.0f, const std::string& weaponId = {}, bool spawnVisual = true, bool spawnLighting = true);
    EffectPart* spawnTracer(glm::vec3 start, glm::vec3 end, const std::string& sourceActorId = {}, float sizeScale = 1.0f, const std::string& weaponId = {});
    EffectPart* spawnBulletImpact(glm::vec3 position, glm::vec3 normal, float sizeScale = 1.0f);
    void spawnWorldCracks(glm::vec3 position, glm::vec3 normal,
                          const glm::vec3& direction = glm::vec3(0.0f),
                          float sizeScale = 1.0f);
    EffectPart* spawnImpactSphereTick(glm::vec3 position, glm::vec3 color, float radius = 0.15f);
    // Spawn an impact tick using the configured color/radius from hitfx.json.
    EffectPart* spawnImpactSphereTickCfg(glm::vec3 position);
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
    void updateSurfaceDecals(float dt);
    void updatePendingBloodDecals(float dt);
    void pushSurfaceDecal(const SurfaceDecal& decal, int maxCount);
    void spawnBloodSurfaceDecals(const glm::vec3& hitPoint,
                                 const glm::vec3& forward,
                                 const glm::vec3& tangent,
                                 const glm::vec3& bitangent,
                                 float damageScale,
                                 float force,
                                 const std::string& sourceActorId,
                                 const std::string& targetActorId);

    static constexpr unsigned int POOL_SIZE = 4096;
    static constexpr unsigned int MAX_BLOOD_PARTICLES = 512;
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
    std::vector<int> mFreeSlots;
    std::vector<BloodParticle> mBloodParticles;
    std::vector<SurfaceDecal> mSurfaceDecals;
    struct PendingBloodDecal { SurfaceDecal decal; int ageTicks = 0; };
    std::vector<PendingBloodDecal> mPendingBloodDecals;
    std::array<BloodDebugSegment, MAX_BLOOD_DEBUG_SEGMENTS> mBloodDebugSegments{};
    unsigned int mActiveCount = 0;
    unsigned int mBloodDebugSegmentCount = 0;
    unsigned int mSpawnCursor = 0;
};
