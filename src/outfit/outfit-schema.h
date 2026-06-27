#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "map/map_common.h"

// ── Stretch modes ───────────────────────────────────────────────────
enum class StretchMode { Fit, Stretch, Crop, Repeat, Mirror };
constexpr const char* kStretchModeNames[] = {"fit", "stretch", "crop", "repeat", "mirror"};

// ── Per-face texture override ───────────────────────────────────────
struct FaceOverride {
    int stretchMode = 0;
    float rotation = 0.0f;
    float offsetX = 0.0f, offsetY = 0.0f;
    float scaleX = 1.0f, scaleY = 1.0f;
    float hue = 0.0f, saturation = 1.0f;
    float brightness = 1.0f, contrast = 1.0f;
    float opacity = 1.0f;
    glm::vec3 tint = {1.0f, 1.0f, 1.0f};
};

// ── Cosmetic instance ───────────────────────────────────────────────
struct CosmeticInstance {
    std::string id;
    Mesh renderMesh;
    std::string parentBone; // "root", "head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec4 color{1.0f};
};

// ── Per-part face assignments ───────────────────────────────────────
struct PartFaceAssignment {
    std::string textureAlias; // alias from the textures map
    FaceOverride overrides;
};

// ── Parsed outfit data ──────────────────────────────────────────────
struct OutfitData {
    std::string name;
    std::string basePath;
    // Texture alias -> relative path
    std::unordered_map<std::string, std::string> textures;
    // Part key -> face key -> assignment
    std::unordered_map<std::string, std::unordered_map<std::string, PartFaceAssignment>> faces;
    // Part key -> color
    std::unordered_map<std::string, glm::vec3> colors;
    // Cosmetics
    std::vector<CosmeticInstance> cosmetics;
    // GPU atlas texture
    GLuint atlasTexture = 0;
};

// ── Part and face key constants ─────────────────────────────────────
constexpr const char* kOutfitPartKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
constexpr int kOutfitPartCount = 6;
constexpr const char* kOutfitFaceKeys[] = {"front", "back", "left", "right", "top", "bottom"};
constexpr int kOutfitFaceCount = 6;

// ── Atlas layout constants ──────────────────────────────────────────
constexpr int kAtlasSize = 2000;
constexpr int kAtlasCell = 313;
constexpr int kAtlasGap = 8;
constexpr int kAtlasPadding = 40;
constexpr int kAtlasInset = 2;
constexpr int kAtlasUsable = kAtlasCell - kAtlasInset * 2;

// ── Utility ─────────────────────────────────────────────────────────
int outfitPartRow(const std::string& name);
int outfitFaceColumn(const std::string& name);
std::string outfitSanitizeName(const std::string& name);
