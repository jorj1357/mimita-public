#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// winsock FIRST
#include <winsock2.h>
#include <ws2tcpip.h>

// windows AFTER
#include <windows.h>

// STL
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <functional>
#include <optional>

// OpenGL types belong to the resident device for the hot game DLL.
#ifndef MIMITA_GAME_DLL
#include <glad/glad.h>
#endif
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>