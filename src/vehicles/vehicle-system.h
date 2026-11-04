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
    // Dedicated-server spawn. The chassis gets a replicated network id and is
    // the only body this vehicle advances on the server's shared fixed tick.
    uint32_t spawnAuthoritative(const std::string& templateId,
                                const glm::mat4& transform,
                                uint32_t chassisNetworkId,
                                uint32_t driverActorId = 0);
    // Client mirror. Creates/binds the chassis mirror and a non-simulating
    // VehicleState. Returns false when the template is missing.
    bool spawnMirror(uint32_t vehicleId, const std::string& templateId,
                     uint32_t chassisNetworkId, const glm::mat4& transform,
                     const std::vector<int>& occupants);
    bool despawn(uint32_t vehicleId);
    bool despawnByChassisNetworkId(uint32_t chassisNetworkId);
    void clear();

    VehicleState* find(uint32_t vehicleId);
    const VehicleState* find(uint32_t vehicleId) const;
    VehicleState* findByChassisNetworkId(uint32_t chassisNetworkId);
    const std::vector<VehicleState>& vehicles() const { return mVehicles; }
    std::vector<VehicleState>& vehiclesMutable() { return mVehicles; }

    void setInput(uint32_t vehicleId, const VehicleInput& input);
    bool enterLocalDriver(uint32_t vehicleId, int seat,
                          uint32_t actorId = 1);
    void exitLocalDriver();
    // Server-side occupancy authority.
    bool enterSeat(uint32_t vehicleId, int seat, uint32_t actorId);
    bool exitSeat(uint32_t vehicleId, int seat, uint32_t actorId);
    uint32_t exitSeatByActor(uint32_t actorId);
    bool seatOccupied(uint32_t vehicleId, int seat, uint32_t* actorId = nullptr) const;
    bool seatWorld(uint32_t vehicleId, int seat, glm::vec3& outPosition,
                   glm::vec3& outForward) const;
    bool isDriver(uint32_t vehicleId, uint32_t actorId) const;
    VehicleInput sampleLocalInput();
    // Interaction callback used by the general interaction target.
    bool activateVehicleInteraction(uint32_t vehicleId, const Camera* camera);
    bool hasLocalDriver() const { return mLocalVehicleId != 0; }
    uint32_t localVehicleId() const { return mLocalVehicleId; }
    uint32_t localVehicleEntityId() const;

    // Network seat request bridge. The interaction callback fills this while a
    // multiplayer client is connected; networking sends it until confirmed.
    void requestSeat(uint32_t vehicleId, int seat, bool exit);
    bool hasPendingSeatRequest() const { return mSeatRequestPending; }
    uint32_t pendingSeatVehicleId() const { return mSeatRequestVehicleId; }
    int pendingSeatIndex() const { return mSeatRequestSeat; }
    uint32_t pendingSeatSerial() const { return mSeatRequestSerial; }
    bool pendingSeatIsExit() const { return mSeatRequestExit; }
    void clearSeatRequest();

    // Dedicated-server process has no local presentation/audio and owns
    // authoritative vehicle simulation.
    void setServerProcess(bool server) { mServerProcess = server; }
    bool isServerProcess() const { return mServerProcess; }

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

    uint32_t spawnInternal(const std::string& templateId,
                           const glm::mat4& transform, uint32_t driverActorId,
                           uint32_t chassisNetworkId, bool authoritative);
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
    bool mServerProcess = false;

    bool mSeatRequestPending = false;
    uint32_t mSeatRequestVehicleId = 0;
    int mSeatRequestSeat = -1;
    uint32_t mSeatRequestSerial = 0;
    bool mSeatRequestExit = false;

    bool mPrevEngineKey = false;
    bool mPrevModeKey = false;
    bool mPrevShiftUpKey = false;
    bool mPrevShiftDownKey = false;
};
