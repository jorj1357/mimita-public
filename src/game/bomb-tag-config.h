#pragma once
#include <GLFW/glfw3.h>

struct BombTagConfigResult {
    bool start = false;
    bool goBack = false;
    int numNpcs = 3;
    int lives = 0; // 0 = infinite
    int timeLimitSeconds = 180;
    float npcDifficulty = 5.0f;
};

BombTagConfigResult drawBombTagConfigMenu(GLFWwindow* win);
