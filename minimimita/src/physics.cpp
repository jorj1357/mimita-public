#include "physics.h"
#include "collision-grid.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>

glm::vec3 closestPtPointTriangle(glm::vec3 p, const Triangle& tri) {
    const glm::vec3 ab = tri.b - tri.a;
    const glm::vec3 ac = tri.c - tri.a;
    const glm::vec3 ap = p - tri.a;

    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return tri.a;

    const glm::vec3 bp = p - tri.b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return tri.b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return tri.a + v * ab;
    }

    const glm::vec3 cp = p - tri.c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return tri.c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return tri.a + w * ac;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return tri.b + w * (tri.c - tri.b);
    }

    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return tri.a + v * ab + w * ac;
}

glm::vec3 closestPtPointSegment(glm::vec3 p, glm::vec3 a, glm::vec3 b) {
    const glm::vec3 ab = b - a;
    float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    return a + t * ab;
}

bool capsuleTriangleCollision(glm::vec3 capA, glm::vec3 capB, float radius,
                              const Triangle& tri, Contact& contact) {
    const float EPS = 1e-8f;

    glm::vec3 ptOnSeg = closestPtPointSegment(tri.a, capA, capB);
    glm::vec3 ptOnTri = closestPtPointTriangle(ptOnSeg, tri);

    ptOnSeg = closestPtPointSegment(ptOnTri, capA, capB);
    ptOnTri = closestPtPointTriangle(ptOnSeg, tri);

    const glm::vec3 diff = ptOnSeg - ptOnTri;
    float distSq = glm::length2(diff);

    if (distSq > radius * radius)
        return false;

    float dist = glm::sqrt(distSq);
    glm::vec3 n;
    if (dist < EPS) {
        n = tri.normal;
        if (glm::dot(n, ptOnSeg - capA) < 0.0f)
            n = -n;
    } else {
        n = diff / dist;
    }

    contact.point = ptOnTri + n * (radius - dist) * 0.5f;
    contact.normal = n;
    contact.depth = radius - dist;

    float upDot = glm::dot(n, WORLD_UP);
    if (upDot > MAX_GROUND_ANGLE)
        contact.side = Contact::FLOOR;
    else if (upDot < -MAX_GROUND_ANGLE)
        contact.side = Contact::CEILING;
    else
        contact.side = Contact::WALL;

    return true;
}

static void slideVelocity(Player& player, const ContactState& state) {
    if (!state.touchingWall && !state.touchingFloor && !state.touchingCeiling)
        return;
    for (const auto& contact : state.contacts) {
        float vDotN = glm::dot(player.velocity, contact.normal);
        if (vDotN < 0.0f)
            player.velocity -= vDotN * contact.normal;
    }
}

void computeWishDir(InputState& input, const Camera& camera) {
    glm::vec3 fwd = camera.forward2D();
    glm::vec3 right = camera.right2D();
    input.wishDir = glm::vec3(0.0f);
    if (input.w) input.wishDir += fwd;
    if (input.s) input.wishDir -= fwd;
    if (input.d) input.wishDir += right;
    if (input.a) input.wishDir -= right;
}

void updatePlayer(Player& player, const InputState& input,
                  const std::vector<Triangle>& triangles, float dt) {
    auto frameStart = std::chrono::high_resolution_clock::now();

    const float WALK_SPEED = 5.0f;
    glm::vec3 wishDir = input.wishDir;
    if (glm::length2(wishDir) > 0.0f)
        wishDir = glm::normalize(wishDir) * WALK_SPEED;
    player.velocity.x = wishDir.x;
    player.velocity.y = wishDir.y;

    if (input.space && player.jumpAvailable) {
        player.velocity.z = 5.0f;
        player.jumpAvailable = false;
    }

    player.velocity.z += GRAVITY * dt;
    player.position += player.velocity * dt;

    ContactState state;
    collectContacts(player, triangles, state);
    resolveContactsIterative(player, triangles, state);
    slideVelocity(player, state);
    player.contacts = state;

    if (state.contacts.size() > 0)
        player.jumpAvailable = true;
    if (state.touchingFloor && player.velocity.z < 0.0f)
        player.velocity.z = 0.0f;
    if (player.position.z < -50.0f) {
        player.velocity = glm::vec3(0.0f);
        player.position.z = 0.0f;
    }

    auto frameEnd = std::chrono::high_resolution_clock::now();
    CollisionProfile& prof = const_cast<CollisionProfile&>(getCollisionProfile());
    prof.totalTimeMs += std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
}
