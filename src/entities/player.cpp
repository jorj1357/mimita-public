// C:\important\quiet\n\mimita-priv-v7\src\entities\player.cpp
// feb 10 2026 CLEANED : slim + correct

#include "player.h"

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#include "physics/config.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "audio/audio.h"

// globals (engine-level)
extern TextureStore gTextures;
extern Renderer* gRenderer;

// =====================================================
// Capsule render (debug/simple visual)
// =====================================================

static GLuint capsuleVAO = 0;
static GLuint capsuleVBO = 0;
static int    capsuleVertCount = 0;

static void initCapsuleMesh()
{
    if (capsuleVAO) return;

    struct V {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec3 normal;
    };

    constexpr int slices = 16;
    constexpr int stacks = 8;
    constexpr float PI = 3.1415926535f;

    float r = PLAYER_RADIUS;
    float h = PLAYER_HEIGHT;

    float cylinderHalf = h * 0.5f - r;

    std::vector<V> verts;

    // ===============================
    // CYLINDER
    // ===============================

    for (int i = 0; i < slices; i++)
    {
        float u0 = float(i) / slices;
        float u1 = float(i+1) / slices;

        float a0 = u0 * 2 * PI;
        float a1 = u1 * 2 * PI;

        glm::vec3 p0(r*cos(a0), r*sin(a0), -cylinderHalf);
        glm::vec3 p1(r*cos(a1), r*sin(a1), -cylinderHalf);
        glm::vec3 p2(r*cos(a0), r*sin(a0),  cylinderHalf);
        glm::vec3 p3(r*cos(a1), r*sin(a1),  cylinderHalf);

        verts.push_back({p0,{u0,0}, glm::normalize(glm::vec3(p0.x,p0.y,0.0f))});
        verts.push_back({p1,{u1,0}, glm::normalize(glm::vec3(p1.x,p1.y,0.0f))});
        verts.push_back({p2,{u0,1}, glm::normalize(glm::vec3(p2.x,p2.y,0.0f))});

        verts.push_back({p1,{u1,0}, glm::normalize(glm::vec3(p1.x,p1.y,0.0f))});
        verts.push_back({p3,{u1,1}, glm::normalize(glm::vec3(p3.x,p3.y,0.0f))});
        verts.push_back({p2,{u0,1}, glm::normalize(glm::vec3(p2.x,p2.y,0.0f))});
    }

    // ===============================
    // TOP HEMISPHERE
    // ===============================

    for (int j = 0; j < stacks; j++)
    {
        float v0 = float(j) / stacks;
        float v1 = float(j+1) / stacks;

        float phi0 = v0 * PI * 0.5f;
        float phi1 = v1 * PI * 0.5f;

        for (int i = 0; i < slices; i++)
        {
            float u0 = float(i) / slices;
            float u1 = float(i+1) / slices;

            float a0 = u0 * 2 * PI;
            float a1 = u1 * 2 * PI;

            glm::vec3 p0(
                r * cos(a0) * cos(phi0),
                r * sin(a0) * cos(phi0),
                r * sin(phi0) + cylinderHalf
            );

            glm::vec3 p1(
                r * cos(a1) * cos(phi0),
                r * sin(a1) * cos(phi0),
                r * sin(phi0) + cylinderHalf
            );

            glm::vec3 p2(
                r * cos(a0) * cos(phi1),
                r * sin(a0) * cos(phi1),
                r * sin(phi1) + cylinderHalf
            );

            glm::vec3 p3(
                r * cos(a1) * cos(phi1),
                r * sin(a1) * cos(phi1),
                r * sin(phi1) + cylinderHalf
            );

            verts.push_back({p0,{u0,v0}, glm::normalize(p0 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,cylinderHalf))});

            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p3,{u1,v1}, glm::normalize(p3 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,cylinderHalf))});
        }
    }

    // ===============================
    // BOTTOM HEMISPHERE
    // ===============================

    for (int j = 0; j < stacks; j++)
    {
        float v0 = float(j) / stacks;
        float v1 = float(j+1) / stacks;

        float phi0 = v0 * PI * 0.5f;
        float phi1 = v1 * PI * 0.5f;

        for (int i = 0; i < slices; i++)
        {
            float u0 = float(i) / slices;
            float u1 = float(i+1) / slices;

            float a0 = u0 * 2 * PI;
            float a1 = u1 * 2 * PI;

            glm::vec3 p0(
                r * cos(a0) * cos(phi0),
                r * sin(a0) * cos(phi0),
                -r * sin(phi0) - cylinderHalf
            );

            glm::vec3 p1(
                r * cos(a1) * cos(phi0),
                r * sin(a1) * cos(phi0),
                -r * sin(phi0) - cylinderHalf
            );

            glm::vec3 p2(
                r * cos(a0) * cos(phi1),
                r * sin(a0) * cos(phi1),
                -r * sin(phi1) - cylinderHalf
            );

            glm::vec3 p3(
                r * cos(a1) * cos(phi1),
                r * sin(a1) * cos(phi1),
                -r * sin(phi1) - cylinderHalf
            );

            verts.push_back({p0,{u0,v0}, glm::normalize(p0 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,-cylinderHalf))});

            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p3,{u1,v1}, glm::normalize(p3 - glm::vec3(0,0,-cylinderHalf))});
        }
    }

    capsuleVertCount = (int)verts.size();

    glGenVertexArrays(1,&capsuleVAO);
    glGenBuffers(1,&capsuleVBO);

    glBindVertexArray(capsuleVAO);
    glBindBuffer(GL_ARRAY_BUFFER,capsuleVBO);

    glBufferData(GL_ARRAY_BUFFER,
        verts.size()*sizeof(V),
        verts.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,normal));
}

