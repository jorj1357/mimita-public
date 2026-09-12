// 09 11 2026
/* purpose
* Owns persistent area-effect volumes: opaque smoke spheres and cycling fire cylinders.
* Provides one shared store/sim/render path for offline, server, and predicted clients.
* Applies fire damage-over-time on a tick interval; smoke only obstructs vision.
* Does NOT own weapon definitions, projectile physics, or packet transport.
* Does NOT decide which weapon spawns an effect; callers pass parsed params.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

class Camera;
struct WeaponDefinition;

enum class AreaEffectType : uint8_t
{
    Smoke = 1,
    Fire = 2
};

struct AreaEffectParams
{
    // Shared
    float radius = 8.0f;
    float height = 8.0f;
    int lifetimeTicks = 480;

    // Smoke
    glm::vec3 color{0.31f, 0.31f, 0.31f};
    float alpha = 1.0f;
    float visibilityMeters = 1.0f;
    bool hideHealthbars = true;

    // Fire
    float cycleSpeed = 2.0f;
    float damagePerInterval = 5.0f;
    int damageIntervalTicks = 10;
};

struct AreaEffectVolume
{
    uint32_t id = 0;
    AreaEffectType type = AreaEffectType::Smoke;
    glm::vec3 position{0.0f};
    AreaEffectParams params;
    int spawnTick = 0;
    int lifetimeTicks = 0;
    uint32_t ownerId = 0;

    bool expired(int currentTick) const
    {
        return lifetimeTicks > 0 && (currentTick - spawnTick) >= lifetimeTicks;
    }
};

// A damageable body tested against fire cylinders. Kept generic so the server
// (ServerPlayer/NPC) and offline (Player/NPC) both reuse one DoT rule.
struct AreaEffectContactTarget
{
    uint32_t id = 0;
    glm::vec3 center{0.0f};
    float radius = 0.5f;
    float height = 2.0f;
};

struct AreaEffectDamageEvent
{
    uint32_t targetId = 0;
    int damage = 0;
};

struct SmokeViewState
{
    bool inside = false;
    float viewMeters = 0.0f;
    bool hideHealthbars = false;
};

class AreaEffectSystem
{
public:
    static AreaEffectSystem& instance();

    void clear();

    uint32_t spawnSmoke(const glm::vec3& position, const AreaEffectParams& params,
                        int currentTick, uint32_t ownerId);
    uint32_t spawnFire(const glm::vec3& position, const AreaEffectParams& params,
                       int currentTick, uint32_t ownerId);

    // Expire finished volumes; remembers currentTick for render color cycling.
    void update(int currentTick);

    void render(const Camera& camera) const;

    // Vision limit + HUD suppression for a camera position inside any smoke.
    SmokeViewState queryCameraSmoke(const glm::vec3& cameraPos) const;

    // Fire damage for this tick. Callers should call once per gameplay tick.
    // Applies damage on damageIntervalTicks boundaries for targets inside a cylinder.
    void collectFireDamage(const std::vector<AreaEffectContactTarget>& targets,
                           int currentTick,
                           std::vector<AreaEffectDamageEvent>& out) const;

    const std::vector<AreaEffectVolume>& volumes() const { return mVolumes; }
    int currentTick() const { return mCurrentTick; }

private:
    AreaEffectSystem() = default;

    uint32_t spawn(AreaEffectType type, const glm::vec3& position,
                   const AreaEffectParams& params, int currentTick, uint32_t ownerId);

    std::vector<AreaEffectVolume> mVolumes;
    uint32_t mNextId = 1;
    int mCurrentTick = 0;
};

// Parse hot-reloadable weapon custom_params into effect params. `effect` selects
// which field set to read; both share radius/lifetime keys where meaningful.
AreaEffectParams areaEffectParamsFromDefinition(const WeaponDefinition& def,
                                                AreaEffectType effect);
