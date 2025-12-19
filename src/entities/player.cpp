// C:\important\go away v5\s\mimita-v5\src\entities\player.cpp

// dec 16 2025 it was so m ess i clean it 

// make sure GLAD comes first
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include "../map/map_common.h"   
#include "player.h"
#include "../camera.h"
#include <glm/gtc/matrix_transform.hpp>

// ---------- static hitbox VAO (init once) ----------
static GLuint hitboxVAO = 0;
static GLuint hitboxVBO = 0;

static void initHitboxVAO()
{
    if (hitboxVAO != 0) return;

    float verts[] = {
        // bottom
        -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,
         0.5,-0.5,-0.5,  0.5,-0.5, 0.5,
         0.5,-0.5, 0.5, -0.5,-0.5, 0.5,
        -0.5,-0.5, 0.5, -0.5,-0.5,-0.5,

        // top
        -0.5,0.5,-0.5,  0.5,0.5,-0.5,
         0.5,0.5,-0.5,  0.5,0.5, 0.5,
         0.5,0.5, 0.5, -0.5,0.5, 0.5,
        -0.5,0.5, 0.5, -0.5,0.5,-0.5,

        // verticals
        -0.5,-0.5,-0.5, -0.5,0.5,-0.5,
         0.5,-0.5,-0.5,  0.5,0.5,-0.5,
         0.5,-0.5, 0.5,  0.5,0.5, 0.5,
        -0.5,-0.5, 0.5, -0.5,0.5, 0.5
    };

    glGenVertexArrays(1, &hitboxVAO);
    glGenBuffers(1, &hitboxVBO);

    glBindVertexArray(hitboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hitboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

    glBindVertexArray(0);
}

// At the very top of player.cpp (below your includes), add:
#include "../renderer/renderer.h"
extern Renderer* gRenderer;

/*
todo no more hardcoding spawn positions
put them in config.h probably
*/
Player::Player() {
    // dec 3 2025 todo 
    // put this , this player init spawn position
    // in a centralized physics/game settings config file
    // and even better an object in the world that is the spawn pos

    // default spawn
    // pos = glm::vec3(0.0f, 2.0f, 0.0f);
    // higher spawn
    // pos = glm::vec3(0.0f, 30.0f, 0.0f);
    // vel = glm::vec3(0.0f);
    // onGround = false;
    // todo NO MORE HARD CODINGGGGGG dec 2 2025
    // this is hitbox actual definition i think i dont know
    // dec 3 2025 todo is THIS hitbox size or is the other one hitbox size
    // 1 blender unit = 1 meter = 1 mimita unit
    // so 25 units = 25 meters,
    // and techinically height should be 1.8m but oh well scaling 
    // default/small
    // hitboxSize = glm::vec3(1.2f, 1.8f, 0.4f);
    // default v2
    // dec 3 2025 
    // i dont know why but this is a rly accurate hitbox size
    // for the v3 character mesh
    // the scaling is weird and sucksb ut whatever 
    // i think 1 meter is 0.5f or
    // theres some 2x mult scaling IDK WAHTEVER 

    // ------------------------------------------------
    // --- fake stupid bullshit red hitbox start ---
    // ------------------------------------------------

    /** 
     * dec 12 2025 hitbox test
     * removed this hitbox size thing bc i think its the
     * red wireframe?
     * and it doenst even do anuthing so whatever  
     */

    // dec 12 2025 default
    // hitboxSize = glm::vec3(0.4f, 3.3f, 1.4f);

    // dec 12 2025 small to test if it does anything
    // hitboxSize = glm::vec3(0.1f, 1.0f, 0.5f);

    // dec 12 2025 it does nothing so i made it basicall 0 
    hitboxSize = glm::vec3(0.001f, 0.001f, 0.001f);

    // ------------------------------------------------
    // --- fake stupid bullshit red hitbox end ---
    // ------------------------------------------------

    // hitbox scale test dec 3 2025
    // hitboxSize = glm::vec3(0.5f, 1.7f, 0.8f);

    // big 1
   //  hitboxSize = glm::vec3(4.0f, 6.0f, 4.0f);

    // bigger
    // hitboxSize = glm::vec3(8.0f, 12.0f, 8.0f);
    // compute offset AFTER size is correct
    // dec 19 2025 i dont think this does antthing? 
    hitboxOffset = glm::vec3(0.0f, 0.0f, hitboxSize.z * 0.5f);
    // dec 3 2025 test no 
    // hitboxOffset = glm::vec3(0.0f, 0.0f, 0.0f);

    // dec 18 2025 THIS IS THE INITIAL SPAWN HEIGHT
    pos = glm::vec3(0.0f, 0.0f, 30.0f);
    vel = glm::vec3(0.0f);
    onGround = false;


}

// dec 3 2025 todo where to put
// also this should find the bounding of whatever mesh we have as our character 
// static void computeMeshBounds(const Mesh& mesh, glm::vec3& minOut, glm::vec3& maxOut)
// dec 4 2025 todo do we even use this function
// {
//     if (mesh.verts.empty())
//     {
//         minOut = maxOut = glm::vec3(0);
//         return;
//     }

//     glm::vec3 mn = mesh.verts[0].pos;
//     glm::vec3 mx = mn;

//     for (const auto& v : mesh.verts)
//     {
//         mn.x = std::min(mn.x, v.pos.x);
//         mn.y = std::min(mn.y, v.pos.y);
//         mn.z = std::min(mn.z, v.pos.z);

//         mx.x = std::max(mx.x, v.pos.x);
//         mx.y = std::max(mx.y, v.pos.y);
//         mx.z = std::max(mx.z, v.pos.z);
//     }

//     minOut = mn;
//     maxOut = mx;
// }

void Player::reset() {
    // default reset
    // pos = glm::vec3(0.0f, 10.0f, 0.0f);
    // higher reset
    // z up , dec 19 2025 
    pos = glm::vec3(0.0f, 0.0f, 50.0f);

    vel = glm::vec3(0.0f);
    onGround = false;
}

void Player::jump(float strength) {
    vel.z = strength;
    onGround = false;
}

void Player::applyGravity(float gravity, float dt) {
    vel.z += gravity * dt;
    pos.z += vel.z * dt;
}

void Player::move(const glm::vec3& dir, float speed, float dt) {
    pos += dir * speed * dt;
}

// dec 2 2025 todo idk where put this 
OBB Player::getOBB() const {
    OBB box;

    // center = position + offset
    box.center = pos + hitboxOffset;

    // half sizes
    box.halfSize = hitboxSize * 0.5f;

    // orientation from player yaw
    glm::mat4 rot = glm::mat4(1.0f);
    // rotate around z axis which is up 
    rot = glm::rotate(rot, glm::radians(-yaw), glm::vec3(0,0,1));
    box.orientation = rot;

    return box;
}

void Player::render(GLuint shaderProgram, GLuint vao, int vertCount,
                    const glm::mat4& view, const glm::mat4& proj,
                    const Camera& camera, GLuint tex) {
    glUseProgram(shaderProgram);

    // shared base transform (root / HumanoidRootPart)
    // dec 19 2025 updating to z = up 
    float yaw = this->yaw;
    // yaw is positive  for z up = up update 
    rootTransform = glm::rotate(rootTransform, glm::radians(yaw), glm::vec3(0,0,1));
    rootTransform = glm::translate(
        rootTransform,
        pos - glm::vec3(0, 0, hitboxSize.z * 0.5f) // todo 0.5f mult why? do we even use this dec 19 2025 
    );

    // ---------------- HITBOX (red wireframe) ----------------
    /**
     * dec 12 2025 todo
     * apparently the hitbox red wireframe hitbox doesnt work 
     * and its handled in physics.cpp ? 
     * so its just visual and doesnt even whatever explodes mself 
     */
    initHitboxVAO();

    // i dont think we use any of tihs dec 19 2025
    // collisions and stuff are in physics.cpp but should be here
    // but we do that later 
    glm::mat4 hitboxModel = glm::mat4(1.0f);
    hitboxModel = glm::translate(hitboxModel, pos + hitboxOffset);
    // dec 19 2025 we do not use this at all bruh phase out 
    hitboxModel = glm::translate(hitboxModel, pos + hitboxOffset);
    // dec 19 2025 watch for yaw negative or positive
    hitboxModel = glm::rotate(hitboxModel, glm::radians(yaw), glm::vec3(0,0,1));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &hitboxModel[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 0.0f, 0.0f);

    // dec 3 2025 changed so hitbox dont get drawn over 
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glLineWidth(1.5f);

    glBindVertexArray(hitboxVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    // ---------------- PLAYER MESH ----------------
    glm::mat4 meshModel = glm::mat4(1.0f);

    // 1. move to world position
    meshModel = glm::translate(meshModel, pos + meshOffset);

    // 2. FIX THE MESH BASIS (upright) WORKING
    meshModel = glm::rotate(
        meshModel,
        glm::radians(-90.0f),
        glm::vec3(1, 0, 0)
    );

    // 3. FIX FORWARD DIRECTION (THIS IS THE MISSING PIECE)
    meshModel = glm::rotate(
        meshModel,
        // -180 facing backwards
        // 180 facing forward
        // but now we're 0,1,0 so ?
        // test 2, -180 radians, 0,1,0 vec3
        // test 2 -180 radians flips us upside down?
        // test 3 180 radians, 0,1,0
        // test 3 im rotating opposite, look right move left
        // test 4 180 radians, 0,0,1
        // test 4 180 radians, 0,0,1 = im upright, rotating about z, facing backwards
        // test 5 -180 radians, 0,0,1
        // test 5 im still facing backwards i think its rotation?
        glm::radians(-180.0f),
        glm::vec3(0, 0, 1)
    );

    // 4. APPLY GAMEPLAY YAW (about Z, because Z is up)
    meshModel = glm::rotate(
        meshModel,
        // test 6 -yaw i think makes us face forward?
        // test 6 u rotate right whewn moving left and vice versa, not fix
        // test 7 define radians as 180.f directl
        // test 7 im just facing one direction whole time not good 
        // test 8 cant be a single value, has to be yaw, but maybe i add 180.0f?
        // TEST 8 THAT WAS IT FALL 7 TIMES GET UP 8 
        glm::radians(yaw + 180.0f),
        // glm::radians(yaw),
        // 0,0,1 is idk default but not right
        // 0,1,0 means im rotating the right way, facing wrong
        // dec 19 2025 this is confirmed the right way i think , 0,1,0  
        glm::vec3(0, 1, 0)
    );

    // 5. scale
    meshModel = glm::scale(meshModel, meshScale);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &meshModel[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 1.0f, 1.0f);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);
    glBindVertexArray(0);

    glUseProgram(0);
}
