#pragma once

struct GLFWwindow;

// Avatar Editor "Animation" tab. Data-driven: lists the current avatar's rig
// parts and animation clips from AvatarAnimationRegistry rather than any
// hard-coded part set, so it extends as the animation system grows.
// See docs/specs/animations/animations.md.
void drawAvatarAnimationTab(GLFWwindow* win, float px, float py, float pw, float ph);
