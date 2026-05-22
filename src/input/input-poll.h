// C:\important\quiet\n\mimita-priv-v7\src\input\input-poll.h
// feb 10 2026
/**
 * purpose
 * header for input polling
 * which is the rewrite so phsics is modular and buncehs 
 * off small files in src/phsics/movement
 */

#pragma once

struct GLFWwindow;
struct InputState;
class Camera;

InputState pollInput(GLFWwindow* win, const Camera& cam);
