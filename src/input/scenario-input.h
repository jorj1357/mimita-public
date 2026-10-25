#pragma once

struct GLFWwindow;
struct InputFrame;

// One override surface for recorded scenario playback. When active, gameplay
// input owners read the recorded frame instead of live GLFW/terminal input.
// This keeps playback on the same input path as normal play.
namespace ScenarioInput {

void setActive(bool active);
bool active();

void setFrame(const InputFrame* frame);
const InputFrame* frame();

bool mouseButton(GLFWwindow* window, int button);
bool keyDown(GLFWwindow* window, int key);

void clear();

} // namespace ScenarioInput
