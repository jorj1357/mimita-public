// 2026-09-28
/* purpose
* Deterministic self-test for the generalized impact + destructible-geometry
* slice (projectile rifle -> crate -> spherical cut -> generated triangles).
* Runs in-process on fixed inputs; no network, renderer, or wall clock.
* Does NOT test actor damage, networking, persistence, or shattering.
*/

#include "impact/impact-system.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "config/material-config.h"
#include "impact/destructible-geometry.h"
#include "impact/destructible-mesh-loader.h"
#include "combat/projectile-simulation.h"
#include "physics/mesh-mass-properties.h"
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

// Closed octahedron with vertices at +/-r on each axis, outward CCW.
BooleanMesh makeOctahedron(float r, uint32_t materialId)
{
    const glm::vec3 v[6] = {
        { r, 0, 0}, {-r, 0, 0}, {0,  r, 0},
        {0, -r, 0}, {0, 0,  r}, {0, 0, -r}
    };
    const int tri[8][3] = {
        {0, 2, 4}, {1, 4, 2}, {1, 3, 4}, {0, 4, 3},
        {0, 5, 2}, {1, 2, 5}, {1, 5, 3}, {0, 3, 5}
    };
    BooleanMesh mesh;
    for (int t = 0; t < 8; ++t)
    {
        const glm::vec3 a = v[tri[t][0]];
        const glm::vec3 b = v[tri[t][1]];
        const glm::vec3 c = v[tri[t][2]];
        glm::vec3 n = glm::cross(b - a, c - a);
        const float len = glm::length(n);
        if (len <= 1e-9f)
            continue;
        n /= len;
        const uint32_t base = (uint32_t)mesh.vertices.size();
        for (const glm::vec3& p : {a, b, c})
        {
            BooleanMeshVertex out;
            out.position = p;
            out.normal = n;
            out.uv = glm::vec2(0.0f);
            out.materialId = materialId;
            mesh.vertices.push_back(out);
        }
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1u);
        mesh.indices.push_back(base + 2u);
    }
    return mesh;
}

// Two solid lobes joined by a thin neck, as one manifold. Cutting the neck must
// separate it into two independent pieces. Built by unioning boxes.
BooleanMesh makeDumbbell(float lobeOffset, float lobeHalf,
                         float neckHalf, float neckLength)
{
    std::vector<BooleanMesh> parts;
    parts.push_back(buildBooleanBoxMeshAt(glm::vec3(-lobeOffset, 0.0f, 0.0f),
                                          glm::vec3(lobeHalf), 0));
    parts.push_back(buildBooleanBoxMeshAt(glm::vec3(lobeOffset, 0.0f, 0.0f),
                                          glm::vec3(lobeHalf), 0));
    parts.push_back(buildBooleanBoxMeshAt(glm::vec3(0.0f, 0.0f, 0.0f),
                                          glm::vec3(neckLength, neckHalf, neckHalf), 0));
    return booleanUnion(parts, 0);
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
                         float mass, float speed, float radius,
                         float sizeScale = 0.0f)
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
    ev.sizeScale = sizeScale;
    ev.cutScale = 1.0f;
    ev.energy = ImpactSystem::kineticEnergy(mass, speed);
    ImpactResult result = ImpactSystem::instance().submit(ev);
    // Production defers the surface rebuild to the fixed tick; tests inspect
    // geometry immediately, so flush here (unbudgeted).
    ImpactSystem::instance().flushPendingCuts();
    if (PhysicalEntity* e = PhysicalEntitySystem::instance().find(targetId))
    {
        result.triangleCount = (uint32_t)e->localTriangles.size();
        result.componentCount = e->destructible.componentCount;
        result.remainingVolume = e->destructible.remainingVolume;
    }
    return result;
}

// Minimal CollisionWorldView that mirrors the server's entity query so the
// shared projectile kernel can be driven against a real destructible crate.
struct EntityOnlyWorld final : CollisionWorldView
{
    CollisionTriangle dummy{};

