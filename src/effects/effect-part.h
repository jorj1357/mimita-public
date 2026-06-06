#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct EffectPart
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float lifetime = 0.0f;
    float maxLifetime = 1.0f;
    std::string label;
    bool billboardText = true;
    float scale = 1.0f;
    bool alive = true;
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
    EffectPart* spawnCustom(glm::vec3 position, glm::vec3 color, float lifetime, const char* label);
    
    // Clear all effects
    void clear();
    
    const std::vector<EffectPart>& getEffects() const { return mEffects; }
    
private:
    EffectPartSystem() = default;
    std::vector<EffectPart> mEffects;
};