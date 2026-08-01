// 07 31 2026, 18 41
/* purpose
* Declares the shared client-side explosion visual spawner for rockets and grenades.
* Provides the single source of truth for explosion sound, fireball, smoke, and debris.
* Used by local prediction, offline explosions, and network confirmation.
* Does NOT apply damage, knockback, camera shake, or server authority.
* Does NOT send packets or render viewmodels.
*/
#pragma once

#include <string>

#include <glm/glm.hpp>

void spawnExplosionFx(const glm::vec3& position, const std::string& weaponId,
                      const std::string& attacker, float sizeScale = 1.0f);
