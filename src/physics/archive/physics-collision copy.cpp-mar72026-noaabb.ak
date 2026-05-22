// // C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-collision.cpp
// // feb 10 2026
// // Purpose:
// // - Handle ALL solid world collisions
// // - No slope logic
// // - No audio
// // - No input handling
// // - Pure positional correction + grounded detection
// //
// // Exposes:
// //   doCollisions(...)

// #include <algorithm>
// #include <cstdio>
// #include <vector>
// #include <cmath>
// #include <glm/glm.hpp>

// #include "physics/config.h"
// #include "world/world.h"
// #include "entities/player.h"

// // =====================================================
// // DEBUG TOGGLE
// // =====================================================
// #define PHYS_DEBUG_COLLISIONS 1

// #if PHYS_DEBUG_COLLISIONS
//     #define PHYS_LOG(...) std::printf(__VA_ARGS__)
// #else
//     #define PHYS_LOG(...)
// #endif

// // =====================================================
// // AABB helpers (TEMP until capsule collisions v3)
// // =====================================================

// struct AABB {
//     glm::vec3 min;
//     glm::vec3 max;
// };

// static inline AABB makePlayerAABB(const Player& p)
// {
//     glm::vec3 half(
//         PLAYER_WIDTH  * 0.5f,
//         PLAYER_DEPTH  * 0.5f,
//         PLAYER_HEIGHT * 0.5f
//     );

//     return { p.pos - half, p.pos + half };
// }

// static inline AABB makeBlockAABB(const Block& b)
// {
//     glm::vec3 half = b.size * 0.5f;
//     return { b.pos - half, b.pos + half };
// }

// static inline bool overlaps(const AABB& a, const AABB& b)
// {
//     return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
//            (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
//            (a.min.z <= b.max.z && a.max.z >= b.min.z);
// }

// // =====================================================
// // Overlap cleanup fallback
// // =====================================================

// static inline void resolveOverlap(
//     const AABB& block,
//     AABB& player,
//     Player& p,
//     bool& groundedOut,
//     const Block& b
// )
// {
//     float px1 = block.max.x - player.min.x;
//     float px2 = player.max.x - block.min.x;
//     float py1 = block.max.y - player.min.y;
//     float py2 = player.max.y - block.min.y;
//     float pz1 = block.max.z - player.min.z;
//     float pz2 = player.max.z - block.min.z;

//     float penX = std::min(px1, px2);
//     float penY = std::min(py1, py2);
//     float penZ = std::min(pz1, pz2);

//     constexpr float SLOP = 0.001f;

//     if (penX <= penY && penX <= penZ)
//     {
//         float centerPlayer = (player.min.x + player.max.x) * 0.5f;
//         float centerBlock  = (block.min.x  + block.max.x)  * 0.5f;

//         float push = (centerPlayer < centerBlock)
//             ? -(penX + SLOP)
//             :  ( penX + SLOP);

//         p.pos.x += push;
//         p.vel.x = 0.0f;

//         PHYS_LOG(
//             "[PHYS][COLLISION] X axis | push=%.4f | block=(%.2f %.2f %.2f)\n",
//             push, b.pos.x, b.pos.y, b.pos.z
//         );
//     }
//     else if (penY <= penZ)
//     {
//         float centerPlayer = (player.min.y + player.max.y) * 0.5f;
//         float centerBlock  = (block.min.y  + block.max.y)  * 0.5f;

//         float push = (centerPlayer < centerBlock)
//             ? -(penY + SLOP)
//             :  ( penY + SLOP);

//         p.pos.y += push;
//         p.vel.y = 0.0f;

//         PHYS_LOG(
//             "[PHYS][COLLISION] Y axis | push=%.4f | block=(%.2f %.2f %.2f)\n",
//             push, b.pos.x, b.pos.y, b.pos.z
//         );
//     }
//     else
//     {
//         float centerPlayer = (player.min.z + player.max.z) * 0.5f;
//         float centerBlock  = (block.min.z  + block.max.z)  * 0.5f;

//         float push = (centerPlayer < centerBlock)
//             ? -(penZ + SLOP)
//             :  ( penZ + SLOP);

