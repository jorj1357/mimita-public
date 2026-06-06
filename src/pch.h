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

// OpenGL
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>