#pragma once
#include <GLFW/glfw3.h>

void startMmrAnimation(int startMmr, int endMmr, int change);
bool isMmrAnimating();
void stopMmrAnimation();
void updateMmrAnimation(float dt);

enum class CompetitiveResultAction { None, PlayAgain, ExitToMenu };

void setLastCompetitiveMatchResult(const struct CompetitiveMatchResult& result);
const struct CompetitiveMatchResult& getLastCompetitiveMatchResult();
CompetitiveResultAction drawCompetitiveResultScreen(GLFWwindow* win);

enum class CompetitiveMenuAction { None, FindMatch, SignIn, GoBack };
CompetitiveMenuAction drawCompetitiveMenu(GLFWwindow* win);
