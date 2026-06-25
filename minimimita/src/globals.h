#pragma once
#include "types.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

extern GLFWwindow* gWindow;
extern int gWinW, gWinH;
extern Player gPlayer;
extern Camera gCamera;
extern InputState gInput;
extern TestMap gCurrentMap;
extern int gCurrentMapIndex;
extern bool gWireframeMode;
extern double gLastTime;
extern float gAccumulator;
extern int gFrameCount;
