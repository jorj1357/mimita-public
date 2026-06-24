#include "collision-grid.h"
#include "physics.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <cfloat>

static CollisionGrid gGrid;
static CollisionProfile gProfile;
static int gContactIterations = 0;
static float gMaxDepth = 0.0f;
static std::vector<int> gVisitedTriangles;
static int gVisitEpoch = 0;

void CollisionGrid::build(const std::vector<Triangle>& triangles, float size) {
    cellSize = size;
    if (triangles.empty()) { valid = false; return; }

    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (const auto& tri : triangles) {
        minX = std::min({minX, tri.a.x, tri.b.x, tri.c.x});
        minY = std::min({minY, tri.a.y, tri.b.y, tri.c.y});
        maxX = std::max({maxX, tri.a.x, tri.b.x, tri.c.x});
        maxY = std::max({maxY, tri.a.y, tri.b.y, tri.c.y});
    }
    originX = minX; originY = minY;
    cellsX = std::max(1, (int)ceil((maxX - minX) / cellSize) + 1);
    cellsY = std::max(1, (int)ceil((maxY - minY) / cellSize) + 1);
    cells.assign(cellsX * cellsY, std::vector<int>());

    for (int i = 0; i < (int)triangles.size(); ++i) {
        const auto& tri = triangles[i];
        float tminX = std::min({tri.a.x, tri.b.x, tri.c.x});
        float tminY = std::min({tri.a.y, tri.b.y, tri.c.y});
        float tmaxX = std::max({tri.a.x, tri.b.x, tri.c.x});
        float tmaxY = std::max({tri.a.y, tri.b.y, tri.c.y});
        int cx0 = std::max(0, std::min((int)floor((tminX - originX) / cellSize), cellsX - 1));
        int cy0 = std::max(0, std::min((int)floor((tminY - originY) / cellSize), cellsY - 1));
        int cx1 = std::max(0, std::min((int)floor((tmaxX - originX) / cellSize), cellsX - 1));
        int cy1 = std::max(0, std::min((int)floor((tmaxY - originY) / cellSize), cellsY - 1));
        for (int cy = cy0; cy <= cy1; ++cy)
            for (int cx = cx0; cx <= cx1; ++cx)
                cells[cy * cellsX + cx].push_back(i);
    }
    valid = true;
}

void CollisionGrid::clear() {
    cells.clear();
    valid = false;
}

static void collectFromGrid(const Player& player, const std::vector<Triangle>& triangles,
                            ContactState& state, glm::vec3 a, glm::vec3 b, int& tested) {
    float minX = std::min(a.x, b.x) - player.radius;
    float minY = std::min(a.y, b.y) - player.radius;
    float maxX = std::max(a.x, b.x) + player.radius;
    float maxY = std::max(a.y, b.y) + player.radius;

    int cx0 = std::max(0, std::min((int)floor((minX - gGrid.originX) / gGrid.cellSize), gGrid.cellsX - 1));
    int cy0 = std::max(0, std::min((int)floor((minY - gGrid.originY) / gGrid.cellSize), gGrid.cellsY - 1));
    int cx1 = std::max(0, std::min((int)floor((maxX - gGrid.originX) / gGrid.cellSize), gGrid.cellsX - 1));
    int cy1 = std::max(0, std::min((int)floor((maxY - gGrid.originY) / gGrid.cellSize), gGrid.cellsY - 1));

    if ((int)gVisitedTriangles.size() < (int)triangles.size())
        gVisitedTriangles.resize(triangles.size(), 0);
    gVisitEpoch++;

    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const auto& cell = gGrid.cells[cy * gGrid.cellsX + cx];
            for (int triIdx : cell) {
                if (gVisitedTriangles[triIdx] == gVisitEpoch) continue;
                gVisitedTriangles[triIdx] = gVisitEpoch;
                tested++;
                Contact contact;
                if (capsuleTriangleCollision(a, b, player.radius, triangles[triIdx], contact)) {
                    contact.triangleIndex = triIdx;
                    if (contact.side == Contact::FLOOR) state.touchingFloor = true;
                    if (contact.side == Contact::WALL)  state.touchingWall = true;
                    if (contact.side == Contact::CEILING) state.touchingCeiling = true;
                    state.contacts.push_back(contact);
                }
            }
        }
    }
}

