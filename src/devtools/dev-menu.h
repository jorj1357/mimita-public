#pragma once

#include "dev-types.h"

struct GLFWwindow;
class NpcSystem;
class Player;

struct DevMenuResult {
    bool goBack = false;
};

DevMenuResult drawDevMenu(GLFWwindow* win, NpcSystem& npcSystem, Player& player);
