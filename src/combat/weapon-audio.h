#pragma once

#include <string>
#include <glm/glm.hpp>

struct WeaponDefinition;
enum class AudioEventType;

namespace WeaponAudio {

void playShootSound(const WeaponDefinition& def, const glm::vec3& position);
void playReloadSound(const WeaponDefinition& def);
void playDryFireSound(const WeaponDefinition& def);
void playEquipSound(const WeaponDefinition& def);
void playHitSound(const WeaponDefinition& def, const glm::vec3& position);
void playGodballWhoosh(const glm::vec3& position, float speed01);
void playGodballImpact(const glm::vec3& position, float damageFraction);
void playSwordswordHitSound(const glm::vec3& position, float strength01);

} // namespace WeaponAudio