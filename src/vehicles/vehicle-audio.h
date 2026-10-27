// 2026-10-27
/* purpose
* Schedule the vehicle engine pulse from the fixed 60 Hz vehicle tick.
* RPM controls how quickly the same sample repeats; the scheduler is tick-bound
* so high or low frame rates cannot change the cadence.
* Does NOT own the vehicle or the audio backend.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "vehicles/vehicle-types.h"

void updateVehicleEngineAudio(VehicleState& state, const VehicleSpec& spec,
                              const glm::vec3& position, uint64_t tick);
void stopVehicleEngineAudio(uint32_t vehicleId);
