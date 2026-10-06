#pragma once

#include <glm/glm.hpp>

void playHitmarkerSound(int damage);
void playDeathSoundForDamage(int damage, bool localKiller, const glm::vec3& position);
void pollHitmarkerAudioConfig();
void registerHitmarkerAudioCommands();