    void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float,
                             std::vector<int>&) const override {}
    const CollisionTriangle& triangleAt(int) const override { return dummy; }
    int triangleCount() const override { return 0; }
    void queryPlayerCapsulesSwept(const glm::vec3&, const glm::vec3&, float,
                                  std::vector<SweptPlayerCapsule>&) const override {}

    void queryEntityTrianglesSwept(const glm::vec3&, const glm::vec3&,
                                   float,
                                   std::vector<SweptEntityTriangle>& out) const override
    {
        for (const PhysicalEntity& e : PhysicalEntitySystem::instance().entities())
        {
            if (e.localTriangles.empty())
                continue;
            for (const CollisionTriangle& tri : e.localTriangles)
            {
                CollisionTriangle wt;
                wt.a = glm::vec3(e.transform * glm::vec4(tri.a, 1.0f));
                wt.b = glm::vec3(e.transform * glm::vec4(tri.b, 1.0f));
                wt.c = glm::vec3(e.transform * glm::vec4(tri.c, 1.0f));
                const glm::vec3 n = glm::cross(wt.b - wt.a, wt.c - wt.a);
                wt.normal = glm::length(n) > 1e-9f ? glm::normalize(n)
                                                   : glm::vec3(0.0f, 0.0f, 1.0f);
                SweptEntityTriangle swept;
                swept.entityId = e.id;
                swept.triangle = wt;
                out.push_back(swept);
            }
        }
    }
};

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
        check(crate && crate->destructible.cuts.size() == 1,
              "one projectile stores exactly one cut");
        if (crate && !crate->destructible.cuts.empty())
        {
            const glm::vec3 local = crate->destructible.cuts[0].cutter.localCenter;
            check(std::fabs(local.x) < 1e-3f && std::fabs(local.y) < 1e-3f &&
                  std::fabs(local.z - 2.5f) < 1e-3f,
                  "cut stored in crate-local coordinates");
            check(r.cutRadius > 0.05f && r.cutRadius <= 1.5f,
                  "cut radius within material clamp");
        }
    }

    // 3b. A localized cut must not shrink the whole crate. The old surface-nets
    // grid had no outside-air border and eroded the entire shell; the corner of
    // the crate must stay at the authored half extent after one small cut.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        submitRifle(id, glm::vec3(0.0f, 0.0f, 2.5f), glm::vec3(0, 0, 1),
                    glm::vec3(0, 0, -1), 0.02f, 900.0f, 0.01f);
        PhysicalEntity* crate = system.find(id);
        float maxCoord = 0.0f;
        if (crate)
            for (const CollisionTriangle& t : crate->localTriangles)
                for (const glm::vec3& v : {t.a, t.b, t.c})
                    maxCoord = std::max(maxCoord,
                        std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z))));
        check(crate && !crate->localTriangles.empty() &&
              std::fabs(maxCoord - 2.5f) < 1e-3f,
              "localized cut does not shrink the whole crate");
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
        const glm::vec3 cutCenter = crate && !crate->destructible.cuts.empty()
            ? crate->destructible.cuts[0].cutter.localCenter : glm::vec3(0.0f);
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
        DestructionCut c1;
        c1.cutter.type = BooleanCutterType::Sphere;
        c1.cutter.localCenter = glm::vec3(1.25f, 0.0f, 0.0f);
        c1.cutter.radius = 1.5f;
        DestructionCut c2;
        c2.cutter.type = BooleanCutterType::Sphere;
        c2.cutter.localCenter = glm::vec3(-1.25f, 0.0f, 0.0f);
        c2.cutter.radius = 1.5f;
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

    // 9. A cut rebuilds the surface and the triangle count stays low and bounded.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        const ImpactResult r = submitRifle(id, glm::vec3(0, 0, 2.5f),
                                           glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                           0.02f, 900.0f, 0.01f);
        check(r.chunksRebuilt >= 1, "a cut rebuilds the surface");
        check(crate->localTriangles.size() > 12 &&
              crate->localTriangles.size() < 1000,
              "destructible surface stays low-poly");

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

    // 10. Generated triangles have outward-consistent winding (front faces out).
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        submitRifle(id, glm::vec3(0, 0, 2.5f), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                    0.02f, 900.0f, 0.01f);
        PhysicalEntity* crate = system.find(id);
        bool windingOk = crate != nullptr && !crate->localTriangles.empty();
        if (crate)
        {
            for (const CollisionTriangle& t : crate->localTriangles)
            {
                const glm::vec3 gn = glm::cross(t.b - t.a, t.c - t.a);
                if (glm::dot(gn, t.normal) <= 0.0f)
                {
                    windingOk = false;
                    break;
                }
            }
        }
        check(windingOk, "generated triangles have outward-consistent winding");
    }

    // 11. The shared projectile kernel detects the crate (EntityImpact), which
    // is what routes a rifle hit into the cut.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        EntityOnlyWorld world;
        ProjectilePhysicsState state;
        state.position = glm::vec3(0.0f, 0.0f, -10.0f);
        state.velocity = glm::vec3(0.0f, 0.0f, 900.0f);
        ProjectilePhysicsConfig config;
        config.radius = 0.1f;
        config.lifetime = 3.0f;
        config.bounceEnabled = false;
        ProjectileStepResult step;
        bool hit = false;
        for (int i = 0; i < 20 && !hit; ++i)
        {
            step = simulateProjectileTick(state, config, world, 1.0f / 60.0f);
            if (step.type == ProjectileCollisionType::EntityImpact)
                hit = true;
        }
        check(hit && step.hitEntityId == id,
              "projectile sweep detects the crate as EntityImpact");
    }

    // 12. The mesh mass-properties integrator matches the analytic box. A box
    // with half extents (1,2,3) has volume 48 and, at unit density, the
    // diagonal inertia (208, 160, 80).
    {
        std::vector<CollisionTriangle> box;
        buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        const MeshMassProperties mp = computeMeshMassProperties(box);
        check(mp.valid, "box mesh mass properties are valid");
        check(std::fabs(mp.volume - 48.0f) < 0.01f,
              "mesh volume matches the analytic box volume");
        check(glm::length(mp.centerOfMass) < 1e-4f,
              "box center of mass is the origin");
        check(std::fabs(mp.unitInertiaDiagonal.x - 208.0f) < 0.1f &&
              std::fabs(mp.unitInertiaDiagonal.y - 160.0f) < 0.1f &&
              std::fabs(mp.unitInertiaDiagonal.z - 80.0f) < 0.1f,
              "mesh inertia matches the analytic box diagonal");
    }

    // 13. A cut removes mass, moves the center of mass away from the hole, and
    // updates inertia. The values are cached per geometry revision.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        crate->density = 1.0f;
        crate->mass = 125.0f;
        refreshEntityMassProperties(*crate);
        const float massBefore = crate->mass;
        const glm::vec3 inertiaBefore = crate->inertia;

        DestructionCut cut;
        cut.cutter.type = BooleanCutterType::Sphere;
        cut.cutter.localCenter = glm::vec3(0.0f, 0.0f, 2.0f);
        cut.cutter.radius = 1.0f;
        const bool cutOk = DestructibleGeometrySystem::instance().addCut(
            crate->destructible, cut) == 1;
        crate->localTriangles = crate->destructible.collisionTriangles;
        refreshEntityMassProperties(*crate);

        check(cutOk, "asymmetric cut is applied");
        check(crate->mass < massBefore && crate->mass > massBefore * 0.9f,
              "cut removes a proportional amount of mass");
        check(crate->destructible.massCenterOfMass.z < -0.005f &&
              std::fabs(crate->destructible.massCenterOfMass.x) < 1e-3f,
              "center of mass moves away from the cut");
        check(glm::length(crate->centerOfMass -
                          crate->destructible.massCenterOfMass) < 1e-5f,
              "physics uses the cached center of mass");
        check(crate->inertia != inertiaBefore,
              "inertia updates with the remaining material");
    }

    // 14. An authored closed mesh (the shape a GLB import produces) is destructible
