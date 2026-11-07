// 2026-11-07
/* purpose
* host adapter that asks the live world-rendering module for the canonical
* six-face box mesh and keeps a safety fallback before the module is loaded
*/
#pragma once

#include "modules/api/world-rendering-api.h"

bool buildBoxMesh(const mimita_world_rendering::BoxMeshV1& request,
                  mimita_world_rendering::BoxMeshV1& out);
