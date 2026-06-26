#pragma once
#include <string>
#include <vector>

struct CapsuleConfig {
    float radius = 0.7f;
    float height = 3.6f;
};

struct CameraConfig {
    float distance = 3.5f;
    float height = 1.0f;
    float shoulderOffset = 2.0f;
};

struct CharacterManifest {
    std::string name;
    std::string description;
    std::string author;
    std::string version = "1.0.0";

    std::string model = "character.glb";
    std::string preview;

    CapsuleConfig capsule;
    CameraConfig camera;

    float bodyPartRadius = 0.15f;
    float scale = 1.0f;

    bool hidden = false;
    std::vector<std::string> tags;
    std::vector<std::string> cosmetics;
    std::vector<std::string> skins;

    bool load(const std::string& path);
    bool isValid() const;
    std::string validationError() const;

    static CharacterManifest defaults();
};
