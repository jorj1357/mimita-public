// 2026-09-28
/* purpose
* Deterministic self-test for the generalized impact + destructible-geometry
* slice (projectile rifle -> crate -> spherical cut -> generated triangles).
* Runs in-process on fixed inputs; no network, renderer, or wall clock.
* Does NOT test actor damage, networking, persistence, or shattering.
*/

#include "impact/impact-system.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "config/material-config.h"
#include "impact/destructible-geometry.h"
#include "physics/physical-entity.h"

namespace MimitaImpact {
namespace {

bool rayTriangle(const glm::vec3& origin, const glm::vec3& dir,
                 const CollisionTriangle& tri, float& outT)
{
    const glm::vec3 e1 = tri.b - tri.a;
    const glm::vec3 e2 = tri.c - tri.a;
    const glm::vec3 p = glm::cross(dir, e2);
    const float det = glm::dot(e1, p);
    if (std::fabs(det) < 1e-8f)
        return false;
    const float invDet = 1.0f / det;
    const glm::vec3 tvec = origin - tri.a;
    const float u = glm::dot(tvec, p) * invDet;
    if (u < -1e-5f || u > 1.0f + 1e-5f)
        return false;
    const glm::vec3 q = glm::cross(tvec, e1);
    const float v = glm::dot(dir, q) * invDet;
    if (v < -1e-5f || u + v > 1.0f + 1e-5f)
        return false;
    const float t = glm::dot(e2, q) * invDet;
    if (t < 0.0f)
        return false;
    outT = t;
    return true;
}

bool rayHitsMesh(const std::vector<CollisionTriangle>& triangles,
                 const glm::vec3& origin, const glm::vec3& dir, float maxT)
{
    for (const CollisionTriangle& tri : triangles)
    {
        float t;
        if (rayTriangle(origin, dir, tri, t) && t <= maxT)
            return true;
    }
    return false;
}

uint64_t checksumMesh(const PhysicalEntity& entity)
{
    uint64_t sum = 1469598103934665603ull;
    auto mix = [&](const glm::vec3& p) {
        const uint64_t x = (uint64_t)(int64_t)std::lround(p.x * 4096.0f);
        const uint64_t y = (uint64_t)(int64_t)std::lround(p.y * 4096.0f);
        const uint64_t z = (uint64_t)(int64_t)std::lround(p.z * 4096.0f);
        sum = (sum ^ x) * 1099511628211ull;
        sum = (sum ^ y) * 1099511628211ull;
        sum = (sum ^ z) * 1099511628211ull;
    };
    for (const CollisionTriangle& tri : entity.localTriangles)
    {
        mix(tri.a); mix(tri.b); mix(tri.c);
    }
    return sum;
}

uint32_t makeCrate(PhysicalEntitySystem& system, glm::vec3 position, float half)
{
    std::vector<CollisionTriangle> box;
    buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(half));
    const uint32_t id = system.add(
        box, glm::translate(glm::mat4(1.0f), position),
        PhysicalEntityMotion::Static, materialIdForName("wood"));
    if (PhysicalEntity* e = system.find(id))
        ImpactSystem::instance().initializeEntity(*e, materialIdForName("wood"),
                                                  glm::vec3(half));
    return id;
}

ImpactResult submitRifle(uint32_t targetId, const glm::vec3& point,
                         const glm::vec3& normal, const glm::vec3& direction,
                         float mass, float speed, float radius)
{
    ImpactEvent ev;
    ev.source = ImpactSource::Projectile;
    ev.target = ImpactTarget::PhysicalEntity;
    ev.targetEntityId = targetId;
    ev.worldPoint = point;
    ev.worldNormal = normal;
    ev.worldDirection = direction;
    ev.mass = mass;
    ev.speed = speed;
    ev.radius = radius;
    ev.cutScale = 1.0f;
    ev.energy = ImpactSystem::kineticEnergy(mass, speed);
    return ImpactSystem::instance().submit(ev);
}

} // anonymous namespace

