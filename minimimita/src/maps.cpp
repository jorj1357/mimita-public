#include "maps.h"

static TestMap makeFlatFloor() {
    TestMap map;
    map.name = "Flat Floor";
    map.spawnPosition = glm::vec3(0.0f, 0.0f, 2.0f);
    float s = 10.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-s, -s, 0.0f),
        glm::vec3(s, -s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(s, -s, 0.0f),
        glm::vec3(s, s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    return map;
}

static TestMap makeRamp() {
    TestMap map;
    map.name = "Ramp";
    map.spawnPosition = glm::vec3(0.0f, -4.0f, 2.0f);
    float w = 6.0f, d = 10.0f, h = 3.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-w, -d, 0.0f),
        glm::vec3(w, -d, 0.0f),
        glm::vec3(-w, d, h)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(w, -d, 0.0f),
        glm::vec3(w, d, h),
        glm::vec3(-w, d, h)
    ));
    return map;
}

static TestMap makeAcuteCorner() {
    TestMap map;
    map.name = "Acute Corner";
    map.spawnPosition = glm::vec3(0.0f, 0.0f, 2.0f);
    float s = 8.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-s, -s, 0.0f),
        glm::vec3(s, -s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(s, -s, 0.0f),
        glm::vec3(s, s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    float aw = 0.3f;
    map.triangles.push_back(Triangle(
        glm::vec3(aw, aw, 0.0f),
        glm::vec3(aw, 0.0f, 0.0f),
        glm::vec3(aw, aw, 5.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(aw, 0.0f, 0.0f),
        glm::vec3(aw, 0.0f, 5.0f),
        glm::vec3(aw, aw, 5.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(0.0f, aw, 0.0f),
        glm::vec3(aw, aw, 0.0f),
        glm::vec3(0.0f, aw, 5.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(aw, aw, 0.0f),
        glm::vec3(aw, aw, 5.0f),
        glm::vec3(0.0f, aw, 5.0f)
    ));
    return map;
}

static TestMap makeNarrowGap() {
    TestMap map;
    map.name = "Narrow Gap";
    map.spawnPosition = glm::vec3(-4.0f, 0.0f, 2.0f);
    float gap = 0.5f;
    float len = 10.0f, w = 3.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-w, -len, 0.0f),
        glm::vec3(-gap, -len, 0.0f),
        glm::vec3(-w, len, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(-gap, -len, 0.0f),
        glm::vec3(-gap, len, 0.0f),
        glm::vec3(-w, len, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(gap, -len, 0.0f),
        glm::vec3(w, -len, 0.0f),
        glm::vec3(gap, len, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(w, -len, 0.0f),
        glm::vec3(w, len, 0.0f),
        glm::vec3(gap, len, 0.0f)
    ));
    return map;
}

static TestMap makeWallSlide() {
    TestMap map;
    map.name = "Wall Slide";
    map.spawnPosition = glm::vec3(-3.0f, 0.0f, 2.0f);
    float s = 8.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-s, -s, 0.0f),
        glm::vec3(s, -s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(s, -s, 0.0f),
        glm::vec3(s, s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    float wallH = 6.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-s, -s, 0.0f),
        glm::vec3(-s, s, 0.0f),
        glm::vec3(-s, -s, wallH)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(-s, s, 0.0f),
        glm::vec3(-s, s, wallH),
        glm::vec3(-s, -s, wallH)
    ));
    return map;
}

static TestMap makeStairs() {
    TestMap map;
    map.name = "Stairs";
    map.spawnPosition = glm::vec3(-5.0f, 0.0f, 1.5f);
    float stepW = 3.0f, stepD = 1.0f, stepH = 0.3f;
    int nSteps = 8;
    for (int i = 0; i < nSteps; ++i) {
        float x0 = -4.0f + i * stepD;
        float x1 = x0 + stepD;
        float z0 = i * stepH;
        float z1 = z0 + stepH;
        map.triangles.push_back(Triangle(
            glm::vec3(x0, -stepW, z0),
            glm::vec3(x1, -stepW, z0),
            glm::vec3(x0, stepW, z0)
        ));
        map.triangles.push_back(Triangle(
            glm::vec3(x1, -stepW, z0),
            glm::vec3(x1, stepW, z0),
            glm::vec3(x0, stepW, z0)
        ));
        map.triangles.push_back(Triangle(
            glm::vec3(x1, -stepW, z0),
            glm::vec3(x1, -stepW, z1),
            glm::vec3(x1, stepW, z0)
        ));
        map.triangles.push_back(Triangle(
            glm::vec3(x1, -stepW, z1),
            glm::vec3(x1, stepW, z1),
            glm::vec3(x1, stepW, z0)
        ));
    }
    return map;
}

static TestMap makeFloatingPlatform() {
    TestMap map;
    map.name = "Floating Platform";
    map.spawnPosition = glm::vec3(3.0f, 0.0f, 2.0f);
    float s = 10.0f;
    map.triangles.push_back(Triangle(
        glm::vec3(-s, -s, 0.0f),
        glm::vec3(s, -s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(s, -s, 0.0f),
        glm::vec3(s, s, 0.0f),
        glm::vec3(-s, s, 0.0f)
    ));
    float pw = 3.0f, ph = 2.5f;
    map.triangles.push_back(Triangle(
        glm::vec3(-pw, -pw, ph),
        glm::vec3(pw, -pw, ph),
        glm::vec3(-pw, pw, ph)
    ));
    map.triangles.push_back(Triangle(
        glm::vec3(pw, -pw, ph),
        glm::vec3(pw, pw, ph),
        glm::vec3(-pw, pw, ph)
    ));
    return map;
}

TestMap getMap(int index) {
    switch (index % MAP_COUNT) {
        case 0: return makeFlatFloor();
        case 1: return makeRamp();
        case 2: return makeAcuteCorner();
        case 3: return makeNarrowGap();
        case 4: return makeWallSlide();
        case 5: return makeStairs();
        case 6: return makeFloatingPlatform();
        default: return makeFlatFloor();
    }
}

const char* getMapName(int index) {
    static const char* names[] = {
        "1: Flat Floor",
        "2: Ramp",
        "3: Acute Corner",
        "4: Narrow Gap",
        "5: Wall Slide",
        "6: Stairs",
        "7: Floating Platform"
    };
    return names[index % MAP_COUNT];
}
