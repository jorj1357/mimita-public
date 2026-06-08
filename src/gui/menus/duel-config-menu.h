#pragma once

#include <GLFW/glfw3.h>

struct DuelConfigResult {
    bool startDuel = false;
    bool goBack = false;

    int numNpcs = 3;
    int killsToWin = 5;
    int duelLengthSeconds = 300;
    float npcDifficulty = 5.0f;
};

DuelConfigResult drawDuelConfigMenu(GLFWwindow* win);
