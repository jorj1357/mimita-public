// // C:\important\quiet\n\mimita-public\mimita-public\src\debug\archive\debug-visuals.cpp
// // dec 24 2025
// /**
//  * purpose
//  * so we CAN FINALLT SEE
//  * WHATTHE HECKS GOING ON 
//  * BC I DIDNT HAVE THESE AT ALL BEFORE
//  * jan 30 2026 
//  * RENDERER.CPP DOES THE RENDERING
//  * THIS JUST GIVES IT THE TOOLS TO DO SO
//  */

//  // C:\important\quiet\n\mimita-public\mimita-public\src\debug\archive\debug-visuals.cpp
// // dec 24 2025

// #include <glad/glad.h>
// #include <GLFW/glfw3.h>
// #include <glm/glm.hpp>
// #include <cstdio>
// #include "physics/config.h"

// #include "debug-visuals.h"
// #include "entities/player.h"
// #include "camera.h"

// #include <vector>
// #include "world/world.h"
// #include "physics/physics-types.h"

// // jan 30 2026 i just learned
// // extern means we are importing some other global from another file
// // chatgpt said this lives in maincpp but i dont know 
// // nevermind im not using it 
// // extern GLuint worldShader;

// namespace {
//     bool gEnabled = false;
//     bool last0 = false;
//     GLFWwindow* gWindow = nullptr;
//     DebugColors gColors;
// }

// void DebugVis::init(GLFWwindow* win) {
//     gWindow = win;
// }

// void DebugVis::update() {
//     if (!gWindow) return;

//     bool key0 = glfwGetKey(gWindow, GLFW_KEY_0) == GLFW_PRESS;
//     if (key0 && !last0) {
//         gEnabled = !gEnabled;
//         printf("DEBUG TOGGLED: %d\n", gEnabled);
//     }
//     last0 = key0;
// }

// bool DebugVis::enabled() {
//     return gEnabled;
// }

// const DebugColors& DebugVis::colors() {
//     return gColors;
// }

// static GLuint lineVAO = 0, lineVBO = 0;

// void drawLine(const glm::vec3& a,
//               const glm::vec3& b,
//               const glm::vec3& color)
// {
//     if (!lineVAO) {
//         glGenVertexArrays(1, &lineVAO);
//         glGenBuffers(1, &lineVBO);
//     }

//     glm::vec3 pts[2] = { a, b };

//     glBindVertexArray(lineVAO);
//     glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_DYNAMIC_DRAW);

//     glVertexAttribPointer(0,3,GL_FLOAT,0,0,(void*)0);
//     glEnableVertexAttribArray(0);

//     glUseProgram(worldShader);
//     glUniform1i(glGetUniformLocation(worldShader,"uUseColor"), 1);
//     glUniform3fv(glGetUniformLocation(worldShader,"uColor"),1,&color.x);
//     glDrawArrays(GL_LINES, 0, 2);
// }

// void drawOBB(const OBB& box, glm::vec3 color)
// {
//     glm::vec3 hs = box.halfSize;

//     glm::vec3 local[8] = {
//         {-hs.x,-hs.y,-hs.z}, {hs.x,-hs.y,-hs.z},
//         {hs.x, hs.y,-hs.z}, {-hs.x, hs.y,-hs.z},
//         {-hs.x,-hs.y, hs.z}, {hs.x,-hs.y, hs.z},
//         {hs.x, hs.y, hs.z}, {-hs.x, hs.y, hs.z},
//     };

//     glm::vec3 world[8];
//     for (int i = 0; i < 8; i++) {
//         world[i] = box.center + glm::vec3(box.orientation * glm::vec4(local[i],1));
//     }

//     int edges[12][2] = {
//         {0,1},{1,2},{2,3},{3,0},
//         {4,5},{5,6},{6,7},{7,4},
//         {0,4},{1,5},{2,6},{3,7}
//     };

//     for (auto& e : edges)
//         drawLine(world[e[0]], world[e[1]], color);
// }

// // void drawCapsule(const Player& p, glm::vec3 color)
// // {
// //     float r = PLAYER_RADIUS;
// //     glm::vec3 a = p.pos + glm::vec3(0,0,r);
// //     glm::vec3 b = p.pos + glm::vec3(0,0,PLAYER_HEIGHT - r);

// //     const int slices = 16;
// //     const float PI = 3.14159265f;

// //     for (int i = 0; i < slices; i++) {
// //         float t0 = i * 2*PI / slices;
// //         float t1 = (i+1) * 2*PI / slices;

// //         glm::vec3 d0 = {cos(t0)*r, sin(t0)*r, 0};
// //         glm::vec3 d1 = {cos(t1)*r, sin(t1)*r, 0};

// //         // bottom ring
// //         drawLine(a + d0, a + d1, color);

// //         // top ring
// //         drawLine(b + d0, b + d1, color);

