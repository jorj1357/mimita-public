// 2026-09-30
/* purpose
* Standalone proof that the vendored Manifold static library performs a real
* boolean subtraction on a hand-built 12-triangle cube and returns a valid,
* closed, manifold result.
* Does NOT link against the game, does NOT modify gameplay, and is not wired
* into build.py. This is dependency verification for the integration plan.
*
* Build (from repo root, game compiler on PATH):
*   g++ -std=c++17 -DMANIFOLD_PAR=-1 -Iexternal/manifold-prebuilt/include \
*       tools/manifold_probe.cpp external/manifold-prebuilt/lib/libmanifold.a \
*       -o build/manifold_probe.exe
* Run:
*   build/manifold_probe.exe
*/

#include <cmath>
#include <cstdio>
#include <vector>

#include <manifold/manifold.h>
#include <manifold/mesh.h>
#include <manifold/version.h>

namespace {

manifold::MeshGL makeCube(float half)
{
    const float h = half;
    const float p[8][3] = {
        {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
        {-h, -h,  h}, {h, -h,  h}, {h, h,  h}, {-h, h,  h},
    };
    const uint32_t tri[12][3] = {
        {4, 5, 6}, {4, 6, 7},   // +z front
        {1, 0, 3}, {1, 3, 2},   // -z back
        {0, 4, 7}, {0, 7, 3},   // -x left
        {5, 1, 2}, {5, 2, 6},   // +x right
        {0, 1, 5}, {0, 5, 4},   // -y bottom
        {3, 7, 6}, {3, 6, 2},   // +y top
    };

    manifold::MeshGL mesh;
    mesh.numProp = 3;
    mesh.vertProperties.resize(8 * 3);
    for (int v = 0; v < 8; ++v)
        for (int c = 0; c < 3; ++c)
            mesh.vertProperties[v * 3 + c] = p[v][c];
    mesh.triVerts.resize(12 * 3);
    for (int t = 0; t < 12; ++t)
        for (int c = 0; c < 3; ++c)
            mesh.triVerts[t * 3 + c] = tri[t][c];
    return mesh;
}

int gFailures = 0;

void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++gFailures;
}

} // namespace

