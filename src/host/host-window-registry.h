#pragma once

struct GLFWwindow;

namespace mimita::host {

// Resident window handle for host-side fallback services. Set by Renderer when
// the device creates its window; cleared on shutdown.
void setResidentWindow(GLFWwindow* window);
GLFWwindow* residentWindow();

}  // namespace mimita::host
