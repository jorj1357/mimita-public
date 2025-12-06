// physics.cpp
#include "physics.h"
#include "../camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// ------------------------------
// Constants
// ------------------------------
const float GRAVITY     = -25.0f;
const float MOVE_SPEED  = 50.0f;
const float JUMP_SPEED  = 10.0f;

const float CAPSULE_RADIUS = 0.35f;
const float CAPSULE_HEIGHT = 1.8f;
const float FLOOR_MAX_ANGLE = 50.0f;

const float FLOOR_MIN_DOT =
    glm::cos(glm::radians(90.0f - FLOOR_MAX_ANGLE));

// ------------------------------
// Helpers
// ------------------------------
static inline float planeDist(const glm::vec3& p,
                              const glm::vec3& n,
                              const glm::vec3& p0)
{
    return glm::dot(p - p0, n);
}

static inline bool pointInTri(const glm::vec3& p,
                              const glm::vec3& a,
                              const glm::vec3& b,
                              const glm::vec3& c,
                              const glm::vec3& n)
{
    if (glm::dot(n, glm::cross(b - a, p - a)) < 0) return false;
    if (glm::dot(n, glm::cross(c - b, p - b)) < 0) return false;
    if (glm::dot(n, glm::cross(a - c, p - c)) < 0) return false;
    return true;
}

static inline glm::vec3 projectToPlane(const glm::vec3& p,
                                       const glm::vec3& n,
                                       const glm::vec3& p0)
{
    float d = glm::dot(p - p0, n);
    return p - n * d;
}

static inline glm::vec3 slideVector(const glm::vec3& v, const glm::vec3& n)
{
    return v - n * glm::dot(v, n);
}


// ==============================================
// MAIN UPDATE FUNCTION
// ==============================================
void updatePhysics(Player& p, const Mesh& world,
                   GLFWwindow* win, float dt, const Camera& cam)
{
    // movement input
    glm::vec3 forward = glm::normalize(glm::vec3(cam.front.x, 0, cam.front.z));
    glm::vec3 right   = glm::cross(forward, glm::vec3(0,1,0));

    glm::vec3 wish(0);

    if (glfwGetKey(win, GLFW_KEY_W)) wish += forward;
    if (glfwGetKey(win, GLFW_KEY_S)) wish -= forward;
    if (glfwGetKey(win, GLFW_KEY_A)) wish -= right;
    if (glfwGetKey(win, GLFW_KEY_D)) wish += right;

    if (glm::dot(wish, wish) > 0.0001f)
        wish = glm::normalize(wish);

    p.vel.x = wish.x * MOVE_SPEED;
    p.vel.z = wish.z * MOVE_SPEED;

    // gravity
    p.vel.y += GRAVITY * dt;

    // jump
    if (p.onGround && glfwGetKey(win, GLFW_KEY_SPACE))
    {
        p.vel.y = JUMP_SPEED;
        p.onGround = false;
    }

    glm::vec3 finalPos = p.pos;
    glm::vec3 move = p.vel * dt;

    p.onGround = false;

    // -----------------------------
    // sweep 4 iterations
    // -----------------------------
    for (int iter = 0; iter < 4; iter++)
    {
        float bestT = 1.0f;
        glm::vec3 bestNormal(0);
        bool hit = false;

        // test triangles
        for (size_t i = 0; i < world.verts.size(); i += 3)
        {
            glm::vec3 a = world.verts[i+0].pos;
            glm::vec3 b = world.verts[i+1].pos;
            glm::vec3 c = world.verts[i+2].pos;

            glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
            if (glm::dot(n, n) < 1e-5f) continue;

            glm::vec3 bottom = finalPos + glm::vec3(0, CAPSULE_RADIUS, 0);

            float d0 = planeDist(bottom, n, a);
            float d1 = planeDist(bottom + move, n, a);

            if (d0 > CAPSULE_RADIUS && d1 < CAPSULE_RADIUS)
            {
                float t = (d0 - CAPSULE_RADIUS) / (d0 - d1);
                if (t < bestT)
                {
                    glm::vec3 hitPos = bottom + move * t;
                    glm::vec3 proj = projectToPlane(hitPos, n, a);

                    if (pointInTri(proj, a, b, c, n))
                    {
                        bestT = t;
                        bestNormal = n;
                        hit = true;
                    }
                }
            }
        }

        if (!hit)
        {
            finalPos += move;
            break;
        }

        finalPos += move * bestT;

        bool isFloor = bestNormal.y > FLOOR_MIN_DOT;

        if (isFloor)
        {
            p.onGround = true;
            p.vel.y = 0;
            move = slideVector(move * (1 - bestT), glm::vec3(0,1,0));
        }
        else
        {
            move = slideVector(move * (1 - bestT), bestNormal);
        }
    }

    p.pos = finalPos;
}