static void collectFromAll(const Player& player, const std::vector<Triangle>& triangles,
                           ContactState& state, glm::vec3 a, glm::vec3 b, int& tested) {
    tested = (int)triangles.size();
    for (size_t i = 0; i < triangles.size(); ++i) {
        Contact contact;
        if (capsuleTriangleCollision(a, b, player.radius, triangles[i], contact)) {
            contact.triangleIndex = (int)i;
            if (contact.side == Contact::FLOOR) state.touchingFloor = true;
            if (contact.side == Contact::WALL)  state.touchingWall = true;
            if (contact.side == Contact::CEILING) state.touchingCeiling = true;
            state.contacts.push_back(contact);
        }
    }
}

void collectContacts(const Player& player, const std::vector<Triangle>& triangles,
                     ContactState& state) {
    auto start = std::chrono::high_resolution_clock::now();
    state.clear();
    glm::vec3 a = player.capA();
    glm::vec3 b = player.capB();
    int tested = 0;
    if (gGrid.valid)
        collectFromGrid(player, triangles, state, a, b, tested);
    else
        collectFromAll(player, triangles, state, a, b, tested);
    auto end = std::chrono::high_resolution_clock::now();
    gProfile.collectTimeUs += (int)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    gProfile.trianglesTested += tested;
    gProfile.contactsGenerated += (int)state.contacts.size();
    gProfile.queriesPerFrame++;
}

static int findDeepestContact(const ContactState& state, float& maxDepth) {
    int deepestIdx = -1;
    maxDepth = -1.0f;
    for (size_t i = 0; i < state.contacts.size(); ++i) {
        float d = state.contacts[i].depth;
        if (d > maxDepth) {
            maxDepth = d;
            deepestIdx = (int)i;
        }
    }
    return deepestIdx;
}

static void recollectContactTriangles(const Player& player, const std::vector<Triangle>& triangles,
                                       std::vector<int>& contactTriIndices, ContactState& state) {
    state.clear();
    glm::vec3 a = player.capA();
    glm::vec3 b = player.capB();
    for (int triIdx : contactTriIndices) {
        Contact contact;
        if (capsuleTriangleCollision(a, b, player.radius, triangles[triIdx], contact)) {
            contact.triangleIndex = triIdx;
            if (contact.side == Contact::FLOOR) state.touchingFloor = true;
            if (contact.side == Contact::WALL) state.touchingWall = true;
            if (contact.side == Contact::CEILING) state.touchingCeiling = true;
            state.contacts.push_back(contact);
        }
    }
    contactTriIndices.clear();
    for (const auto& c : state.contacts) {
        if (std::find(contactTriIndices.begin(), contactTriIndices.end(), c.triangleIndex) == contactTriIndices.end())
            contactTriIndices.push_back(c.triangleIndex);
    }
}

void resolveContactsIterative(Player& player, const std::vector<Triangle>& triangles,
                              ContactState& state) {
    auto start = std::chrono::high_resolution_clock::now();
    gContactIterations = 0;
    gMaxDepth = 0.0f;
    if (state.contacts.empty()) return;

    std::vector<int> contactTriIndices;
    for (const auto& c : state.contacts)
        contactTriIndices.push_back(c.triangleIndex);

    for (int iter = 0; iter < 5; ++iter) {
        float maxDepth;
        int deepestIdx = findDeepestContact(state, maxDepth);
        if (deepestIdx < 0 || maxDepth < 0.000001f)
            break;
        gContactIterations = iter + 1;
        if (maxDepth > gMaxDepth) gMaxDepth = maxDepth;
        player.position += state.contacts[deepestIdx].normal * maxDepth;
        recollectContactTriangles(player, triangles, contactTriIndices, state);
        if (state.contacts.empty())
            break;
    }
    auto end = std::chrono::high_resolution_clock::now();
    gProfile.resolveTimeUs += (int)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    gProfile.depenetrationIters += gContactIterations;
}

void buildCollisionGrid(const std::vector<Triangle>& triangles, float cellSize) {
    gGrid.build(triangles, cellSize);
}

void clearCollisionGrid() {
    gGrid.clear();
}

const CollisionProfile& getCollisionProfile() {
    return gProfile;
}

void resetCollisionProfile() {
    gProfile = CollisionProfile();
}

void printCollisionProfile() {
    printf("[PHYSICS] total=%.3fms collect=%dus resolve=%dus "
           "tris=%d contacts=%d iters=%d queries=%d\n",
           gProfile.totalTimeMs,
           gProfile.collectTimeUs,
           gProfile.resolveTimeUs,
           gProfile.trianglesTested,
           gProfile.contactsGenerated,
           gProfile.depenetrationIters,
           gProfile.queriesPerFrame);
}
