#include "menu-avatar-preview.h"

#include <cstdio>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static void readV3(const json& o, const char* k, glm::vec3& v)
{
    if (o.contains(k) && o[k].is_array() && o[k].size() >= 3)
        v = glm::vec3(o[k][0].get<float>(), o[k][1].get<float>(), o[k][2].get<float>());
}

bool loadViewportSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    bool changed = false;
    if (j.contains("anchor")) { cfg.anchor = j["anchor"].get<std::string>(); changed = true; }
    auto l = [&](const char* k, float& f) { if (j.contains(k)) { f = j[k].get<float>(); changed = true; } };
    l("offsetX", cfg.offsetX); l("offsetY", cfg.offsetY);
    l("width", cfg.width); l("height", cfg.height);
    return changed;
}

bool loadCameraSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("camera") || !j["camera"].is_object()) return false;
    const auto& c = j["camera"];
    readV3(c, "position", cfg.cameraPosition);
    readV3(c, "target", cfg.cameraTarget);
    if (c.contains("fov")) cfg.cameraFOV = c["fov"].get<float>();
    if (c.contains("near")) cfg.cameraNear = c["near"].get<float>();
    if (c.contains("far")) cfg.cameraFar = c["far"].get<float>();
    if (c.contains("orbit_distance")) cfg.orbitDistance = c["orbit_distance"].get<float>();
    if (c.contains("orbit_height")) cfg.orbitHeight = c["orbit_height"].get<float>();
    if (c.contains("orbit_yaw")) cfg.orbitYaw = c["orbit_yaw"].get<float>();
    if (c.contains("orbit_pitch")) cfg.orbitPitch = c["orbit_pitch"].get<float>();
    return true;
}

bool loadCharacterSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("character") || !j["character"].is_object()) return false;
    const auto& ch = j["character"];
    readV3(ch, "position", cfg.characterPosition);
    readV3(ch, "rotation_degrees", cfg.characterRotationDeg);
    readV3(ch, "scale", cfg.characterScale);
    if (ch.contains("initial_yaw")) cfg.initialYaw = ch["initial_yaw"].get<float>();
    if (ch.contains("initial_pitch")) cfg.initialPitch = ch["initial_pitch"].get<float>();
    if (ch.contains("initial_roll")) cfg.initialRoll = ch["initial_roll"].get<float>();
    return true;
}

bool loadRotationSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("rotation") || !j["rotation"].is_object()) return false;
    const auto& r = j["rotation"];
    if (r.contains("enabled")) cfg.rotationEnabled = r["enabled"].get<bool>();
    if (r.contains("clockwise")) cfg.rotationClockwise = r["clockwise"].get<bool>();
    if (r.contains("pause_when_hovered")) cfg.rotationPauseWhenHovered = r["pause_when_hovered"].get<bool>();
    if (r.contains("reverse")) cfg.rotationReverse = r["reverse"].get<bool>();
    if (r.contains("degrees_per_second")) cfg.rotationDegreesPerSecond = r["degrees_per_second"].get<float>();
    if (r.contains("start_angle")) cfg.rotationStartAngle = r["start_angle"].get<float>();
    if (r.contains("acceleration")) cfg.rotationAcceleration = r["acceleration"].get<float>();
    return true;
}

bool loadLightingSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("lighting") || !j["lighting"].is_object()) return false;
    const auto& l = j["lighting"];
    readV3(l, "ambient", cfg.ambientColor);
    if (l.contains("exposure")) cfg.exposure = l["exposure"].get<float>();
    if (l.contains("brightness")) cfg.brightness = l["brightness"].get<float>();
    if (l.contains("gamma")) cfg.gamma = l["gamma"].get<float>();
    return true;
}

bool loadRenderSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("render") || !j["render"].is_object()) return false;
    const auto& r = j["render"];
    if (r.contains("draw_weapon")) cfg.drawWeapon = r["draw_weapon"].get<bool>();
    if (r.contains("draw_nameplate")) cfg.drawNameplate = r["draw_nameplate"].get<bool>();
    if (r.contains("draw_healthbar")) cfg.drawHealthbar = r["draw_healthbar"].get<bool>();
    if (r.contains("draw_shadow")) cfg.drawShadow = r["draw_shadow"].get<bool>();
    if (r.contains("draw_outline")) cfg.drawOutline = r["draw_outline"].get<bool>();
    return true;
}

bool loadAnimationSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("animation") || !j["animation"].is_object()) return false;
    const auto& a = j["animation"];
    if (a.contains("idle_animation") && a["idle_animation"].is_string())
        cfg.idleAnimation = a["idle_animation"].get<std::string>();
    if (a.contains("idle_speed")) cfg.idleSpeed = a["idle_speed"].get<float>();
    if (a.contains("idle_start_time")) cfg.idleStartTime = a["idle_start_time"].get<float>();
    if (a.contains("animation_speed")) cfg.animationSpeed = a["animation_speed"].get<float>();
    return true;
}

bool loadBackgroundSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("background") || !j["background"].is_object()) return false;
    const auto& b = j["background"];
    if (b.contains("enabled")) cfg.backgroundEnabled = b["enabled"].get<bool>();
    if (b.contains("blur")) cfg.backgroundBlur = b["blur"].get<float>();
    if (b.contains("opacity")) cfg.backgroundOpacity = b["opacity"].get<float>();
    return true;
}

bool loadModelSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("model") || !j["model"].is_object()) return false;
    const auto& m = j["model"];
    readV3(m, "offset", cfg.modelOffset);
    if (m.contains("look_at_camera")) cfg.lookAtCamera = m["look_at_camera"].get<bool>();
    if (m.contains("floor_offset")) cfg.floorOffset = m["floor_offset"].get<float>();
    return true;
}

