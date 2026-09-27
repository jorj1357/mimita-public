// 2026-09-27
/* purpose
* Phase 0 feasibility probe for the actor-triangle collision migration.
* Uses the shared GL-free loaders in actor-collision-mesh.cpp to prove body-part
* and weapon render-mesh triangles can be produced with no GL context and
* transformed into world space the way the unified solver will consume them.
* Does NOT collide, correct, render, or mutate any Player.
* Does NOT duplicate GLB parsing; the shared loader is the single owner.
* Does NOT require a window, GL context, or running game loop.
*/

#include "physics/movement/actor-triangle-spike.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

#include "physics/movement/actor-collision-mesh.h"

namespace {

constexpr const char* kDefaultBodyPath =
    "assets/entity/player/default/mimita-char-no-animations-v4.glb";

constexpr const char* kWeaponCandidates[] = {
    "assets/objects/weapons/mimita-revolver-v1.glb",
    "assets/objects/weapons/mimita-shotgun-v1.glb",
    "assets/objects/weapons/mimita-scythe-v1.glb",
};

bool finiteAabb(const glm::vec3& mn, const glm::vec3& mx)
{
    return std::isfinite(mn.x) && std::isfinite(mn.y) && std::isfinite(mn.z) &&
           std::isfinite(mx.x) && std::isfinite(mx.y) && std::isfinite(mx.z) &&
           mx.x >= mn.x && mx.y >= mn.y && mx.z >= mn.z;
}

} // namespace

bool actorTriangleSpike(std::string* outSummary)
{
    std::string report;
    bool ok = true;

    const bool headless = (glfwGetCurrentContext() == nullptr);
    report += std::string("[SPIKE] headless(no GL context)=") + (headless ? "yes" : "no") + "\n";

    // ── Spike 1 + 3: body-part triangles, CPU-only. ───────────────────────
    {
        std::vector<ActorMeshPart> parts;
        const bool loaded = loadActorBodyMeshParts(kDefaultBodyPath, parts);
        int totalTriangles = 0;
        for (const ActorMeshPart& part : parts)
        {
            totalTriangles += (int)part.triangles.size();
            char line[256];
            std::snprintf(line, sizeof(line),
                "[SPIKE][BODY] part=%-8s node=%d localTriangles=%zu localAabb=(%.3f %.3f %.3f)-(%.3f %.3f %.3f)\n",
                part.name.c_str(), part.nodeIndex, part.triangles.size(),
                part.localMin.x, part.localMin.y, part.localMin.z,
                part.localMax.x, part.localMax.y, part.localMax.z);
            report += line;
        }
        const bool bodyOk = loaded && parts.size() == 6 && totalTriangles > 0;
        char line[192];
        std::snprintf(line, sizeof(line),
            "[SPIKE][BODY] loaded=%d parts=%zu/6 totalTriangles=%d\n",
            (int)loaded, parts.size(), totalTriangles);
        report += line;
        report += bodyOk
            ? "[SPIKE][BODY] PASS: body triangles extract CPU-only via shared loader\n"
            : "[SPIKE][BODY] FAIL: body triangles not extractable headlessly\n";
        ok = ok && bodyOk;
    }

    // ── Spike 2 + 3: weapon render-mesh triangles + world transform. ──────
    {
        bool weaponLoaded = false;
        std::string usedPath;
        int weaponTriangles = 0;
        glm::vec3 mn(0.0f), mx(0.0f), wmn(0.0f), wmx(0.0f);
        bool fin = false;
        bool nonDegenerate = false;

        for (const char* candidate : kWeaponCandidates)
        {
            std::vector<CollisionTriangle> tris;
            if (!loadActorWeaponTriangles(candidate, tris) || tris.empty())
                continue;

            weaponLoaded = true;
            usedPath = candidate;
            weaponTriangles = (int)tris.size();

            mn = glm::vec3(std::numeric_limits<float>::max());
            mx = glm::vec3(-std::numeric_limits<float>::max());
            for (const CollisionTriangle& t : tris)
                for (const glm::vec3& v : {t.a, t.b, t.c})
                {
                    mn = glm::min(mn, v);
                    mx = glm::max(mx, v);
                }

            glm::mat4 weaponTransform(1.0f);
            weaponTransform = glm::translate(weaponTransform, glm::vec3(1.5f, 0.25f, 1.1f));
            weaponTransform = glm::rotate(weaponTransform, glm::radians(90.0f), glm::vec3(0, 1, 0));
            weaponTransform = glm::rotate(weaponTransform, glm::radians(15.0f), glm::vec3(1, 0, 0));
            weaponTransform = glm::scale(weaponTransform, glm::vec3(1.3f));

            wmn = glm::vec3(std::numeric_limits<float>::max());
            wmx = glm::vec3(-std::numeric_limits<float>::max());
            float maxEdge = 0.0f;
            for (const CollisionTriangle& t : tris)
            {
                const glm::vec3 a = glm::vec3(weaponTransform * glm::vec4(t.a, 1.0f));
                const glm::vec3 b = glm::vec3(weaponTransform * glm::vec4(t.b, 1.0f));
                const glm::vec3 c = glm::vec3(weaponTransform * glm::vec4(t.c, 1.0f));
                for (const glm::vec3& v : {a, b, c})
                {
                    wmn = glm::min(wmn, v);
                    wmx = glm::max(wmx, v);
                }
                maxEdge = std::max({maxEdge, glm::length(b - a),
                                    glm::length(c - b), glm::length(a - c)});
            }
            fin = finiteAabb(wmn, wmx);
            nonDegenerate = maxEdge > 1e-4f;
            break;
        }

        char line[320];
        std::snprintf(line, sizeof(line),
            "[SPIKE][WEAPON] loaded=%d path=%s localTriangles=%d localAabb=(%.3f %.3f %.3f)-(%.3f %.3f %.3f)\n",
            (int)weaponLoaded, usedPath.c_str(), weaponTriangles,
            mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
        report += line;
        std::snprintf(line, sizeof(line),
            "[SPIKE][WEAPON] worldAabb=(%.3f %.3f %.3f)-(%.3f %.3f %.3f) finite=%d nonDegenerate=%d\n",
            wmn.x, wmn.y, wmn.z, wmx.x, wmx.y, wmx.z, (int)fin, (int)nonDegenerate);
        report += line;

        const bool weaponOk = weaponLoaded && weaponTriangles > 0 && fin && nonDegenerate;
        report += weaponOk
            ? "[SPIKE][WEAPON] PASS: render-mesh triangles transform to world space CPU-only\n"
            : "[SPIKE][WEAPON] FAIL: weapon render-mesh triangles unavailable\n";
        ok = ok && weaponOk;
    }

    report += ok
        ? "[SPIKE] RESULT: PASS (body + weapon triangles CPU-only, GL-free)\n"
        : "[SPIKE] RESULT: FAIL\n";

    if (outSummary)
        *outSummary = report;
    return ok;
}
