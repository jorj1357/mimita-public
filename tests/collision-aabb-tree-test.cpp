// 09 29 2026
/* purpose
* Verify the flat AABB tree in src/physics/movement/collision-aabb-tree.h:
* empty builds, full/empty overlap, deterministic query-vs-brute-force parity,
* and no duplicate primitive results.
* Does NOT test the collision narrowphase, response, or the world mesh.
*/

#include <algorithm>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>

#include "physics/physics-types.h"
#include "physics/movement/collision-aabb-tree.h"

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-58s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

static bool overlaps(const AABB& a, const AABB& b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

static std::vector<AABB> makeGrid()
{
    std::vector<AABB> aabbs;
    // A 10x10x4 deterministic grid of unit-ish boxes with gaps.
    for (int x = 0; x < 10; ++x)
        for (int y = 0; y < 10; ++y)
            for (int z = 0; z < 4; ++z)
            {
                const glm::vec3 mn(x * 1.5f, y * 1.5f, z * 1.5f);
                aabbs.push_back({mn, mn + glm::vec3(1.0f)});
            }
    return aabbs;
}

static std::vector<int> allPrims(size_t n)
{
    std::vector<int> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = (int)i;
    return out;
}

static void testEmpty()
{
    TEST("empty tree query returns nothing");
    AabbTree tree;
    std::vector<int> out;
    tree.query(AABB{{-1, -1, -1}, {1, 1, 1}}, out);
    CHECK(out.empty(), "empty tree produced %zu results", out.size());
    PASS();
}

static void testFullAndNone()
{
    std::vector<AABB> aabbs = makeGrid();
    AabbTree tree;
    tree.build(allPrims(aabbs.size()), aabbs);

    TEST("query covering the world returns every primitive once");
    {
        std::vector<int> out;
        tree.query(AABB{{-100, -100, -100}, {100, 100, 100}}, out);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        CHECK(out.size() == aabbs.size(), "got %zu of %zu", out.size(), aabbs.size());
    }
    PASS();

    TEST("far query returns nothing");
    {
        std::vector<int> out;
        tree.query(AABB{{1000, 1000, 1000}, {1001, 1001, 1001}}, out);
        CHECK(out.empty(), "far query produced %zu results", out.size());
    }
    PASS();
}

static void testParity()
{
    TEST("query set matches brute force and has no duplicates");
    std::vector<AABB> aabbs = makeGrid();
    AabbTree tree;
    tree.build(allPrims(aabbs.size()), aabbs, 4);

    const AABB queries[] = {
        AABB{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        AABB{{-1.0f, -1.0f, -1.0f}, {2.2f, 2.2f, 2.2f}},
        AABB{{3.0f, 3.0f, 1.0f}, {5.5f, 5.5f, 3.5f}},
        AABB{{7.0f, 7.0f, 0.0f}, {100.0f, 100.0f, 100.0f}},
        AABB{{-50.0f, -50.0f, -50.0f}, {50.0f, 50.0f, 50.0f}},
    };
    for (int q = 0; q < 5; ++q)
    {
        std::vector<int> got;
        tree.query(queries[q], got);
        std::sort(got.begin(), got.end());
        got.erase(std::unique(got.begin(), got.end()), got.end());

        std::vector<int> expected;
        for (size_t i = 0; i < aabbs.size(); ++i)
            if (overlaps(queries[q], aabbs[i]))
                expected.push_back((int)i);

        char name[128];
        std::snprintf(name, sizeof(name),
            "query%d matches brute force (%zu vs %zu)", q + 1,
            got.size(), expected.size());
        CHECK(got == expected, "%s", name);
    }
    PASS();
}

static void testLeafSizes()
{
    TEST("parity holds for leaf sizes 1, 2, and 8");
    std::vector<AABB> aabbs = makeGrid();
    const std::vector<int> prims = allPrims(aabbs.size());
    const AABB q{{1.0f, 1.0f, 0.0f}, {8.0f, 8.0f, 4.0f}};

    std::vector<int> expected;
    for (size_t i = 0; i < aabbs.size(); ++i)
        if (overlaps(q, aabbs[i])) expected.push_back((int)i);

    for (int leaf : {1, 2, 8})
    {
        AabbTree tree;
        tree.build(prims, aabbs, leaf);
        std::vector<int> got;
        tree.query(q, got);
        std::sort(got.begin(), got.end());
        got.erase(std::unique(got.begin(), got.end()), got.end());
        CHECK(got == expected, "leaf=%d got %zu expected %zu",
              leaf, got.size(), expected.size());
    }
    PASS();
}

int main()
{
    printf("collision AABB tree\n");
    testEmpty();
    testFullAndNone();
    testParity();
    testLeafSizes();
    printf("\n%d passed, %d failed\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
