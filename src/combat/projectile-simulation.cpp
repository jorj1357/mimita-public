#include "projectile-simulation.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// ── Closest point on triangle (shared utility) ──────────────────────
static glm::vec3 closestPointOnTriangle(
    const glm::vec3& p,
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d5 >= 0.0f && d6 >= 0.0f) return c;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return a + (d1 / (d1 - d3)) * ab;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return a + (d2 / (d2 - d6)) * ac;
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b);
    float denom = va + vb + vc;
    return denom == 0.0f ? a : (va * a + vb * b + vc * c) / denom;
}

// ── Resolve sphere-vs-world collision for one substep ───────────────
// Returns true if a collision was resolved (depenetration + possible bounce).
static bool resolveSphereWorld(
    ProjectilePhysicsState& state,
    const ProjectilePhysicsConfig& config,
    const CollisionWorldView& world)
{
    // Broadphase: gather candidate triangles near the sphere
    std::vector<int> candidates;
    world.querySphereTriangles(state.position, config.radius, candidates);

    bool hit = false;
    glm::vec3 bestNormal(0.0f, 0.0f, 1.0f);
    float bestPenetration = 0.0f;

    for (int idx : candidates)
    {
        const CollisionTriangle& tri = world.triangleAt(idx);
        glm::vec3 closest = closestPointOnTriangle(state.position, tri.a, tri.b, tri.c);
        glm::vec3 diff = state.position - closest;
        float dist = glm::length(diff);
        if (dist >= config.radius || dist <= 0.0001f)
            continue;
        float penetration = config.radius - dist;
        if (penetration > bestPenetration)
        {
            bestPenetration = penetration;
            bestNormal = diff / dist;
            hit = true;
        }
    }

    if (!hit)
        return false;

    // Depenetrate
    state.position += bestNormal * (bestPenetration + 0.001f);

    // Check if moving into the surface
    float into = glm::dot(state.velocity, bestNormal);
    if (into < 0.0f)
    {
        // Actual impact — apply restitution and friction once
        glm::vec3 tangent = state.velocity - bestNormal * into;
        float tangentRetention = std::clamp(1.0f - config.friction, 0.0f, 1.0f);
        state.velocity = tangent * tangentRetention - bestNormal * into * config.restitution;
        state.angularVelocity *= 0.35f;
        state.bounceCount++;
    }

    return true;
}

bool simulateProjectileTick(
    ProjectilePhysicsState& state,
    const ProjectilePhysicsConfig& config,
    const CollisionWorldView& world,
    float fixedDt)
{
    if (state.exploded || state.sleeping)
        return false;

    state.age += fixedDt;

    // Lifetime expiry
    if (config.lifetime > 0.0f && state.age >= config.lifetime)
    {
        state.exploded = true;
        return false;
    }

    // Adaptive substeps to prevent tunneling
    float speed = glm::length(state.velocity);
    float maxStep = std::max(config.radius * 0.5f, 0.1f);
    int subSteps = std::min((int)std::ceil(speed * fixedDt / maxStep) + 1, 8);
    float subDt = fixedDt / (float)subSteps;

    for (int s = 0; s < subSteps && !state.exploded; ++s)
    {
        // Gravity
        state.velocity.z -= config.gravity * subDt;

        // Linear drag
        state.velocity *= std::max(0.0f, 1.0f - config.drag * subDt);

        // Angular drag
        float angSpeed = glm::length(state.angularVelocity);
        if (config.angularDrag > 0.0f && angSpeed > 0.0f)
            state.angularVelocity *= std::max(0.0f, 1.0f - config.angularDrag * subDt);
        if (angSpeed > 0.0001f)
        {
            float newAngSpeed = glm::length(state.angularVelocity);
            if (newAngSpeed > 0.0001f)
            {
                glm::quat delta = glm::angleAxis(
                    newAngSpeed * subDt, glm::normalize(state.angularVelocity));
                state.rotation = glm::normalize(delta * state.rotation);
            }
        }

        // Position integration
        glm::vec3 step = state.velocity * subDt;
        state.position += step;

        // World collision
        resolveSphereWorld(state, config, world);
    }

    // Max-bounce settle: only on upward support normal with low speed
    if (state.bounceCount >= config.maxBounceCount && config.maxBounceCount > 0)
    {
        float spd = glm::length(state.velocity);
        if (spd > 0.1f)
        {
            // Reduce lateral velocity gradually, don't zero it
            glm::vec3 lateral(state.velocity.x, state.velocity.y, 0.0f);
            state.velocity -= lateral * 0.95f;
            if (state.velocity.z < -20.0f)
                state.velocity.z = -20.0f;
        }
        else
        {
            state.velocity = glm::vec3(0.0f);
            state.angularVelocity = glm::vec3(0.0f);
            state.sleeping = true;
        }
    }

    return !state.exploded;
}
