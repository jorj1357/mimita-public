// C:\important\go away v5\s\mimita-v5\src\entities\player.cpp

// make sure GLAD comes first
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "player.h"
#include <algorithm>
#include "../camera.h"
#include <glm/gtc/matrix_transform.hpp>

// At the very top of player.cpp (below your includes), add:
#include "../renderer/renderer.h"
extern Renderer* gRenderer;

/*
todo no more hardcoding spawn positions
put them in config.h probably
*/
Player::Player() {
    pos = glm::vec3(0.0f, 2.0f, 0.0f);
    vel = glm::vec3(0.0f);
    onGround = false;
    hitboxSize = glm::vec3(0.8f, 1.8f, 0.8f);
}

void Player::reset() {
    pos = glm::vec3(0.0f, 10.0f, 0.0f);
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
    glm::mat4 hitboxModel = glm::mat4(1.0f);
    hitboxModel = glm::translate(hitboxModel, pos + hitboxOffset);
    hitboxModel = glm::rotate(hitboxModel, glm::radians(-yaw), glm::vec3(0, 1, 0));
    hitboxModel = glm::scale(hitboxModel, hitboxSize);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &hitboxModel[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 0.0f, 0.0f);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);
    glBindVertexArray(0);

    // reuse your existing edge array
    float hitboxVerts[] = {
        // bottom
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
        0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
        0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f,
        // top
        -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
        0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
        // verticals
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
        0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    GLuint hbVAO, hbVBO;
    glGenVertexArrays(1, &hbVAO);
    glGenBuffers(1, &hbVBO);
    glBindVertexArray(hbVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hbVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(hitboxVerts), hitboxVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glDrawArrays(GL_LINES, 0, 24);
    glDeleteBuffers(1, &hbVBO);
    glDeleteVertexArrays(1, &hbVAO);
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