bool destructibleSelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    if (MaterialConfig::instance().revision() == 0)
        MaterialConfig::instance().load();

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();

    // 1. Impact math.
    {
        const float energy = ImpactSystem::kineticEnergy(0.02f, 900.0f);
        check(std::fabs(energy - 8100.0f) < 1.0f, "kinetic energy 0.5*m*v^2");

        const float direct = ImpactSystem::impactAngleFactor(
            glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        const float shallow = ImpactSystem::impactAngleFactor(
            glm::vec3(1.0f, 0.0f, -0.05f), glm::vec3(0.0f, 0.0f, 1.0f));
        check(std::fabs(direct - 1.0f) < 1e-4f, "direct hit angle factor = 1");
        check(shallow < direct, "shallow hit angle factor < direct");
    }

    // 2. Cut radius monotonic in speed and mass.
    {
        const MaterialDefinition& mat =
            MaterialConfig::instance().find(materialIdForName("wood"));
        ImpactEvent slow, fast;
        slow.energy = ImpactSystem::kineticEnergy(0.02f, 200.0f);
        slow.worldDirection = glm::vec3(0, 0, -1);
        slow.worldNormal = glm::vec3(0, 0, 1);
        slow.radius = 0.01f;
        fast = slow;
        fast.energy = ImpactSystem::kineticEnergy(0.02f, 900.0f);
        check(ImpactSystem::calculateCutRadius(fast, mat) >
              ImpactSystem::calculateCutRadius(slow, mat),
              "higher speed creates a larger cut radius");

        ImpactEvent light = fast, heavy = fast;
        light.energy = ImpactSystem::kineticEnergy(0.005f, 900.0f);
        heavy.energy = ImpactSystem::kineticEnergy(0.05f, 900.0f);
        check(ImpactSystem::calculateCutRadius(heavy, mat) >
              ImpactSystem::calculateCutRadius(light, mat),
              "higher mass creates a larger cut radius");
    }

    // 3. One projectile creates one spherical cut, stored in local space.
    {
        system.clear();
        const glm::vec3 pos(10.0f, 0.0f, 0.0f);
        const uint32_t id = makeCrate(system, pos, 2.5f);
        PhysicalEntity* crate = system.find(id);
        const ImpactResult r = submitRifle(id, pos + glm::vec3(0, 0, 2.5f),
                                           glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                           0.02f, 900.0f, 0.01f);
        check(r.applied && r.cutCreated, "projectile impact applied");
        check(crate && crate->destructible.sphereCuts.size() == 1,
              "one projectile stores exactly one cut");
        if (crate && !crate->destructible.sphereCuts.empty())
        {
            const glm::vec3 local = crate->destructible.sphereCuts[0].localCenter;
            check(std::fabs(local.x) < 1e-3f && std::fabs(local.y) < 1e-3f &&
                  std::fabs(local.z - 2.5f) < 1e-3f,
                  "cut stored in crate-local coordinates");
            check(r.cutRadius > 0.05f && r.cutRadius <= 1.5f,
                  "cut radius within material clamp");
        }
    }

    // 4. Shallow hit damages less than a direct hit.
    {
        system.clear();
        const uint32_t directId = makeCrate(system, glm::vec3(0.0f), 2.5f);
        const uint32_t shallowId = makeCrate(system, glm::vec3(50.0f, 0.0f, 0.0f), 2.5f);
        const ImpactResult direct = submitRifle(directId, glm::vec3(0, 0, 2.5f),
                                                glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                                0.02f, 900.0f, 0.01f);
        const ImpactResult shallow = submitRifle(shallowId,
                                                 glm::vec3(50.0f, 0.0f, 2.5f),
                                                 glm::vec3(0, 0, 1),
                                                 glm::vec3(1.0f, 0.0f, -0.05f),
                                                 0.02f, 900.0f, 0.01f);
        check(direct.damage > shallow.damage, "shallow hit < direct hit damage");
    }

    // 5. Rebuilding the same cuts produces identical triangles.
    {
        system.clear();
        const uint32_t a = makeCrate(system, glm::vec3(0.0f), 2.5f);
        const uint32_t b = makeCrate(system, glm::vec3(0.0f), 2.5f);
        submitRifle(a, glm::vec3(0, 0, 2.5f), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                    0.02f, 900.0f, 0.01f);
        submitRifle(b, glm::vec3(0, 0, 2.5f), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                    0.02f, 900.0f, 0.01f);
        PhysicalEntity* ea = system.find(a);
        PhysicalEntity* eb = system.find(b);
        check(ea && eb && checksumMesh(*ea) == checksumMesh(*eb),
              "identical cuts produce identical triangles");
    }

    // 6. Interior hole triangles exist and the hole is physically empty.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        const ImpactResult r = submitRifle(id, glm::vec3(0, 0, 2.5f),
                                           glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                           0.02f, 900.0f, 0.01f);
        const glm::vec3 cutCenter = crate ? crate->destructible.sphereCuts[0].localCenter
                                          : glm::vec3(0.0f);
        // A point just inside the cut (toward the crate interior) is empty.
        const glm::vec3 inside = cutCenter + glm::vec3(0.0f, 0.0f, -r.cutRadius * 0.5f);
        check(destructibleCrateDistance(crate->destructible, inside) > 0.0f,
              "hole interior is empty in the SDF");
        check(destructibleCrateDistance(crate->destructible, glm::vec3(0, 0, -2.4f)) < 0.0f,
              "material away from the hole is still solid");

        bool interiorTri = false;
        for (const CollisionTriangle& tri : crate->localTriangles)
        {
            const glm::vec3 centroid = (tri.a + tri.b + tri.c) / 3.0f;
            if (glm::length(centroid - cutCenter) < r.cutRadius + 0.3f)
            {
                interiorTri = true;
                break;
            }
        }
        check(interiorTri, "generated triangles exist near the cut surface");
    }

    // 7. A sphere (union) reaching through the crate creates a pass-through hole.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        DestructionCutSphere c1;
        c1.localCenter = glm::vec3(1.25f, 0.0f, 0.0f);
        c1.radius = 1.5f;
        DestructionCutSphere c2;
        c2.localCenter = glm::vec3(-1.25f, 0.0f, 0.0f);
        c2.radius = 1.5f;
        DestructibleGeometrySystem::instance().addCut(crate->destructible, c1);
        DestructibleGeometrySystem::instance().addCut(crate->destructible, c2);
        check(!rayHitsMesh(crate->destructible.collisionTriangles,
                           glm::vec3(-4.0f, 0, 0), glm::vec3(1, 0, 0), 8.0f),
              "ray through the pass-through hole hits nothing");
    }

    // 8. Outside the hole stays collidable; cuts update the collision mesh.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        const uint64_t before = checksumMesh(*crate);
        submitRifle(id, glm::vec3(0, 0, 2.5f), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                    0.02f, 900.0f, 0.01f);
        check(checksumMesh(*crate) != before, "collision mesh updates after a cut");
        check(rayHitsMesh(crate->localTriangles,
                          glm::vec3(1.5f, 1.5f, -6.0f), glm::vec3(0, 0, 1), 20.0f),
              "crate remains collidable away from the hole");
    }

    // 9. Only affected chunks rebuild; triangle count stays bounded.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        const int totalChunks = crate->destructible.chunkCountPerAxis *
                                crate->destructible.chunkCountPerAxis *
                                crate->destructible.chunkCountPerAxis;
        const ImpactResult r = submitRifle(id, glm::vec3(0, 0, 2.5f),
                                           glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                           0.02f, 900.0f, 0.01f);
        check(r.chunksRebuilt > 0 && r.chunksRebuilt < totalChunks,
              "only affected chunks rebuild");

        for (int i = 0; i < 40; ++i)
        {
            const float s = (float)(i % 5) - 2.0f;
            submitRifle(id, glm::vec3(s, s * 0.4f, 2.5f), glm::vec3(0, 0, 1),
                        glm::vec3(0, 0, -1), 0.02f, 900.0f, 0.01f);
        }
        check(crate->localTriangles.size() <=
              DestructibleGeometrySystem::instance().maxTrianglesPerEntity,
              "triangle count stays within the budget");
    }

    if (outSummary)
        *outSummary = report;
    return ok;
}

} // namespace MimitaImpact
