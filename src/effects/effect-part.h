#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct EffectPart
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
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
    unsigned int ownerId = 0;
    bool affectedByGravity = false;
    bool sticky = false;
    bool flatDecal = false;
    bool debugVisual = false;
    bool alive = true;
    // Cylinder rendering for blood splatter
    bool cylinderDecal = false;
    float cylinderHeight = 0.0f;
    bool beam = false;
    bool box = false;
};

class EffectPartSystem
{
public:
    static EffectPartSystem& instance();
    
    void init();
    void update(float dt);
    void render(const class Camera& camera) const;
    
    // Spawn effect parts
    EffectPart* spawn(const EffectPart& effect);
    EffectPart* spawnFootstep(glm::vec3 position);
    EffectPart* spawnDash(glm::vec3 position);
    EffectPart* spawnFreeze(glm::vec3 position, float freezeDuration);
    EffectPart* spawnImpact(glm::vec3 position, glm::vec3 color, const char* label);
    EffectPart* spawnDamage(glm::vec3 position, const std::string& victim, int damage);
    void spawnBlood(glm::vec3 position, glm::vec3 direction, float amount);
    void spawnStickyBlood(glm::vec3 position, glm::vec3 normal, float force, unsigned int ownerId = 0);
    // Blood projection: raycast from hit point along direction, project blood onto surfaces behind target
    void spawnProjectedBlood(glm::vec3 hitPosition, glm::vec3 direction, float damage, float distance, const std::string& bodyPart, const class World& world);
    EffectPart* spawnWorldImpact(glm::vec3 position, glm::vec3 normal);
    EffectPart* spawnMuzzleFlash(glm::vec3 position);
    EffectPart* spawnTracer(glm::vec3 start, glm::vec3 end);
    EffectPart* spawnBulletImpact(glm::vec3 position);
    void spawnWorldDebris(glm::vec3 position, glm::vec3 normal);
    void destroyOwner(unsigned int ownerId);
    EffectPart* spawnCustom(glm::vec3 position, glm::vec3 color, float lifetime, const char* label);
    
    // Clear all effects
    void clear();
    
    const std::vector<EffectPart>& getEffects() const { return mEffects; }
    
private:
    EffectPartSystem() = default;
    std::vector<EffectPart> mEffects;
};
