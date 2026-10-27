// 2026-10-27
/* purpose
* Own spawned vehicles: spawn/despawn, local seat control, and the fixed-tick
* suspension/tire/drivetrain update. Vehicles are chassis PhysicalEntity bodies
* plus data-driven wheel units; no car-specific physics engine exists.
* Does NOT load JSON (VehicleConfigRegistry owns that), render, or send packets.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "vehicles/vehicle-types.h"

struct World;
class Player;
class Camera;

// Cached broadphase ray query shared by vehicle wheel contact and authoring
// placement. Tests chunk triangles and large world triangles.
bool vehicleRaycastWorld(const World& world, const glm::vec3& origin,
                         const glm::vec3& direction, float maxDistance,
                         float& hitDistance, glm::vec3& hitNormal);

class VehicleSystem {
public:
    static VehicleSystem& instance();

    bool loadConfig(const std::string& directory = "config/vehicles");
    bool pollReload();
    void registerFixedTick();

    uint32_t spawn(const std::string& templateId, const glm::mat4& transform,
                   uint32_t driverActorId = 0);
    bool despawn(uint32_t vehicleId);
    void clear();

    VehicleState* find(uint32_t vehicleId);
    const VehicleState* find(uint32_t vehicleId) const;
    const std::vector<VehicleState>& vehicles() const { return mVehicles; }

    void setInput(uint32_t vehicleId, const VehicleInput& input);
    bool enterLocalDriver(uint32_t vehicleId, int seat);
    void exitLocalDriver();
    // Interaction callback used by the general interaction target.
    bool activateVehicleInteraction(uint32_t vehicleId, const Camera* camera);
    bool hasLocalDriver() const { return mLocalVehicleId != 0; }
    uint32_t localVehicleId() const { return mLocalVehicleId; }
    uint32_t localVehicleEntityId() const;

    // Despawns every vehicle below killZ. A locally driven occupant is unseated,
    // placed at the void position, and killed through the normal void-death path.
    void checkVoidDeath(Player& player, float killZ);

    // World-space seat pose for the locally controlled vehicle. Returns false
    // when there is no local driver or the vehicle entity is gone.
    bool localSeatWorld(glm::vec3& outPosition, glm::vec3& outForward) const;
    glm::vec3 localVehiclePosition() const;

    // Fixed-step callback registered with PhysicalEntitySystem. Applies
    // suspension, tire, drag, and drivetrain forces before entity integration.
    void update(float dt, const World& world);

private:
    VehicleSystem() = default;

    VehicleInput readLocalInput(const VehicleState& state);
    void updateDrivetrain(VehicleState& state, const VehicleSpec& spec, float dt);
    void updateWheel(VehicleState& state, const VehicleSpec& spec,
                     int wheelIndex, float dt, const World& world,
                     const std::vector<int>& candidates);
    void updateChassisAero(VehicleState& state, const VehicleSpec& spec,
                           float dt);

    std::vector<VehicleState> mVehicles;
    uint32_t mNextId = 1;
    uint32_t mLocalVehicleId = 0;

    bool mPrevEngineKey = false;
    bool mPrevModeKey = false;
    bool mPrevShiftUpKey = false;
    bool mPrevShiftDownKey = false;
};