bool loadCameraControlsSection(const json& j, MenuCharacterPreviewConfig& cfg)
{
    if (!j.contains("camera_controls") || !j["camera_controls"].is_object()) return false;
    const auto& cc = j["camera_controls"];
    if (cc.contains("zoom")) cfg.zoom = cc["zoom"].get<float>();
    if (cc.contains("min_zoom")) cfg.minZoom = cc["min_zoom"].get<float>();
    if (cc.contains("max_zoom")) cfg.maxZoom = cc["max_zoom"].get<float>();
    return true;
}

// ── Debug printing ─────────────────────────────────────────

static void printSec(const char* prefix, const char* key, float v, float p)
{
    if (v != p) printf("  %s.%s: %.2f -> %.2f\n", prefix, key, p, v);
}

static void printSec(const char* prefix, const char* key, bool v, bool p)
{
    if (v != p) printf("  %s.%s: %s -> %s\n", prefix, key, p ? "true" : "false", v ? "true" : "false");
}

static void printSec(const char* prefix, const char* key, const std::string& v, const std::string& p)
{
    if (v != p) printf("  %s.%s: %s -> %s\n", prefix, key, p.c_str(), v.c_str());
}

static void printSec3(const char* prefix, const char* key, const glm::vec3& v, const glm::vec3& p)
{
    if (glm::length(v - p) < 0.001f) return;
    printf("  %s.%s: (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f)\n", prefix, key, p.x, p.y, p.z, v.x, v.y, v.z);
}

static void printChanges(const MenuCharacterPreviewConfig& c, const MenuCharacterPreviewConfig& p)
{
    printSec("viewport", "offsetX", c.offsetX, p.offsetX);
    printSec("viewport", "offsetY", c.offsetY, p.offsetY);
    printSec("viewport", "width", c.width, p.width);
    printSec("viewport", "height", c.height, p.height);
    printSec3("camera", "position", c.cameraPosition, p.cameraPosition);
    printSec3("camera", "target", c.cameraTarget, p.cameraTarget);
    printSec("camera", "fov", c.cameraFOV, p.cameraFOV);
    printSec("camera", "orbit_distance", c.orbitDistance, p.orbitDistance);
    printSec("camera", "orbit_height", c.orbitHeight, p.orbitHeight);
    printSec("camera", "orbit_yaw", c.orbitYaw, p.orbitYaw);
    printSec("camera", "orbit_pitch", c.orbitPitch, p.orbitPitch);
    printSec3("character", "position", c.characterPosition, p.characterPosition);
    printSec3("character", "rotation_degrees", c.characterRotationDeg, p.characterRotationDeg);
    printSec3("character", "scale", c.characterScale, p.characterScale);
    printSec("character", "initial_yaw", c.initialYaw, p.initialYaw);
    printSec("character", "initial_pitch", c.initialPitch, p.initialPitch);
    printSec("character", "initial_roll", c.initialRoll, p.initialRoll);
    printSec("rotation", "enabled", c.rotationEnabled, p.rotationEnabled);
    printSec("rotation", "degrees_per_second", c.rotationDegreesPerSecond, p.rotationDegreesPerSecond);
    printSec("rotation", "clockwise", c.rotationClockwise, p.rotationClockwise);
    printSec("rotation", "start_angle", c.rotationStartAngle, p.rotationStartAngle);
    printSec("rotation", "acceleration", c.rotationAcceleration, p.rotationAcceleration);
    printSec3("lighting", "ambient", c.ambientColor, p.ambientColor);
    printSec("lighting", "exposure", c.exposure, p.exposure);
    printSec("lighting", "brightness", c.brightness, p.brightness);
    printSec("lighting", "gamma", c.gamma, p.gamma);
    printSec("render", "draw_weapon", c.drawWeapon, p.drawWeapon);
    printSec("render", "draw_nameplate", c.drawNameplate, p.drawNameplate);
    printSec("render", "draw_healthbar", c.drawHealthbar, p.drawHealthbar);
    printSec("render", "draw_shadow", c.drawShadow, p.drawShadow);
    printSec("render", "draw_outline", c.drawOutline, p.drawOutline);
    printSec("animation", "idle_animation", c.idleAnimation, p.idleAnimation);
    printSec("animation", "idle_speed", c.idleSpeed, p.idleSpeed);
    printSec("animation", "animation_speed", c.animationSpeed, p.animationSpeed);
    printSec("background", "enabled", c.backgroundEnabled, p.backgroundEnabled);
    printSec("background", "blur", c.backgroundBlur, p.backgroundBlur);
    printSec("background", "opacity", c.backgroundOpacity, p.backgroundOpacity);
    printSec3("model", "offset", c.modelOffset, p.modelOffset);
    printSec("model", "look_at_camera", c.lookAtCamera, p.lookAtCamera);
    printSec("model", "floor_offset", c.floorOffset, p.floorOffset);
    printSec("camera_controls", "zoom", c.zoom, p.zoom);
    printSec("camera_controls", "min_zoom", c.minZoom, p.minZoom);
    printSec("camera_controls", "max_zoom", c.maxZoom, p.maxZoom);
}

void MenuAvatarPreview::printMenuPreviewConfig(const MenuCharacterPreviewConfig& c, const MenuCharacterPreviewConfig& p, bool first)
{
    if (first) printf("[MENU PREVIEW] All config values:\n");
    else printf("[MENU PREVIEW] Changed values:\n");
    printChanges(c, first ? MenuCharacterPreviewConfig{} : p);
}
