// C:\important\quiet\n\mimita-priv-v7\src\audio\audio.h
/**
 * purpose
 * audio header file 
 * so we can pla sounds and it sound good and cool
 */

#pragma once
#include <string>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

void audioUpdate(float dt);

void playFreezeBeginSound();
void playFreezeHoldSound();
void playFreezeEndSound();

void playSound(const std::string& name, float volume = 1.0f);
void playAirJumpSound();
void playSoundAt(const std::string& name, glm::vec3 pos, float volume = 1.0f);
void playRandomFootstep();