int main()
{
    std::printf("Manifold probe: version %d.%d.%d\n",
                MANIFOLD_VERSION_MAJOR, MANIFOLD_VERSION_MINOR,
                MANIFOLD_VERSION_PATCH);

    // 1. A hand-built 12-triangle cube must import as a valid closed manifold.
    manifold::MeshGL cubeMesh = makeCube(0.5f);
    manifold::Manifold cube(cubeMesh);
    std::printf("Input cube: status=%d tris=%zu verts=%zu volume=%.6f\n",
                (int)cube.Status(), cube.NumTri(), cube.NumVert(), cube.Volume());
    check(cube.Status() == manifold::Manifold::Error::NoError, "cube imports with NoError");
    check(!cube.IsEmpty(), "cube is not empty");
    check(cube.NumTri() == 12 && cube.NumVert() == 8, "cube is 12 tris / 8 verts");
    check(std::fabs(cube.Volume() - 1.0) < 1e-5, "cube volume == 1.0");

    // 2. Subtract a sphere fully inside the cube: removes ~4/3*pi*r^3.
    const double r = 0.3;
    manifold::Manifold cutter = manifold::Manifold::Sphere(r, 32);
    manifold::Manifold result = cube - cutter;
    const double expectedRemoved = 4.0 / 3.0 * 3.141592653589793 * r * r * r;

    std::printf("Result: status=%d tris=%zu verts=%zu volume=%.6f (expected ~%.6f)\n",
                (int)result.Status(), result.NumTri(), result.NumVert(),
                result.Volume(), 1.0 - expectedRemoved);
    check(result.Status() == manifold::Manifold::Error::NoError, "subtract returns NoError");
    check(!result.IsEmpty(), "result is not empty");
    check(result.Volume() < cube.Volume(), "result volume decreased");
    check(std::fabs(result.Volume() - (1.0 - expectedRemoved)) < 5e-3,
          "result volume matches sphere subtraction");
    check(result.NumTri() > cube.NumTri(), "result has more triangles around the hole");

    // 3. Decompose must report the number of connected components. A shallow
    //    interior bubble leaves a single component.
    std::vector<manifold::Manifold> parts = result.Decompose();
    std::printf("Decompose: %zu component(s)\n", parts.size());
    for (size_t i = 0; i < parts.size(); ++i)
        std::printf("  part %zu: tris=%zu volume=%.6f\n", i,
                    parts[i].NumTri(), parts[i].Volume());
    check(parts.size() >= 1, "Decompose returns at least one component");

    // 4. Round-trip the output through MeshGL and confirm the provenance data
    //    the integration relies on is present.
    manifold::MeshGL out = result.GetMeshGL();
    std::printf("GetMeshGL: numProp=%u verts=%u tris=%u runs=%u "
                "mergeFrom=%zu mergeTo=%zu faceID=%zu\n",
                (unsigned)out.numProp, (unsigned)out.NumVert(),
                (unsigned)out.NumTri(), (unsigned)out.NumRun(),
                out.mergeFromVert.size(), out.mergeToVert.size(),
                out.faceID.size());
    check(out.numProp >= 3, "output has position properties");
    check(out.NumTri() == result.NumTri(), "MeshGL tri count matches Manifold");
    check(out.mergeFromVert.size() == out.mergeToVert.size(),
          "merge vectors are paired");
    check(out.NumRun() >= 1, "output is split into at least one run");
    check(out.runOriginalID.size() == out.NumRun(), "runOriginalID matches run count");
    check(out.triVerts.size() == result.NumTri() * 3, "triVerts stride is 3");
    std::printf("  cube OriginalID=%d, cutter OriginalID=%d\n",
                cube.OriginalID(), cutter.OriginalID());
    for (size_t run = 0; run < out.NumRun(); ++run)
        std::printf("  run %zu: originalID=%u beginTri=%u endTri=%u\n", run,
                    out.runOriginalID[run], out.runIndex[run],
                    out.runIndex[run + 1]);
    check(out.runIndex.size() == out.NumRun() + 1,
          "runIndex is one longer than runOriginalID");
    check(out.faceID.size() == out.NumTri(), "faceID is per-triangle");

    // 5. A cutter that does not touch the cube must be a no-op.
    manifold::Manifold far = cube - manifold::Manifold::Sphere(0.1, 16).Translate({5.0, 0.0, 0.0});
    check(far.Status() == manifold::Manifold::Error::NoError, "non-touching subtract is valid");
    check(std::fabs(far.Volume() - cube.Volume()) < 1e-5, "non-touching subtract is a no-op");

    // 6. A capsule cutter (cylinder + two spheres) must also produce a valid
    //    result, proving the capsule path is buildable from primitives.
    manifold::Manifold capsule =
        manifold::Manifold::Cylinder(1.0, 0.15, 0.15, 24, true)
            .Translate({0.0, 0.0, 0.0});
    capsule = capsule + manifold::Manifold::Sphere(0.15, 24).Translate({0.0, 0.0, 0.5});
    capsule = capsule + manifold::Manifold::Sphere(0.15, 24).Translate({0.0, 0.0, -0.5});
    manifold::Manifold drilled = cube - capsule;
    std::printf("Capsule drill: status=%d tris=%zu volume=%.6f\n",
                (int)drilled.Status(), drilled.NumTri(), drilled.Volume());
    check(drilled.Status() == manifold::Manifold::Error::NoError, "capsule subtract is valid");
    check(drilled.Volume() < cube.Volume(), "capsule subtract removed volume");

    std::printf("\n%s (%d failure(s))\n", gFailures == 0 ? "PROBE PASS" : "PROBE FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