// =====================================================
// Player
// =====================================================

Player::Player()
{
    reset();
}

void Player::reset()
{
    // pos = {0,0,50};
    // debug test
    // pos = {1,5,2};
    // debug test 2 for the ctf map
    pos = {1,5,30};
    vel = {0,0,0};
    dashVel = {0,0};
    onGround = false;

    // put this here so idk? mar 7 2026
    jumpHeldPrev = false;
    airJumpsLeft = 1;
    // dashCharges = DASH_MAX_CHARGES;
    groundReturnCharges = GROUND_RETURN_MAX_CHARGES;

    freezeTimer = 0.0f;
    freezeActive = false;
    freezeAvailable = true;
    freezeHeldPrev = false;
    freezeHoldSoundPlayed = false;
}

Capsule Player::getCapsule() const
{
    Capsule c;
    c.r = PLAYER_RADIUS;

    float half = PLAYER_HEIGHT * 0.5f;
    c.a = pos - glm::vec3(0,0,half - c.r);
    c.b = pos + glm::vec3(0,0,half - c.r);

    return c;
}

OBB Player::getOBB() const
{
    OBB b;
    b.center = pos;
    b.halfSize = glm::vec3(PLAYER_WIDTH,PLAYER_DEPTH,PLAYER_HEIGHT) * 0.5f;
    b.orientation = glm::rotate(glm::mat4(1.0f),
                                glm::radians(-yaw),
                                glm::vec3(0,0,1));
    return b;
}

void Player::updateAudio(float dt)
{
    // simple and works but annoying  air jump 
    if (didGroundJump) playSound("entity/player/jump",1.0f);
    // if (didAirJump)    playSound("entity/player/doublejump",1.0f);

    // with 0.5 sec wait time from audio.cpp 
    if (didAirJump)
        playAirJumpSound();

    // // testing this so that we stop spamming air jump
    // if (didGroundJump && !jumpHeldPrev)
    //     playSound("entity/player/jump",1.0f);

    // // this one spams so much we attempt fix 1 mar 7 2026 
    // if (didAirJump && !jumpHeldPrev)
    //     playSound("entity/player/doublejump",1.0f);
    
    if (didDash)       playSound("entity/player/dash",1.0f);
    if (didLand)       playSound("entity/player/land",1.0f);

    glm::vec2 xy = glm::vec2(vel.x,vel.y) + dashVel;
    // if (onGround && glm::length(xy) > 0.1f) {
    // > 0.1f was old, mar 8 2026
    // setting it to be 0.5f so i stop playing the sound so much
    if (onGround && glm::length(xy) > 0.5f) {
        footstepTimer -= dt;
        if (footstepTimer <= 0.0f) {
            playRandomFootstep();
            footstepTimer = 0.35f;
        }
    } else {
        footstepTimer = 0.0f;
    }

    didGroundJump = didAirJump = didDash = didLand = false;
}

void Player::render(unsigned int shader,
                    const glm::mat4& view,
                    const glm::mat4& proj) const
{
    initCapsuleMesh();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
    glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
    glUniform1i(glGetUniformLocation(shader,"uTex"),0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextures.get("greenwirev1"));

    glBindVertexArray(capsuleVAO);
    glDrawArrays(GL_TRIANGLES, 0, capsuleVertCount);
}