//         p.pos.z += push;

//         if (push > 0.0f)
//         {
//             groundedOut = true;

//             if (p.vel.z < 0.0f)
//                 p.vel.z = 0.0f;

//             PHYS_LOG(
//                 "[PHYS][GROUND] Grounded on block=(%.2f %.2f %.2f)\n",
//                 b.pos.x, b.pos.y, b.pos.z
//             );
//         }
//         else
//         {
//             if (p.vel.z > 0.0f)
//                 p.vel.z = 0.0f;
//         }

//         PHYS_LOG(
//             "[PHYS][COLLISION] Z axis | push=%.4f | block=(%.2f %.2f %.2f)\n",
//             push, b.pos.x, b.pos.y, b.pos.z
//         );
//     }

//     player = makePlayerAABB(p);
// }

// // =====================================================
// // Swept AABB
// // move = displacement for THIS STEP, not raw velocity
// // =====================================================

// static bool sweptAABB(
//     const AABB& moving,
//     const glm::vec3& move,
//     const AABB& block,
//     float& hitTime,
//     glm::vec3& hitNormal
// )
// {
//     glm::vec3 invEntry;
//     glm::vec3 invExit;

//     if (move.x > 0.0f) {
//         invEntry.x = block.min.x - moving.max.x;
//         invExit.x  = block.max.x - moving.min.x;
//     } else {
//         invEntry.x = block.max.x - moving.min.x;
//         invExit.x  = block.min.x - moving.max.x;
//     }

//     if (move.y > 0.0f) {
//         invEntry.y = block.min.y - moving.max.y;
//         invExit.y  = block.max.y - moving.min.y;
//     } else {
//         invEntry.y = block.max.y - moving.min.y;
//         invExit.y  = block.min.y - moving.max.y;
//     }

//     if (move.z > 0.0f) {
//         invEntry.z = block.min.z - moving.max.z;
//         invExit.z  = block.max.z - moving.min.z;
//     } else {
//         invEntry.z = block.max.z - moving.min.z;
//         invExit.z  = block.min.z - moving.max.z;
//     }

//     glm::vec3 entry;
//     glm::vec3 exit;

//     entry.x = (std::fabs(move.x) < ALMOST_ZERO) ? -INFINITY : invEntry.x / move.x;
//     exit.x  = (std::fabs(move.x) < ALMOST_ZERO) ?  INFINITY : invExit.x  / move.x;

//     entry.y = (std::fabs(move.y) < ALMOST_ZERO) ? -INFINITY : invEntry.y / move.y;
//     exit.y  = (std::fabs(move.y) < ALMOST_ZERO) ?  INFINITY : invExit.y  / move.y;

//     entry.z = (std::fabs(move.z) < ALMOST_ZERO) ? -INFINITY : invEntry.z / move.z;
//     exit.z  = (std::fabs(move.z) < ALMOST_ZERO) ?  INFINITY : invExit.z  / move.z;

//     float entryTime = std::max(std::max(entry.x, entry.y), entry.z);
//     float exitTime  = std::min(std::min(exit.x,  exit.y),  exit.z);

//     if (entryTime > exitTime || entryTime < 0.0f || entryTime > 1.0f)
//         return false;

//     hitTime = entryTime;

//     if (entryTime == entry.x)
//         hitNormal = { move.x > 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
//     else if (entryTime == entry.y)
//         hitNormal = { 0.0f, move.y > 0.0f ? -1.0f : 1.0f, 0.0f };
//     else
//         hitNormal = { 0.0f, 0.0f, move.z > 0.0f ? -1.0f : 1.0f };

//     return true;
// }

// // we are A CAPSULE NOW MAR 7 2026 NOT A BOX

// static bool capsuleVsBlock(
//     const Capsule& cap,
//     const AABB& block,
//     glm::vec3& correction,
//     bool& grounded
// )
// {
//     glm::vec3 closest;

//     closest.x = glm::clamp(cap.a.x, block.min.x, block.max.x);
//     closest.y = glm::clamp(cap.a.y, block.min.y, block.max.y);
//     closest.z = glm::clamp(cap.a.z, block.min.z, block.max.z);

