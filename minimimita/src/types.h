#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <string>
#include <vector>

static const glm::vec3 WORLD_UP(0.0f, 0.0f, 1.0f);
static const float GRAVITY = -9.81f;
static const float PHYSICS_DT = 1.0f / 120.0f;
static const float MAX_GROUND_ANGLE = 0.4f;

struct Contact {
    glm::vec3 point;
    glm::vec3 normal;
    float depth;
    int triangleIndex = -1;
    enum Side { FLOOR, WALL, CEILING };
    Side side;
};

struct ContactState {
    std::vector<Contact> contacts;
    bool touchingFloor = false;
    bool touchingWall = false;
    bool touchingCeiling = false;

    void clear() {
        contacts.clear();
        touchingFloor = false;
        touchingWall = false;
        touchingCeiling = false;
    }
};

struct Triangle {
    glm::vec3 a, b, c;
    glm::vec3 normal;

    Triangle() = default;
    Triangle(glm::vec3 a_, glm::vec3 b_, glm::vec3 c_)
        : a(a_), b(b_), c(c_) {
        normal = glm::normalize(glm::cross(b - a, c - a));
    }
};

struct Player {
    glm::vec3 position;
    glm::vec3 velocity;
    float radius = 0.3f;
    float height = 1.8f;
    ContactState contacts;

    glm::vec3 capA() const {
        return position - (height * 0.5f - radius) * WORLD_UP;
    }
    glm::vec3 capB() const {
        return position + (height * 0.5f - radius) * WORLD_UP;
    }
};

struct Camera {
    glm::vec3 target = glm::vec3(0.0f);
    float yaw = 0.0f;
    float pitch = -0.4f;
    float distance = 8.0f;

    glm::vec3 position() const {
        glm::vec3 dir;
        dir.x = cos(pitch) * sin(yaw);
        dir.y = cos(pitch) * cos(yaw);
        dir.z = sin(pitch);
        return target + dir * distance;
    }

    glm::vec3 forward2D() const {
        glm::vec3 fwd = glm::normalize(target - position());
        fwd.z = 0.0f;
        float len = glm::length(fwd);
        if (len < 0.001f) return glm::vec3(0.0f, -1.0f, 0.0f);
        return fwd / len;
    }

    glm::vec3 right2D() const {
        return glm::normalize(glm::cross(forward2D(), WORLD_UP));
    }

    glm::mat4 view() const {
        return glm::lookAt(position(), target, WORLD_UP);
    }

    glm::mat4 projection(float aspect) const {
        return glm::perspective(glm::radians(100.0f), aspect, 0.1f, 100.0f);
    }
};

struct InputState {
    bool w = false, a = false, s = false, d = false;
    bool space = false;
    bool spacePrev = false;
    bool r = false;
    glm::vec3 wishDir = glm::vec3(0.0f);
    char commandBuffer[256] = {};
    int commandLen = 0;
    bool commandEnter = false;
};

struct TestMap {
    std::string name;
    std::vector<Triangle> triangles;
    glm::vec3 spawnPosition;
};
