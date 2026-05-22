// // C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-triangle.cpp
// // dec 16 2025
// /**
//  * purpose
//  * this has all the SB that we dont want in the phsics cpp file 
//  * so phsics just calls the functions 
//  * and its all nice and readable 
//  */

// #include "collision-capsule-triangle.h"
// #include "physics/config.h"
// #include <glm/glm.hpp>
// #include <glm/gtc/epsilon.hpp>
// #include <cstdio>

// // ----- helpers -----

// static glm::vec3 closestPtPointTriangle(
//     const glm::vec3& p,
//     const glm::vec3& a,
//     const glm::vec3& b,
//     const glm::vec3& c)
// {
//     // Real-Time Collision Detection (Christer Ericson) style
//     glm::vec3 ab = b - a;
//     glm::vec3 ac = c - a;
//     glm::vec3 ap = p - a;

//     float d1 = glm::dot(ab, ap);
//     float d2 = glm::dot(ac, ap);
//     if (d1 <= 0.0f && d2 <= 0.0f) return a;

//     glm::vec3 bp = p - b;
//     float d3 = glm::dot(ab, bp);
//     float d4 = glm::dot(ac, bp);
//     if (d3 >= 0.0f && d4 <= d3) return b;

//     float vc = d1 * d4 - d3 * d2;
//     if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
//         float v = d1 / (d1 - d3);
//         return a + v * ab;
//     }

//     glm::vec3 cp = p - c;
//     float d5 = glm::dot(ab, cp);
//     float d6 = glm::dot(ac, cp);
//     if (d6 >= 0.0f && d5 <= d6) return c;

//     float vb = d5 * d2 - d1 * d6;
//     if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
//         float w = d2 / (d2 - d6);
//         return a + w * ac;
//     }

//     float va = d3 * d6 - d5 * d4;
//     if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
//         float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
//         return b + w * (c - b);
//     }

//     // inside face region
//     float denom = 1.0f / (va + vb + vc);
//     float v = vb * denom;
//     float w = vc * denom;
//     return a + ab * v + ac * w;
// }

// static void closestPtSegmentSegment(
//     const glm::vec3& p1, const glm::vec3& q1,
//     const glm::vec3& p2, const glm::vec3& q2,
//     float& s, float& t,
//     glm::vec3& c1, glm::vec3& c2)
// {
//     glm::vec3 d1 = q1 - p1;
//     glm::vec3 d2 = q2 - p2;
//     glm::vec3 r  = p1 - p2;
//     float a = glm::dot(d1, d1);
//     float e = glm::dot(d2, d2);
//     float f = glm::dot(d2, r);

//     if (a <= 1e-8f && e <= 1e-8f) {
//         s = t = 0.0f;
//         c1 = p1;
//         c2 = p2;
//         return;
//     }
//     if (a <= 1e-8f) {
//         s = 0.0f;
//         t = glm::clamp(f / e, 0.0f, 1.0f);
//     } else {
//         float c = glm::dot(d1, r);
//         if (e <= 1e-8f) {
//             t = 0.0f;
//             s = glm::clamp(-c / a, 0.0f, 1.0f);
//         } else {
//             float b = glm::dot(d1, d2);
//             float denom = a * e - b * b;
//             if (denom != 0.0f) s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
//             else s = 0.0f;
//             t = (b * s + f) / e;

//             if (t < 0.0f) { t = 0.0f; s = glm::clamp(-c / a, 0.0f, 1.0f); }
//             else if (t > 1.0f) { t = 1.0f; s = glm::clamp((b - c) / a, 0.0f, 1.0f); }
//         }
//     }

//     c1 = p1 + d1 * s;
//     c2 = p2 + d2 * t;
// }

// // ----- main -----

// glm::vec3 collideCapsuleTriangleMove(
//     const Capsule& capAtStart,
//     const glm::vec3& move,
//     const Triangle& tri,
//     bool& onGround)
// {
//     // We test the capsule at its END position this frame
//     Capsule cap = capAtStart;
//     cap.a += move;
//     cap.b += move;

//     // Find closest points between capsule segment and triangle (face + edges)
//     glm::vec3 bestSegP(0), bestTriP(0);
//     float bestD2 = 1e30f;

//     // 1) closest from segment endpoints to triangle face
//     {
//         glm::vec3 p = closestPtPointTriangle(cap.a, tri.a, tri.b, tri.c);
//         float d2 = glm::dot(cap.a - p, cap.a - p);
//         if (d2 < bestD2) { bestD2 = d2; bestSegP = cap.a; bestTriP = p; }
//     }
//     {
//         glm::vec3 p = closestPtPointTriangle(cap.b, tri.a, tri.b, tri.c);
//         float d2 = glm::dot(cap.b - p, cap.b - p);
//         if (d2 < bestD2) { bestD2 = d2; bestSegP = cap.b; bestTriP = p; }
//     }

//     // 2) closest between capsule segment and each triangle edge
//     const glm::vec3 ea[3] = { tri.a, tri.b, tri.c };
//     const glm::vec3 eb[3] = { tri.b, tri.c, tri.a };
//     for (int i = 0; i < 3; i++) {
//         float s, t;
//         glm::vec3 c1, c2;
//         closestPtSegmentSegment(cap.a, cap.b, ea[i], eb[i], s, t, c1, c2);
//         float d2 = glm::dot(c1 - c2, c1 - c2);
//         if (d2 < bestD2) { bestD2 = d2; bestSegP = c1; bestTriP = c2; }
//     }

//     float r2 = cap.r * cap.r;
//     if (bestD2 >= r2) return move; // no overlap

//     float dist = sqrtf(glm::max(bestD2, 1e-12f));
//     float pen  = cap.r - dist;

//     // Push direction: from triangle -> capsule
//     glm::vec3 n;
//     if (dist > 1e-6f) n = (bestSegP - bestTriP) / dist;
//     else {
//         // fallback to triangle normal if perfectly on it
//         glm::vec3 tn = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
//         n = tn;
//     }

//     // ground check (your constant name is misleading; you want "how upward is the normal")
//     if (n.y > MAX_SLOPE_ANGLE) onGround = true;

//     // Resolve penetration + slide:
//     glm::vec3 out = move + n * pen;               // push out of the triangle
//     float into = glm::dot(out, n);
//     if (into < 0.0f) out -= n * into;             // remove movement into the surface

//     // idk where to put this dec 17 2025
//     static int hitCount = 0;
//     hitCount++;
//     if ((hitCount % 200) == 0) {
//         printf("[COL] hit pen=%.4f n(%.2f %.2f %.2f)\n",
//             (double)pen, (double)n.x, (double)n.y, (double)n.z);
//     }

//     return out;
// }
