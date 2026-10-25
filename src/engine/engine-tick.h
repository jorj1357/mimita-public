#pragma once

#include <string>

struct Engine;
void engineTick(Engine& engine);
void engineSetRoomCodeFilePath(const std::string& path);
