#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <string>
#include <vector>
#include "gui/ui-system.h"

namespace UISys {
extern GLFWwindow* gWindow;
extern GLuint gProgram;
extern GLuint gVao;
extern GLuint gVbo;
extern GLint gScreenLoc;
extern GLint gColorLoc;
extern GLint gUseTexLoc;
extern GLint gTexLoc;
extern int gFbW;
extern int gFbH;
extern bool gDebug;
extern bool gMousePrev;
extern bool gMouseDown;
extern bool gMouseClickEdge;
extern bool gUiEditMode;
extern int gFrame;
extern int gDrawCalls;
extern int gWidgets;
extern std::vector<std::string> gWarnings;
extern std::vector<UITrackedWidget> gTrackedWidgets;
extern std::string gHoverOwnerKey;
extern std::string gPrevHoverOwnerKey;
extern bool gOverlapDebugEnabled;
extern bool gCoordDebug;
extern double gScrollYOffset;
extern bool gDropdownModalActive;
}

inline bool pointIn(double mx, double my, UIRect r)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

void ensureProgram();
bool uiCanPlayUISound();
