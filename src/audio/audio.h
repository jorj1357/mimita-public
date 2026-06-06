// C:\important\quiet\n\mimita-priv-v7\src\audio\audio.h
/**
 * purpose
 * audio header file 
 * so we can pla sounds and it sound good and cool
 */

#pragma once
#include <string>
#include <glm/glm.hpp>

void audioUpdate(float dt);

void playFreezeBeginSound();
void playFreezeHoldSound();
void playFreezeEndSound();

void playSound(const std::string& name, float volume = 1.0f);
void playEventSound(const std::string& name, float volume = 1.0f);
void playSoundPitched(const std::string& name, float volume, float pitch);
void playWorldSound(const std::string& name, glm::vec3 pos, float volume = 1.0f,
                    float pitch = 1.0f, float maxDistance = 30.0f);
void setAudioListener(glm::vec3 pos, glm::vec3 forward);
void playAirJumpSound();
void playSoundAt(const std::string& name, glm::vec3 pos, float volume = 1.0f);
void playRandomFootstep();

void playMenuClick();
