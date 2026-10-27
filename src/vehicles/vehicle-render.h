// 2026-10-27
/* purpose
* Draw the visible vehicle body helper and smooth cylinder wheels from the
* shared vehicle runtime state.
* Does NOT simulate vehicles or own their mass/tire data.
*/
#pragma once

class Camera;

void drawVehicleWheels(const Camera& camera);
