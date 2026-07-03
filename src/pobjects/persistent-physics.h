#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct World;
class Player;
class NpcSystem;
class Camera;
struct WeaponDefinition;
struct WeaponRuntime;

enum class PersistentShape : uint8_t {
    Sphere,
    Cylinder,
    Box,
    Capsule
};

struct PersistentPhysicsConfig {
    PersistentShape shape = PersistentShape::Sphere;
    float radius = 0.3f;
    float height = 0.6f;
    float mass = 1.0f;
    float gravity = 30.0f;
    float drag = 0.3f;
    float angularDrag = 0.5f;
    float restitution = 0.25f;
    float friction = 0.6f;
    float maxLinearVelocity = 50.0f;
    float maxAngularVelocity = 25.0f;
    float sleepVelocity = 0.05f;
    float sleepAngular = 0.03f;
    float sleepTime = 0.8f;
    float lifetime = 0.0f;
    bool explodeOnContact = false;
    bool collideWithPlayer = true;
    bool collideWithNpcs = true;
    bool collideWithWorld = true;
    float explosionRadius = 8.0f;
    float explosionDamage = 150.0f;
    float explosionKnockback = 160.0f;
    float explosionSelfKnockbackMul = 0.0f;
    std::string explosionSound = "weapon/bomb/explosion2";
    std::string spawnSound;
};

struct PersistentPhysicsObject {
    uint32_t id = 0;
    uint32_t ownerId = 0;
    std::string ownerName;
    PersistentPhysicsConfig cfg;

    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};

    float age = 0.0f;
    int bounceCount = 0;
    bool exploded = false;
    bool sleeping = false;
    float sleepTimer = 0.0f;
    bool alive = false;

    std::string weaponId;
};

class PersistentPhysicsSystem {
public:
    static PersistentPhysicsSystem& instance();

    uint32_t spawn(const PersistentPhysicsConfig& cfg,
                    const glm::vec3& position,
                    const glm::vec3& velocity,
                    const glm::vec3& angularVelocity,
                    uint32_t ownerId,
                    const std::string& ownerName,
                    const std::string& weaponId);

    void update(float dt, const World& world, Player& player, NpcSystem& npcs, class Camera* camera = nullptr);
    void render(const Camera& camera) const;

    void clear();
    void destroy(uint32_t id);

    const std::vector<PersistentPhysicsObject>& objects() const { return mObjects; }
    std::vector<PersistentPhysicsObject>& objects() { return mObjects; }

private:
    PersistentPhysicsSystem() = default;

    void physicsStep(PersistentPhysicsObject& obj, float dt, const World& world);
    void checkCollisions(PersistentPhysicsObject& obj, float dt, const World& world, Player& player, NpcSystem& npcs);
    void doExplosion(PersistentPhysicsObject& obj, const World& world, Player& player, NpcSystem& npcs, class Camera* camera);
    void renderPrimitive(const PersistentPhysicsObject& obj, const Camera& camera) const;

    uint32_t mNextId = 1;
    std::vector<PersistentPhysicsObject> mObjects;
};
