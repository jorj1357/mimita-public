// 2026-10-27
/* purpose
* Resolve the seat the player is aiming at and handle the F interaction edge for
* entering/exiting a vehicle. A valid seat target wins over revive.
* Does NOT own vehicle physics, seats, or camera.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

struct GLFWwindow;
class Camera;

bool vehicleFindAimedSeat(const Camera& camera, uint32_t& outVehicleId,
                          int& outSeat, float maxDistance = 6.0f);

// Returns true when F was consumed by vehicle enter/exit this frame.
bool vehicleHandleInteractionKey(GLFWwindow* window, const Camera& camera);
