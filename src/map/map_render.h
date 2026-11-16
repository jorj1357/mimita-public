// C:\important\go away v5\s\mimita-v5\src\map\map_render.h

#pragma once
#include <cstdint>
#include "map_common.h"

std::uint32_t createMapVAO(const Mesh& mesh);
void drawMap(std::uint32_t vao, size_t vertexCount);
