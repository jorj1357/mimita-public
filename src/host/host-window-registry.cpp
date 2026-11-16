#include "host/host-window-registry.h"

namespace mimita::host {
namespace {
GLFWwindow* gResidentWindow = nullptr;
}

void setResidentWindow(GLFWwindow* window) {
    gResidentWindow = window;
}

GLFWwindow* residentWindow() {
    return gResidentWindow;
}

}  // namespace mimita::host