// through the same owner as the crate: populated surface, mesh-derived mass, and
// a real cut.
    {
        system.clear();
        const BooleanMesh octahedron = makeOctahedron(1.0f, 0);
        std::string reason;
        check(booleanValidate(octahedron, &reason) == BooleanError::None,
              "authored closed mesh validates as a manifold");

        std::vector<CollisionTriangle> box;
        buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(1.0f));
        const uint32_t id = system.add(
            box, glm::mat4(1.0f), PhysicalEntityMotion::Dynamic);
        PhysicalEntity* e = system.find(id);
        e->halfExtents = glm::vec3(1.0f);
        e->density = 2.0f;
        ImpactSystem::instance().initializeEntityFromMesh(
            *e, octahedron, glm::vec3(1.0f), materialIdForName("wood"));

        check(!e->localTriangles.empty(),
              "imported mesh populates collision triangles at spawn");
        check(std::fabs(e->destructible.remainingVolume - 4.0f / 3.0f) < 0.01f,
              "imported mesh volume matches the octahedron");
        check(e->destructible.massFromMesh,
              "imported mesh uses mesh-derived mass properties");

        const float massBefore = e->mass;
        const size_t trianglesBefore = e->localTriangles.size();
        const ImpactResult r = submitRifle(id, glm::vec3(0, 0, 0.5f),
                                           glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                           0.02f, 900.0f, 0.01f);
        check(r.applied && r.cutCreated, "rifle cut applies to an imported mesh");
        check(e->localTriangles.size() != trianglesBefore,
              "imported mesh surface regenerates after a cut");
        check(e->mass < massBefore, "imported mesh loses mass after a cut");
    }

    // 15. The GLB loader is GL-free and never lets bad input into gameplay: a
    // missing file and a typical (non-watertight) weapon model both come back
    // with a reason, and a successful load always carries finite bounds.
    {
        const DestructibleMeshLoad missing =
            loadDestructibleMeshFromGLB("assets/does-not-exist.glb");
        check(!missing.success && !missing.error.empty(),
              "missing GLB is rejected with a reason");

        const DestructibleMeshLoad weapon = loadDestructibleMeshFromGLB(
            "assets/objects/weapons/mimita-revolver-v1.glb");
        check(!weapon.success ||
                  (weapon.mesh.triangleCount() > 0 && weapon.halfExtents.x > 0.0f),
              "GLB loader imports a closed mesh or rejects it cleanly");
        if (!weapon.success)
            check(!weapon.error.empty(), "rejected GLB reports the reason");

        const DestructibleMeshLoad sword = loadDestructibleMeshFromGLB(
            "assets/objects/things/cosmetics/sword1.glb");
        check(sword.success && sword.mesh.triangleCount() > 0 &&
                  sword.halfExtents.x > 0.0f,
              "a real watertight GLB imports as a closed destructible mesh");

        const DestructibleMeshLoad openMap =
            loadDestructibleMeshFromGLB("assets/maps/colltest.glb");
        check(!openMap.success && !openMap.error.empty(),
              "a non-watertight GLB is rejected with a reason");
    }

    // 16. The fracture trigger is silent on a healthy shape and fires when a cut
    // disconnects the material. A dumbbell base cut through its thin neck must
    // separate into two pieces.
    {
        system.clear();
        const BooleanMesh dumbbell = makeDumbbell(1.3f, 0.8f, 0.25f, 1.4f);
        std::string reason;
        const BooleanError valid = booleanValidate(dumbbell, &reason);
        check(valid == BooleanError::None && dumbbell.triangleCount() > 0,
              "the dumbbell base is a closed manifold");

        std::vector<CollisionTriangle> box;
        buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(2.0f));
        const uint32_t id = system.add(
            box, glm::mat4(1.0f), PhysicalEntityMotion::Dynamic);
        PhysicalEntity* e = system.find(id);
        e->halfExtents = glm::vec3(2.0f);
        ImpactSystem::instance().initializeEntityFromMesh(
            *e, dumbbell, glm::vec3(2.0f), materialIdForName("wood"));
        check(evaluateFracture(e->destructible,
                               DestructibleGeometrySystem::instance().fractureTuning)
                  .reason == FractureReason::None,
              "an uncut dumbbell does not want to fracture");

        // A sphere at the neck, larger than the neck cross-section, severs it.
        DestructionCut neck;
        neck.cutter.type = BooleanCutterType::Sphere;
        neck.cutter.localCenter = glm::vec3(0.0f, 0.0f, 0.0f);
        neck.cutter.radius = 0.5f;
        DestructibleGeometrySystem::instance().addCut(e->destructible, neck);
        e->localTriangles = e->destructible.collisionTriangles;
        const FractureDecision decision = evaluateFracture(
            e->destructible, DestructibleGeometrySystem::instance().fractureTuning);
        check(decision.shouldFracture &&
                  decision.reason == FractureReason::DisconnectedComponent,
              "cutting the neck triggers component fracture");

        std::vector<BooleanPiece> pieces =
            DestructibleGeometrySystem::instance().decomposePieces(e->destructible);
        check(pieces.size() >= 2, "decompose returns the disconnected pieces");
        check(!pieces.empty() && pieces[0].volume >= pieces.back().volume,
              "pieces are ordered largest first");
    }

    // 17. Cutting a supporting neck off one side unbalances the remaining piece:
    // the center of mass leaves the support footprint and fracture fires as
    // UnbalancedSupport without a disconnected component.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        // Remove a large off-center lobe from the +X side. The single remaining
        // piece's center of mass shifts to -X, away from the support footprint.
        DestructionCut lobe;
        lobe.cutter.type = BooleanCutterType::Sphere;
        lobe.cutter.localCenter = glm::vec3(2.0f, 0.0f, 0.0f);
        lobe.cutter.radius = 2.0f;
        DestructibleGeometrySystem::instance().addCut(crate->destructible, lobe);
        crate->localTriangles = crate->destructible.collisionTriangles;

        const FractureDecision decision = evaluateFracture(
            crate->destructible, DestructibleGeometrySystem::instance().fractureTuning);
        check(crate->destructible.componentCount == 1,
              "the unbalanced cut leaves one connected piece");
        check(decision.shouldFracture &&
                  decision.reason == FractureReason::UnbalancedSupport,
              "an off-center cut triggers support fracture");
        check(crate->destructible.massCenterOfMass.x < -0.05f,
              "center of mass moved away from the removed side");
    }

    // 18. End-to-end: a dynamic dumbbell whose neck is cut fractures through
    // the real ImpactSystem path into independent bodies; the hit entity keeps
    // the largest piece and children inherit material, mass, and motion.
    {
        system.clear();
        const BooleanMesh dumbbell = makeDumbbell(1.3f, 0.8f, 0.25f, 1.4f);
        std::vector<CollisionTriangle> box;
        buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(2.0f));
        const uint32_t id = system.add(
            box, glm::mat4(1.0f), PhysicalEntityMotion::Dynamic,
            materialIdForName("wood"));
        PhysicalEntity* e = system.find(id);
        e->halfExtents = glm::vec3(2.0f);
        e->density = 1.0f;
        e->velocity = glm::vec3(0.0f, 0.0f, -1.0f);
        e->angularVelocity = glm::vec3(0.0f, 0.0f, 2.0f);
        ImpactSystem::instance().initializeEntityFromMesh(
            *e, dumbbell, glm::vec3(2.0f), materialIdForName("wood"));
        const size_t before = system.entities().size();

        // Sever the neck directly, then submit a shot that observes the
        // disconnection and runs the fracture spawner.
        DestructionCut neck;
        neck.cutter.type = BooleanCutterType::Sphere;
        neck.cutter.localCenter = glm::vec3(0.0f);
        neck.cutter.radius = 0.5f;
        DestructibleGeometrySystem::instance().addCut(e->destructible, neck);
        e->localTriangles = e->destructible.collisionTriangles;
        refreshEntityMassProperties(*e);

        const ImpactResult result = submitRifle(id, glm::vec3(0, 0, 2.0f),
                                                glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                                0.02f, 900.0f, 0.01f);
        check(result.fractured && result.fragmentCount >= 1,
              "a disconnected dynamic object fractures into bodies");
        check(system.entities().size() > before,
              "fracture adds independent entities");
        check(system.find(id) != nullptr,
              "the hit entity keeps the primary piece");

        bool fragmentsValid = result.fragmentCount >= 1;
        for (uint32_t i = 0; i < result.fragmentCount; ++i)
        {
            PhysicalEntity* child = system.find(result.fragmentEntityIds[i]);
            if (!child || child->motion != PhysicalEntityMotion::Dynamic ||
                child->mass <= 0.0f || child->localTriangles.empty() ||
                child->serverDriven)
                fragmentsValid = false;
        }
        check(fragmentsValid,
              "each fragment is an independent dynamic body with mass and collision");

        // The primary piece should be the larger lobe, so the original entity's
        // mass is comparable to a child's, and all pieces share the material.
        check(result.fragmentCount >= 1 && system.find(id) != nullptr &&
                  system.find(id)->destructible.materialId == materialIdForName("wood"),
              "the primary piece keeps the source material");
    }

    // 19. Hole size is config-driven and follows force AND projectile size.
    // A big/slow projectile and a small/fast one can produce different holes;
    // both `radius` (size) and energy (force) must influence the result.
    {
        const MaterialDefinition& mat =
            MaterialConfig::instance().find(materialIdForName("wood"));

        ImpactEvent base;
        base.worldDirection = glm::vec3(0, 0, -1);
        base.worldNormal = glm::vec3(0, 0, 1);
        base.radius = 0.3f;
        base.cutScale = 1.0f;

        // Force: identical size, more energy -> bigger hole.
        ImpactEvent slow = base, fast = base;
        slow.energy = ImpactSystem::kineticEnergy(0.02f, 100.0f);
        fast.energy = ImpactSystem::kineticEnergy(0.02f, 800.0f);
        const float slowR = ImpactSystem::calculateCutRadius(slow, mat);
        const float fastR = ImpactSystem::calculateCutRadius(fast, mat);
        check(fastR > slowR, "more force cuts a bigger hole at equal size");

        // Size: identical force, bigger projectile -> bigger hole.
        ImpactEvent small = base, big = base;
        small.radius = 0.1f;
        big.radius = 0.8f;
        small.sizeScale = big.sizeScale = 1.0f;
        small.energy = big.energy = ImpactSystem::kineticEnergy(0.02f, 400.0f);
        const float smallR = ImpactSystem::calculateCutRadius(small, mat);
        const float bigR = ImpactSystem::calculateCutRadius(big, mat);
        check(bigR > smallR, "a bigger projectile cuts a bigger hole at equal force");

        // Size scale 0 == force only, so a huge projectile can be neutralized.
        ImpactEvent noSize = base;
        noSize.radius = 0.8f;
        noSize.sizeScale = 0.0f;
        noSize.energy = ImpactSystem::kineticEnergy(0.02f, 400.0f);
        check(ImpactSystem::calculateCutRadius(noSize, mat) < bigR,
              "cut_radius_scale=0 removes the projectile-size contribution");
    }

    // 20. One shot makes a real hole; every shot does, not just occasionally.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        // A rifle-sized projectile: radius 0.6 with size feeding the hole.
        int holesMade = 0;
        for (int i = 0; i < 10; ++i)
        {
            // Distinct spots so this measures "every shot cuts" not deepening.
            const float s = (float)(i - 5) * 0.2f;
            const ImpactResult r = submitRifle(id, glm::vec3(s, 0.0f, 2.5f),
                                               glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
                                               0.02f, 100.0f, 0.6f, 1.0f);
            if (r.cutCreated)
                ++holesMade;
        }
        check(holesMade == 10, "every rifle shot creates a hole");
        check(crate->destructible.cuts.size() == 10,
              "each shot stores one cut");
    }

    // 21. Repeated shots on the same axis tunnel through the object: after
    // enough hits the ray along that axis passes through empty space.
    {
        system.clear();
        const uint32_t id = makeCrate(system, glm::vec3(0.0f), 2.5f);
        PhysicalEntity* crate = system.find(id);
        const glm::vec3 rayDir(0.0f, 0.0f, 1.0f);
        // Shoot the same world axis, with the hit point advancing inward as a
        // penetrating projectile would, so successive shots remove the material
        // behind the previous hole until the path is clear.
        for (int i = 0; i < 16; ++i)
        {
            const float z = -2.5f + (float)i * 0.3f;
            submitRifle(id, glm::vec3(0.0f, 0.0f, z), glm::vec3(0, 0, 1),
                        rayDir, 0.02f, 100.0f, 0.6f, 1.0f);
        }

        const bool tunnelled = !rayHitsMesh(crate->localTriangles,
                                            glm::vec3(0.0f, 0.0f, -4.0f),
                                            rayDir, 8.0f);
        check(tunnelled, "repeated same-axis shots tunnel through the object");
    }

    if (outSummary)
        *outSummary = report;
    return ok;
}

} // namespace MimitaImpact
