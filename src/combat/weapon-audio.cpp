#include "weapon-audio.h"
#include "weapon-types.h"
#include "audio/audio.h"
#include <cstdlib>
#include <cstdio>
#include "debug/debug-log.h"

namespace WeaponAudio {

void playShootSound(const WeaponDefinition& def, const glm::vec3& position) {
    if (def.soundShoot.empty()) return;
    float pitchRange = std::max(0.0f, def.soundPitchVariation);
    float volRange = std::max(0.0f, def.soundVolumeVariation);
    float rndPitch = 1.0f + ((rand() % 20001 - 10000) / 10000.0f) * pitchRange;
    float rndVolume = 1.0f + ((rand() % 20001 - 10000) / 10000.0f) * volRange;
    Debug::log(Debug::Category::Audio, "[WEAPON AUDIO] weapon=%s event=shoot path=%s pitch=%.3f volume=%.3f\n",
               def.id.c_str(), def.soundShoot.c_str(), rndPitch, rndVolume);
    playWorldSound(def.soundShoot, position, rndVolume, rndPitch, 80.0f);
}

void playReloadSound(const WeaponDefinition& def) {
    if (def.soundReload.empty()) return;
    printf("[SOUND] weapon=%s event=reload path=%s\n", def.id.c_str(), def.soundReload.c_str());
    playSound(def.soundReload, 0.8f);
}

void playDryFireSound(const WeaponDefinition& def) {
    if (def.soundDryFire.empty()) return;
    printf("[SOUND] weapon=%s event=dry_fire path=%s\n", def.id.c_str(), def.soundDryFire.c_str());
    playSound(def.soundDryFire, 0.25f);
}

void playEquipSound(const WeaponDefinition& def) {
    if (def.soundEquip.empty()) return;
    printf("[SOUND] weapon=%s event=equip path=%s\n", def.id.c_str(), def.soundEquip.c_str());
    playSound(def.soundEquip, 0.85f);
}

void playHitSound(const WeaponDefinition& def, const glm::vec3& position) {
    if (def.soundHit.empty()) return;
    printf("[SOUND] weapon=%s event=hit_entity path=%s\n", def.id.c_str(), def.soundHit.c_str());
    float dist = glm::length(position - audioListenerPosition());
    float vol, pit;
    computeImpactAudio(1.2f, dist, 0.5f, vol, pit);
    playWorldSound(def.soundHit, position, vol, pit, 60.0f);
    Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=%s dist=%.1f pitch=%.2f volume=%.2f\n",
               def.soundHit.c_str(), dist, pit, vol);
}

void playGodballWhoosh(const glm::vec3& position, float speed01) {
    float clamped = std::min(speed01, 1.0f);
    float volume = 0.1f + clamped * 0.4f;
    float pitch = 0.5f + clamped * 0.8f;
    playWorldSound("whoosh", position, volume, pitch, 20.0f);
}

void playGodballImpact(const glm::vec3& position, float damageFraction) {
    float clamped = std::clamp(damageFraction, 0.0f, 1.0f);
    float volume = 0.1f + clamped * 0.9f;
    float pitch = 1.4f - clamped * 0.9f;
    pitch = std::clamp(pitch, 0.5f, 1.4f);
    printf("[SOUND] weapon=godball event=hit damageFrac=%.2f pitch=%.2f volume=%.2f\n",
           damageFraction, pitch, volume);
    playWorldSound("godballhit", position, volume, pitch, 25.0f);
}

void playSwordswordHitSound(const glm::vec3& position, float strength01) {
    float clamped = std::clamp(strength01, 0.0f, 1.0f);
    int r = rand() % 4 + 1;
    std::string name = "swordswordhit" + std::to_string(r);
    float volume = 0.1f + clamped * 0.9f;
    float pitch = 1.4f - clamped * 0.9f;
    pitch = std::clamp(pitch, 0.5f, 1.4f);
    printf("[SOUND] weapon=swordsword event=hit variant=%d strength=%.2f pitch=%.2f volume=%.2f\n",
           r, strength01, pitch, volume);
    playWorldSound(name, position, volume, pitch, 35.0f);
}

} // namespace WeaponAudio