//     glm::vec3 delta = cap.a - closest;
//     float dist2 = glm::dot(delta, delta);

//     float r = cap.r;

//     if (dist2 > r*r)
//         return false;

//     float dist = sqrtf(dist2);

//     glm::vec3 normal;

//     if (dist > 0.00001f)
//         normal = delta / dist;
//     else
//         normal = {0,0,1};

//     float penetration = r - dist;

//     correction = normal * penetration;

//     if (normal.z > 0.5f)
//         grounded = true;

//     return true;
// }

// // =====================================================
// // PUBLIC ENTRY
// // =====================================================

// void doCollisions(
//     Player& p,
//     const World& world,
//     bool& groundedThisFrame,
//     float dt
// )
// {
//     groundedThisFrame = false;

//     AABB playerAABB = makePlayerAABB(p);
//     glm::vec3 move = p.vel * dt;

//     // gather blocks near start and end of this move
//     std::vector<Block*> nearbyBlocksA;
//     std::vector<Sphere*> nearbySpheresA;
//     std::vector<Block*> nearbyBlocksB;
//     std::vector<Sphere*> nearbySpheresB;

//     world.getNearby(p.pos,        nearbyBlocksA, nearbySpheresA);
//     world.getNearby(p.pos + move, nearbyBlocksB, nearbySpheresB);

//     std::vector<Block*> nearbyBlocks = nearbyBlocksA;
//     for (Block* b : nearbyBlocksB)
//     {
//         if (std::find(nearbyBlocks.begin(), nearbyBlocks.end(), b) == nearbyBlocks.end())
//             nearbyBlocks.push_back(b);
//     }

//     // sweep + slide iterations
//     for (int i = 0; i < 4; i++)
//     {
//         float earliest = 1.0f;
//         glm::vec3 hitNormal(0.0f);

//         for (Block* b : nearbyBlocks)
//         {
//             if (!b || b->isSlope)
//                 continue;

//             AABB ba = makeBlockAABB(*b);

//             float t = 1.0f;
//             glm::vec3 normal(0.0f);

//             if (sweptAABB(playerAABB, move, ba, t, normal))
//             {
//                 if (t < earliest)
//                 {
//                     earliest = t;
//                     hitNormal = normal;
//                 }
//             }
//         }

//         // move to first hit point (or full move if none)
//         glm::vec3 stepMove = move * earliest;
//         p.pos += stepMove;
//         playerAABB = makePlayerAABB(p);

//         // if nothing hit, done
//         if (earliest >= 1.0f)
//             break;

//         // remaining movement after reaching hit point
//         move -= stepMove;

//         // grounding / ceiling handling
//         if (hitNormal.z > 0.0f)
//         {
//             groundedThisFrame = true;
//             if (p.vel.z < 0.0f)
//                 p.vel.z = 0.0f;
//         }
//         else if (hitNormal.z < 0.0f)
//         {
//             if (p.vel.z > 0.0f)
//                 p.vel.z = 0.0f;
//         }

//         // slide remaining move along the surface
//         float vn = glm::dot(move, hitNormal);
//         if (vn < 0.0f)
//             move -= hitNormal * vn;

//         // tiny nudge out of the surface to avoid re-hitting the exact same plane
//         p.pos += hitNormal * 0.0005f;
//         playerAABB = makePlayerAABB(p);
//     }

//     // fallback overlap cleanup for tiny penetrations / resting contact
//     Capsule cap = p.getCapsule();

//     for (Block* b : nearbyBlocks)
//     {
//         if (!b || b->isSlope)
//             continue;

//         AABB ba = makeBlockAABB(*b);

//         glm::vec3 correction;
//         bool grounded = false;

//         if (capsuleVsBlock(cap, ba, correction, grounded))
//         {
//             p.pos += correction;

//             if (grounded)
//             {
//                 groundedThisFrame = true;

//                 if (p.vel.z < 0)
//                     p.vel.z = 0;
//             }

//             cap = p.getCapsule();
//         }
//     }
// }