// 08 29 2026, 15 30
/* purpose
* Owns the password-entry popup for private servers.
* Draws a JSON-driven modal overlay with a password text input and join/cancel buttons.
* Reuses the same pause-menu pattern: GuiLayout + drawGuiElement.
* Does NOT own network connection logic or ICE transport setup.
* Does NOT manage server browser state or room lookup.
* Does NOT persist passwords or handle authentication beyond comparison.
*/

#pragma once

#include <string>

struct GLFWwindow;

namespace PasswordPopup {

bool isOpen();
void open(const std::string& roomCode, const std::string& serverName);
void close();
void handleKey(GLFWwindow* window, int key, int action);
void render(GLFWwindow* window);

std::string getRoomCode();
std::string getPassword();
void clearResult();
void setWrongPassword(bool wrong, const std::string& serverName = "");

} // namespace PasswordPopup
