// 09 29 2026
/* purpose
* Verify the limb replication buffer in src/ragdoll/ragdoll-replication.h:
* interpolation between two low-rate frames, inactive handling, and the fixed
* limb mapping.
* Does NOT test the transport, server relay, or rendering.
*/

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ragdoll/ragdoll-replication.h"

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-52s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

static RagdollReplicationPose makePose(float x, bool active = true)
{
    RagdollReplicationPose p;
    p.active = active;
    p.mode = RAGDOLL_NET_HYBRID;
    p.count = kRagdollLimbCount;
    for (int i = 0; i < kRagdollLimbCount; ++i) {
        p.limbs[i].position = glm::vec3(x + (float)i, 0.0f, 0.0f);
        p.limbs[i].orientation = glm::identity<glm::quat>();
    }
    return p;
}

static void testLimbMapping()
{
    TEST("fixed limb mapping matches the six body parts");
    CHECK(std::string(ragdollReplicatedPartName(0)) == "torso", "index 0");
    CHECK(std::string(ragdollReplicatedPartName(1)) == "head", "index 1");
    CHECK(std::string(ragdollReplicatedPartName(2)) == "leftArm", "index 2");
    CHECK(std::string(ragdollReplicatedPartName(3)) == "rightArm", "index 3");
    CHECK(std::string(ragdollReplicatedPartName(4)) == "leftLeg", "index 4");
    CHECK(std::string(ragdollReplicatedPartName(5)) == "rightLeg", "index 5");
    PASS();
}

static void testInterpolation()
{
    TEST("samples between two frames using the render delay");
    RagdollReplicationState s;
    s.push(makePose(0.0f), 0.0);
    s.push(makePose(10.0f), 100.0);

    RagdollReplicationPose out;
    // now=100ms with a 50ms delay samples t=50ms -> halfway between 0 and 100.
    CHECK(s.sample(100.0, 0.05, out), "sample should succeed");
    CHECK(out.active, "sampled pose active");
    CHECK(std::fabs(out.limbs[0].position.x - 5.0f) < 0.01f,
          "midpoint x=%.3f expected 5.0", (double)out.limbs[0].position.x);

    // Sample past the newest frame -> newest.
    CHECK(s.sample(400.0, 0.05, out), "sample should succeed");
    CHECK(std::fabs(out.limbs[0].position.x - 10.0f) < 0.01f,
          "newest x=%.3f expected 10.0", (double)out.limbs[0].position.x);
    PASS();
}

static void testInactive()
{
    TEST("inactive stream does not sample a pose");
    RagdollReplicationState s;
    RagdollReplicationPose idle;
    idle.active = false;
    s.push(idle, 0.0);

    RagdollReplicationPose out;
    CHECK(!s.sample(100.0, 0.05, out), "inactive should return false");
    PASS();
}

static void testLastFrameAfterStop()
{
    TEST("stops emitting after the owner goes inactive");
    RagdollReplicationState s;
    s.push(makePose(0.0f), 0.0);
    RagdollReplicationPose idle;
    idle.active = false;
    s.push(idle, 100.0);

    RagdollReplicationPose out;
    CHECK(!s.sample(200.0, 0.05, out), "should be inactive");
    PASS();
}

int main()
{
    printf("ragdoll replication buffer\n");
    testLimbMapping();
    testInterpolation();
    testInactive();
    testLastFrameAfterStop();
    printf("\n%d passed, %d failed\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
