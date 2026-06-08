#include "weapon-audio.h"
#include "weapon-types.h"
#include "audio/audio.h"
#include <cstdlib>
#include <cstdio>

namespace WeaponAudio {

void playShootSound(const WeaponDefinition& def, const glm::vec3& position) {
    if (def.soundShoot.empty()) return;
    float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);
    float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f);
    playWorldSound(def.soundShoot, position, rndVolume, rndPitch, 80.0f);
}

void playReloadSound(const WeaponDefinition& def) {
    if (def.soundReload.empty()) return;
    playSound(def.soundReload, 0.8f);
}

void playDryFireSound(const WeaponDefinition& def) {
    if (def.soundDryFire.empty()) return;
    playSound(def.soundDryFire, 0.25f);
}

void playEquipSound(const WeaponDefinition& def) {
    if (def.soundEquip.empty()) return;
    playSound(def.soundEquip, 0.85f);
}

void playHitSound(const WeaponDefinition& def, const glm::vec3& position) {
    if (def.soundHit.empty()) return;
    playWorldSound(def.soundHit, position, 0.85f, 1.0f, 35.0f);
}

void playGodballWhoosh(const glm::vec3& position, float speed01) {
    float clamped = std::min(speed01, 1.0f);
    float volume = 0.1f + clamped * 0.4f;
    float pitch = 0.5f + clamped * 0.8f;
    playWorldSound("whoosh", position, volume, pitch, 20.0f);
}

void playGodballImpact(const glm::vec3& position, float damageFraction) {
    float clamped = std::min(damageFraction, 1.0f);
    float volume = 0.3f + clamped * 0.6f;
    float pitch = 0.8f + clamped * 0.4f;
    playWorldSound("player_hurt", position, volume, pitch, 25.0f);
}

} // namespace WeaponAudio