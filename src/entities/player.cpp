// C:\important\go away v5\s\mimita-v5\src\entities\player.cpp

// make sure GLAD comes first
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "player.h"
#include <algorithm>
#include "../camera.h"
#include <glm/gtc/matrix_transform.hpp>

// dec 2 2025 idk where put this todo 
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
    pos = glm::vec3(0.0f, 30.0f, 0.0f);
    vel = glm::vec3(0.0f);
    onGround = false;
    // todo NO MORE HARD CODINGGGGGG dec 2 2025
    // this is hitbox actual definition i think i dont know
    hitboxSize = glm::vec3(1.2f, 1.8f, 0.4f);
}

void Player::reset() {
    // default reset
    // pos = glm::vec3(0.0f, 10.0f, 0.0f);
    // higher reset
    pos = glm::vec3(0.0f, 50.0f, 0.0f);

    vel = glm::vec3(0.0f);
    onGround = false;
}

void Player::jump(float strength) {
    vel.y = strength;
    onGround = false;
}

void Player::applyGravity(float gravity, float dt) {
    vel.y += gravity * dt;
    pos.y += vel.y * dt;
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
    rot = glm::rotate(rot, glm::radians(-yaw), glm::vec3(0,1,0));
    box.orientation = rot;

    return box;
}

void Player::render(GLuint shaderProgram, GLuint vao, int vertCount,
                    const glm::mat4& view, const glm::mat4& proj,
                    const Camera& camera, GLuint tex) {
    glUseProgram(shaderProgram);

    // shared base transform (root / HumanoidRootPart)
    float yaw = camera.yaw;
    rootTransform = glm::mat4(1.0f);
    rootTransform = glm::translate(rootTransform, pos - glm::vec3(0, hitboxSize.y * 0.5f, 0));
    rootTransform = glm::rotate(rootTransform, glm::radians(-yaw), glm::vec3(0, 1, 0));

    // ---------------- HITBOX (red wireframe) ----------------
    initHitboxVAO();

    glm::mat4 hitboxModel = glm::mat4(1.0f);
    hitboxModel = glm::translate(hitboxModel, pos + hitboxOffset);
    hitboxModel = glm::rotate(hitboxModel, glm::radians(-yaw), glm::vec3(0,1,0));
    hitboxModel = glm::scale(hitboxModel, hitboxSize);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &hitboxModel[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 0.0f, 0.0f);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    glBindVertexArray(hitboxVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    // ---------------- PLAYER MESH ----------------
    glm::mat4 meshModel = glm::mat4(1.0f);
    meshModel = glm::translate(meshModel, pos + meshOffset);
    meshModel = glm::rotate(meshModel, glm::radians(-yaw), glm::vec3(0, 1, 0));
    meshModel = glm::scale(meshModel, meshScale);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &meshModel[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 1.0f, 1.0f);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);
    glBindVertexArray(0);

    glUseProgram(0);
}
