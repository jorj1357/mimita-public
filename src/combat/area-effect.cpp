// 09 11 2026
/* purpose
* Implements the shared persistent area-effect volume store, camera smoke query,
* fire damage-over-time, and world rendering for smoke spheres and fire cylinders.
* Reads hot-reloadable values from WeaponDefinition custom_params.
* Does NOT own networking, damage application to players, or weapon firing.
*/
#include "combat/area-effect.h"

#include <algorithm>
#include <cmath>

#include "camera.h"
#include "combat/weapon-types.h"
#include "debug/debug-visuals.h"

namespace
{
float cp(const WeaponDefinition& def, const char* key, float fallback)
{
    auto it = def.customParams.find(key);
    return it != def.customParams.end() ? it->second : fallback;
}

bool cpBool(const WeaponDefinition& def, const char* key, bool fallback)
{
    auto it = def.customParams.find(key);
    if (it == def.customParams.end())
        return fallback;
    return it->second > 0.0f;
}

// Fire color cycle stops: red -> orange -> white -> orange -> red.
const glm::vec3 kFireStops[5] = {
    {1.0f, 0.05f, 0.0f},
    {1.0f, 0.45f, 0.0f},
    {1.0f, 1.0f, 1.0f},
    {1.0f, 0.45f, 0.0f},
    {1.0f, 0.05f, 0.0f},
};

glm::vec3 fireCycleColor(float elapsedSeconds, float cycleSpeed)
{
    const float stepsPerSecond = std::max(0.01f, cycleSpeed);
    float phase = std::fmod(std::max(0.0f, elapsedSeconds) * stepsPerSecond, 5.0f);
    int i = (int)std::floor(phase) % 5;
    int j = (i + 1) % 5;
    float frac = phase - std::floor(phase);
    return glm::mix(kFireStops[i], kFireStops[j], frac);
}

bool pointInsideFireCylinder(const AreaEffectVolume& v,
                             const glm::vec3& point,
                             float pointRadius)
{
    const float dx = point.x - v.position.x;
    const float dy = point.y - v.position.y;
    const float horiz = std::sqrt(dx * dx + dy * dy);
    if (horiz > v.params.radius + pointRadius)
        return false;
    const float base = v.position.z;
    const float top = v.position.z + v.params.height;
    return point.z >= base - pointRadius && point.z <= top + pointRadius;
}

} // namespace

AreaEffectSystem& AreaEffectSystem::instance()
{
    static AreaEffectSystem system;
    return system;
}

void AreaEffectSystem::clear()
{
    mVolumes.clear();
    mNextId = 1;
    mCurrentTick = 0;
}

uint32_t AreaEffectSystem::spawn(AreaEffectType type, const glm::vec3& position,
                                 const AreaEffectParams& params,
                                 int currentTick, uint32_t ownerId)
{
    AreaEffectVolume volume;
    volume.id = mNextId++;
    volume.type = type;
    volume.position = position;
    volume.params = params;
    volume.spawnTick = currentTick;
    volume.lifetimeTicks = params.lifetimeTicks;
    volume.ownerId = ownerId;
    mVolumes.push_back(volume);
    return volume.id;
}

uint32_t AreaEffectSystem::spawnSmoke(const glm::vec3& position,
                                      const AreaEffectParams& params,
                                      int currentTick, uint32_t ownerId)
{
    return spawn(AreaEffectType::Smoke, position, params, currentTick, ownerId);
}

uint32_t AreaEffectSystem::spawnFire(const glm::vec3& position,
                                     const AreaEffectParams& params,
                                     int currentTick, uint32_t ownerId)
{
    return spawn(AreaEffectType::Fire, position, params, currentTick, ownerId);
}

void AreaEffectSystem::update(int currentTick)
{
    mCurrentTick = currentTick;
    for (auto it = mVolumes.begin(); it != mVolumes.end();)
    {
        if (it->expired(currentTick))
            it = mVolumes.erase(it);
        else
            ++it;
    }
}

void AreaEffectSystem::render(const Camera& camera) const
{
    for (const AreaEffectVolume& v : mVolumes)
    {
        if (v.type == AreaEffectType::Smoke)
        {
            DebugVis::drawFilledSphere(camera, v.position, v.params.radius,
                                       glm::vec4(v.params.color, v.params.alpha));
        }
        else if (v.type == AreaEffectType::Fire)
        {
            const float elapsed = (float)(mCurrentTick - v.spawnTick) / 60.0f;
            const glm::vec3 color = fireCycleColor(elapsed, v.params.cycleSpeed);
            const glm::vec3 center = v.position + glm::vec3(0.0f, 0.0f, v.params.height * 0.5f);
            DebugVis::drawFilledCylinder(camera, center, glm::vec3(0.0f, 0.0f, 1.0f),
                                         v.params.radius, v.params.height,
                                         glm::vec4(color, 1.0f));
        }
    }
}

SmokeViewState AreaEffectSystem::queryCameraSmoke(const glm::vec3& cameraPos) const
{
    SmokeViewState state;
    for (const AreaEffectVolume& v : mVolumes)
    {
        if (v.type != AreaEffectType::Smoke)
            continue;
        if (glm::length(cameraPos - v.position) > v.params.radius)
            continue;
        state.inside = true;
        // Tightest (smallest) view limit wins when inside multiple clouds.
        if (state.viewMeters <= 0.0f || v.params.visibilityMeters < state.viewMeters)
            state.viewMeters = v.params.visibilityMeters;
        state.hideHealthbars = state.hideHealthbars || v.params.hideHealthbars;
    }
    return state;
}

void AreaEffectSystem::collectFireDamage(
    const std::vector<AreaEffectContactTarget>& targets,
    int currentTick,
    std::vector<AreaEffectDamageEvent>& out) const
{
    for (const AreaEffectVolume& v : mVolumes)
    {
        if (v.type != AreaEffectType::Fire)
            continue;
        const int interval = std::max(1, v.params.damageIntervalTicks);
        if (currentTick % interval != 0)
            continue;
        for (const AreaEffectContactTarget& t : targets)
        {
            if (pointInsideFireCylinder(v, t.center, t.radius))
            {
                out.push_back({t.id, (int)std::round(v.params.damagePerInterval)});
            }
        }
    }
}

AreaEffectParams areaEffectParamsFromDefinition(const WeaponDefinition& def,
                                                AreaEffectType effect)
{
    AreaEffectParams params;
    if (effect == AreaEffectType::Smoke)
    {
        params.radius = cp(def, "smoke_radius", 10.0f);
        params.lifetimeTicks = (int)cp(def, "smoke_lifetime_ticks", 600.0f);
        params.color = glm::vec3(
            cp(def, "smoke_color_r", 0.31f),
            cp(def, "smoke_color_g", 0.31f),
            cp(def, "smoke_color_b", 0.31f));
        params.alpha = cp(def, "smoke_color_a", 1.0f);
        params.visibilityMeters = cp(def, "smoke_visibility_meters", 1.0f);
        params.hideHealthbars = cpBool(def, "smoke_hide_healthbars", true);
    }
    else
    {
        params.radius = cp(def, "fire_radius", 8.0f);
        params.height = cp(def, "fire_height", 8.0f);
        params.lifetimeTicks = (int)cp(def, "fire_lifetime_ticks", 480.0f);
        params.cycleSpeed = cp(def, "fire_cycle_speed", 2.0f);
        params.damagePerInterval = cp(def, "fire_damage_per_interval", 5.0f);
        params.damageIntervalTicks = (int)cp(def, "fire_damage_interval_ticks", 10.0f);
    }
    return params;
}