// //         // side lines
// //         drawLine(a + d0, b + d0, color);
// //     }
// // }

// // feb 2 2026 testing 

// // void drawCapsule(const Player& p, glm::vec3 color)
// // {
// //     Capsule c = p.getCapsule();
// //     glm::vec3 a = c.a;
// //     glm::vec3 b = c.b;
// //     float r = c.r;

// //     const int slices = 16;
// //     const float PI = 3.14159265f;

// //     for (int i = 0; i < slices; i++) {
// //         float t0 = i * 2*PI / slices;
// //         float t1 = (i+1) * 2*PI / slices;

// //         glm::vec3 d0 = {cos(t0)*r, sin(t0)*r, 0};
// //         glm::vec3 d1 = {cos(t1)*r, sin(t1)*r, 0};

// //         drawLine(a + d0, a + d1, color); // bottom ring
// //         drawLine(b + d0, b + d1, color); // top ring
// //         drawLine(a + d0, b + d0, color); // side line
// //     }
// // }

// // feb 3 2026 draw better hihtbox for capsule idk
// void drawCapsule(const Player& p, glm::vec3 color)
// {
//     Capsule c = p.getCapsule();
//     glm::vec3 a = c.a;
//     glm::vec3 b = c.b;
//     float r = c.r;

//     const int slices = 16;
//     const int hemiSteps = 8;
//     const float PI = 3.14159265f;

//     // cylinder rings
//     for (int i = 0; i < slices; i++) {
//         float t0 = (i / (float)slices) * 2*PI;
//         float t1 = ((i+1) / (float)slices) * 2*PI;

//         glm::vec3 d0(cos(t0)*r, sin(t0)*r, 0);
//         glm::vec3 d1(cos(t1)*r, sin(t1)*r, 0);

//         drawLine(a + d0, a + d1, color);
//         drawLine(b + d0, b + d1, color);
//         drawLine(a + d0, b + d0, color);
//     }

//     // bottom hemisphere
//     for (int j = 0; j < hemiSteps; j++) {
//         float v0 = (j / (float)hemiSteps) * (PI/2);
//         float v1 = ((j+1) / (float)hemiSteps) * (PI/2);

//         for (int i = 0; i < slices; i++) {
//             float t0 = (i / (float)slices) * 2*PI;
//             float t1 = ((i+1) / (float)slices) * 2*PI;

//             auto sph = [&](float v, float t) {
//                 return glm::vec3(
//                     cos(t)*sin(v)*r,
//                     sin(t)*sin(v)*r,
//                     -cos(v)*r
//                 );
//             };

//             drawLine(a + sph(v0,t0), a + sph(v0,t1), color);
//             drawLine(a + sph(v0,t0), a + sph(v1,t0), color);
//         }
//     }

//     // top hemisphere
//     for (int j = 0; j < hemiSteps; j++) {
//         float v0 = (j / (float)hemiSteps) * (PI/2);
//         float v1 = ((j+1) / (float)hemiSteps) * (PI/2);

//         for (int i = 0; i < slices; i++) {
//             float t0 = (i / (float)slices) * 2*PI;
//             float t1 = ((i+1) / (float)slices) * 2*PI;

//             auto sph = [&](float v, float t) {
//                 return glm::vec3(
//                     cos(t)*sin(v)*r,
//                     sin(t)*sin(v)*r,
//                     cos(v)*r
//                 );
//             };

//             drawLine(b + sph(v0,t0), b + sph(v0,t1), color);
//             drawLine(b + sph(v0,t0), b + sph(v1,t0), color);
//         }
//     }
// }

// void drawWorldHitboxes(const std::vector<Block*>& blocks)
// {
//     for (Block* b : blocks) {
//         OBB box;
//         box.center = b->pos;
//         box.halfSize = b->size * 0.5f;   // half extents
//         box.orientation = glm::mat4(b->rot);

//         drawOBB(box, DebugVis::colors().worldChunks);
//     }
// }

// // where the m agic happen jan 30 2026 
// void drawDebugStuff(const Player& player,
//                     const Camera& camera,
//                     const World& world)
// {
//     // Player capsule
//     drawCapsule(player, DebugVis::colors().playerCapsule);

//     std::vector<Block*> nearbyBlocks;
//     std::vector<Sphere*> nearbySpheres;
//     // stop feb 3 2026 commneted out bc phsics suck
//     // world.getNearby(player.pos, nearbyBlocks, nearbySpheres);

//     drawWorldHitboxes(nearbyBlocks);
    
//     // Look vector
//     // i dont rl want this for now jan 30 2026 
//     // this is just like velocity of where im going but its a line 
//     // drawLine(
//     //     camera.pos,
//     //     // front, not forward? not sure if this works jan 30 2026
//     //     camera.pos + camera.front * 5.0f,
//     //     DebugVis::colors().lookVector
//     // );
// }
