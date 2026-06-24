#include "globals.h"

GLFWwindow* gWindow = nullptr;
int gWinW = 1280, gWinH = 800;
Player gPlayer;
Camera gCamera;
InputState gInput;
TestMap gCurrentMap;
int gCurrentMapIndex = -1;
bool gWireframeMode = false;
double gLastTime = 0.0;
float gAccumulator = 0.0f;
int gFrameCount = 0;
