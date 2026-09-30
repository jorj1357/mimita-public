// 2026-09-30
/* purpose
* Implement the mesh mass-properties integrator: signed tetrahedron volume and
* first/second moments over a closed triangle mesh, then the parallel-axis shift
* to the center of mass. Diagonal inertia only; see the header note.
* Does NOT own entities, gameplay mass, or motion.
*/

#include "physics/mesh-mass-properties.h"

#include <algorithm>
#include <cmath>

MeshMassProperties computeMeshMassProperties(
    const std::vector<CollisionTriangle>& triangles)
{
    MeshMassProperties out;
    if (triangles.empty())
        return out;

    double volume = 0.0;
    glm::dvec3 firstMoment(0.0);
    // Second moments about the origin: xx, xy, xz, yy, yz, zz.
    double xx = 0.0, xy = 0.0, xz = 0.0, yy = 0.0, yz = 0.0, zz = 0.0;

    for (const CollisionTriangle& tri : triangles)
    {
        const glm::dvec3 a(tri.a);
        const glm::dvec3 b(tri.b);
        const glm::dvec3 c(tri.c);
        const glm::dvec3 s = a + b + c;

        // Signed volume of the tetrahedron (origin, a, b, c). Inward-wound
        // cavity walls contribute negative volume exactly as intended.
        const double v = glm::dot(a, glm::cross(b, c)) / 6.0;
        if (!std::isfinite(v))
            return out;
        volume += v;
        firstMoment += v * s / 4.0;

        // Integral of x*x^T over the tetrahedron is
        // v/20 * (s*s^T + a*a^T + b*b^T + c*c^T).
        const double w = v / 20.0;
        auto accumulate = [&](const glm::dvec3& p) {
            xx += w * p.x * p.x;
            xy += w * p.x * p.y;
            xz += w * p.x * p.z;
            yy += w * p.y * p.y;
            yz += w * p.y * p.z;
            zz += w * p.z * p.z;
        };
        accumulate(s);
        accumulate(a);
        accumulate(b);
        accumulate(c);
    }

    if (std::fabs(volume) < 1e-9 || !std::isfinite(volume))
        return out;

    const glm::dvec3 com = firstMoment / volume;
    if (!std::isfinite(com.x) || !std::isfinite(com.y) || !std::isfinite(com.z))
        return out;

    // Inertia about the origin: I = trace(C) * identity - C.
    const double ixxOrigin = yy + zz;
    const double iyyOrigin = xx + zz;
    const double izzOrigin = xx + yy;

    // Parallel-axis shift from the origin to the center of mass.
    const double c2 = glm::dot(com, com);
    const double ixx = ixxOrigin - volume * (c2 - com.x * com.x);
    const double iyy = iyyOrigin - volume * (c2 - com.y * com.y);
    const double izz = izzOrigin - volume * (c2 - com.z * com.z);

    if (!std::isfinite(ixx) || !std::isfinite(iyy) || !std::isfinite(izz))
        return out;

    constexpr double kMinInertia = 1e-6;
    out.valid = true;
    out.volume = (float)std::fabs(volume);
    out.centerOfMass = glm::vec3(com);
    out.unitInertiaDiagonal = glm::vec3(
        (float)std::max(std::fabs(ixx), kMinInertia),
        (float)std::max(std::fabs(iyy), kMinInertia),
        (float)std::max(std::fabs(izz), kMinInertia));
    return out;
}