// 08 15 2026, 15 30
/* purpose
* Outfits panel entry point.
* List, create, select, rename, copy, delete outfits.
* DOES NOT draw popups; avatar-editor-popups does.
*/
#pragma once
#include <GLFW/glfw3.h>

// Outfits panel (right): list + create/select/rename/copy/delete.
void drawAvatarOutfitsPanel(GLFWwindow* win, float px, float py, float pw, float ph);
