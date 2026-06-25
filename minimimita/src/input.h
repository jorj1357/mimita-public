#pragma once
#include <GLFW/glfw3.h>

void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);
void cursorPosCallback(GLFWwindow* win, double x, double y);
void scrollCallback(GLFWwindow* win, double xoffset, double yoffset);
void windowSizeCallback(GLFWwindow* win, int w, int h);
void errorCallback(int err, const char* desc);
