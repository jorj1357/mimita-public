// 09 21 2026
/* purpose
* Implements the fixed-spread determinism self-test.
* See pellet-pattern-selftest.h for scope.
*/
#include "combat/pellet-pattern-selftest.h"

#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

#include "combat/pellet-pattern.h"

namespace {

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b, float eps)
{
    return glm::length(a - b) <= eps;
}

void appendDirections(std::string& report, const char* label, const glm::vec3* dirs, int count)
{
    char line[256];
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(line, sizeof(line), "  %s[%d] = (%.6f, %.6f, %.6f)\n",
                      label, i, dirs[i].x, dirs[i].y, dirs[i].z);
        report += line;
    }
}

} // namespace

bool runPelletPatternSelfTest(std::string& report)
{
    report.clear();
    bool ok = true;
    const float eps = 1e-5f;

    const glm::vec3 base = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
    const int pelletCount = 15;
    const float spreadDegrees = 10.0f;

    glm::vec3 clientDirs[MAX_PELLETS_PER_BLAST]{};
    const int clientCount = buildFixedPelletDirections(
        base, pelletCount, spreadDegrees, clientDirs, MAX_PELLETS_PER_BLAST);
    if (clientCount != pelletCount)
    {
        char line[128];
        std::snprintf(line, sizeof(line),
                      "[FAIL] fixed grid count=%d expected=%d\n", clientCount, pelletCount);
        report += line;
        ok = false;
    }

    for (int i = 0; i < clientCount; ++i)
    {
        if (std::fabs(glm::length(clientDirs[i]) - 1.0f) > eps)
        {
            char line[128];
            std::snprintf(line, sizeof(line), "[FAIL] pellet %d not normalized\n", i);
            report += line;
            ok = false;
        }
    }

    // Server path with two different seeds must match the client path exactly.
    PelletPatternConfig seedA;
    seedA.pelletCount = pelletCount;
    seedA.spreadDegrees = spreadDegrees;
    seedA.spreadSeed = 0u;

    PelletPatternConfig seedB = seedA;
    seedB.spreadSeed = 0xdeadbeefu;

    glm::vec3 serverA[MAX_PELLETS_PER_BLAST]{};
    glm::vec3 serverB[MAX_PELLETS_PER_BLAST]{};
    const int countA = generatePelletDirections(base, seedA, serverA, MAX_PELLETS_PER_BLAST);
    const int countB = generatePelletDirections(base, seedB, serverB, MAX_PELLETS_PER_BLAST);

    if (countA != clientCount || countB != clientCount)
    {
        report += "[FAIL] server pellet count differs from client\n";
        ok = false;
    }
    else
    {
        for (int i = 0; i < clientCount; ++i)
        {
            if (!nearlyEqual(clientDirs[i], serverA[i], eps) ||
                !nearlyEqual(clientDirs[i], serverB[i], eps))
            {
                char line[160];
                std::snprintf(line, sizeof(line),
                              "[FAIL] seed changed pellet %d (client/server mismatch)\n", i);
                report += line;
                ok = false;
            }
        }
    }

    // Single-ray fixed cycle: reproducible and periodic.
    const float raySpread = 1.2f;
    const int cycleLength = 9;
    glm::vec3 firstRun[9]{};
    glm::vec3 secondRun[9]{};
    unsigned int idxA = 0u;
    unsigned int idxB = 0u;
    for (int i = 0; i < cycleLength; ++i)
        firstRun[i] = buildFixedSpreadDirection(base, raySpread, idxA);
    for (int i = 0; i < cycleLength; ++i)
        secondRun[i] = buildFixedSpreadDirection(base, raySpread, idxB);
    for (int i = 0; i < cycleLength; ++i)
    {
        if (!nearlyEqual(firstRun[i], secondRun[i], eps))
        {
            char line[128];
            std::snprintf(line, sizeof(line), "[FAIL] single-ray cycle not reproducible at %d\n", i);
            report += line;
            ok = false;
        }
    }

    // The 10th shot must repeat the 1st (fixed cycle wraps).
    const glm::vec3 tenth = buildFixedSpreadDirection(base, raySpread, idxA);
    if (!nearlyEqual(tenth, firstRun[0], eps))
    {
        report += "[FAIL] single-ray cycle did not wrap after 9 shots\n";
        ok = false;
    }

    // Center shot (cycle 0) has no offset; later shots move off-axis.
    if (!nearlyEqual(firstRun[0], base, eps))
    {
        report += "[FAIL] single-ray cycle shot 0 should equal base direction\n";
        ok = false;
    }
    bool anyOffset = false;
    for (int i = 1; i < cycleLength; ++i)
        if (!nearlyEqual(firstRun[i], base, 1e-4f))
            anyOffset = true;
    if (!anyOffset)
    {
        report += "[FAIL] single-ray cycle never offsets from base direction\n";
        ok = false;
    }

    // Zero spread must return the base direction unchanged (grid and ray).
    glm::vec3 noSpread[4]{};
    const int noSpreadCount = buildFixedPelletDirections(base, 4, 0.0f, noSpread, 4);
    if (noSpreadCount != 1 || !nearlyEqual(noSpread[0], base, eps))
    {
        report += "[FAIL] zero spread should return the base direction only\n";
        ok = false;
    }

    if (ok)
    {
        report += "[OK] pellet grid deterministic and seed-independent; single-ray cycle periodic\n";
        appendDirections(report, "pellet", clientDirs, clientCount);
    }
    return ok;
}
