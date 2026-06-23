#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <string>
#include <vector>

static const glm::vec3 WORLD_UP(0.0f, 0.0f, 1.0f);
static const float GRAVITY = -9.81f;
static const float PHYSICS_DT = 1.0f / 120.0f;
static const float MAX_GROUND_ANGLE = 0.70710678f; // cos(45 deg)

struct Contact {
    glm::vec3 point;
    glm::vec3 normal;
    float depth;
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
    bool grounded = false;
    ContactState contacts;

    glm::vec3 capA() const {
        return position - (height * 0.5f - radius) * WORLD_UP;
    }
    glm::vec3 capB() const {
        return position + (height * 0.5f - radius) * WORLD_UP;
    }
};

struct Point {
    glm::vec3 position;
    glm::vec3 velocity;
    float radius = 0.1f;
};

struct Line {
    glm::vec3 pointA;
    glm::vec3 pointB;
    float radius = 0.1f;
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

    glm::mat4 view() const {
        return glm::lookAt(position(), target, WORLD_UP);
    }

    glm::mat4 projection(float aspect) const {
        return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
};

struct InputState {
    bool w = false, a = false, s = false, d = false;
    bool space = false;
    bool spacePrev = false;
    bool r = false;
    int mapIndex = 0;
    float mouseDX = 0.0f, mouseDY = 0.0f;
    bool mouseDown = false;
    double scrollY = 0.0;
};

struct TestMap {
    std::string name;
    std::vector<Triangle> triangles;
    glm::vec3 spawnPosition;
};
