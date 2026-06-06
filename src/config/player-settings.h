#pragma once

#include <string>

struct PlayerSettings {
    float fov = 100.0f;
    float masterVolume = 1.0f;
    float musicVolume = 1.0f;
    float sfxVolume = 1.0f;
    float sensitivity = 0.15f;
    int equippedSlot = 1;
    std::string outfitPath = "devscripts/character_outfit_template.png";

    float collisionSeamTolerance = 0.035f;
    float collisionMovementBias = 0.75f;
    float collisionBounceStrength = 0.16f;
    float collisionBounceMinSpeed = 7.0f;
    float collisionBounceMaxSpeed = 45.0f;

    float weaponSwayStrength = 0.12f;
    float weaponAimFollowSpeed = 12.0f;
    // float weaponRecoilStrength = 22.0f;
    // float weaponRecoilStrength = 42.0f;
    float weaponRecoilStrength = 99.0f;
    float weaponWeight = 1.0f;
    float weaponRecoilDecay = 5.0f;
    float freecamSpeed = 18.0f;

    bool debugCombat = false;
};

PlayerSettings& GetPlayerSettings();
bool LoadPlayerSettings(const std::string& account = "default");
bool SavePlayerSettings(const std::string& account = "default");
