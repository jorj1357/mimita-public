#include "avatar.h"
#include "avatar-autosave.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <numeric>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "entities/player.h"
#include "world/texture-store.h"
#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "cosmetic-system.h"
#include "stb_image.h"

// Set by applyToPlayer before loadModel for per-avatar body part overrides
extern nlohmann::json gAvatarBodypartOverrides;



using json = nlohmann::json;

extern TextureStore gTextures;

// FaceVector
FaceSettings& FaceVector::byName(const std::string& name) {
    if (name == "front") return front;
    if (name == "back") return back;
    if (name == "left") return left;
    if (name == "right") return right;
    if (name == "top") return top;
    return bottom;
}

const FaceSettings& FaceVector::byName(const std::string& name) const {
    if (name == "front") return front;
    if (name == "back") return back;
    if (name == "left") return left;
    if (name == "right") return right;
    if (name == "top") return top;
    return bottom;
}

// AvatarDefinition
void AvatarDefinition::expandSimple() {
    auto fill = [](FaceVector& part, const std::string& all) {
        part.front.texture = all;
        part.back.texture = all;
        part.left.texture = all;
        part.right.texture = all;
        part.top.texture = all;
        part.bottom.texture = all;
    };
    fill(head, simple.skin);
    head.front.texture = simple.face.empty() ? simple.skin : simple.face;
    fill(torso, simple.shirt);
    fill(leftArm, simple.shirt);
    fill(rightArm, simple.shirt);
    fill(leftLeg, simple.pants);
    fill(rightLeg, simple.pants);
}

FaceSettings AvatarDefinition::resolve(const std::string& part, const std::string& face) const {
    auto getPart = [&]() -> const FaceVector* {
        if (part == "head") return &head;
        if (part == "torso") return &torso;
        if (part == "leftArm") return &leftArm;
        if (part == "rightArm") return &rightArm;
        if (part == "leftLeg") return &leftLeg;
        if (part == "rightLeg") return &rightLeg;
        return nullptr;
    };
    const FaceVector* p = getPart();
    if (!p) return {};
    return p->byName(face);
}

void AvatarDefinition::clear() {
    format_version = 2;
    avatar_id.clear();
    created_at.clear();
    updated_at.clear();
    name.clear();
    basePath.clear();
    simple = {};
    advancedMode = false;
    head = {};
    torso = {};
    leftArm = {};
    rightArm = {};
    leftLeg = {};
    rightLeg = {};
    colors = {};
    cosmetics.clear();
    bodypartOverrides = nullptr;
    activePreset.clear();
    playerModel.clear();
    textureMode = "legacy_faces";
    atlasPath.clear();
    alphaMode = "blend";
    alphaCutoff = 0.5f;
    unlit = false;
}

// AvatarSystem
AvatarSystem& AvatarSystem::instance() {
    static AvatarSystem sys;
    return sys;
}

std::string AvatarSystem::avatarPath(const std::string& name) {
    return "assets/avatars/" + name;
}

std::string AvatarSystem::resolvePath(const std::string& relativePath) const {
    if (relativePath.empty()) return {};
    if (relativePath.find('/') != std::string::npos ||
        relativePath.find('\\') != std::string::npos) {
        return relativePath;
    }
    return mBasePath + "/" + relativePath;
}

// ── JSON helpers ────────────────────────────────────────────────────
static json serializeTransform(const FaceTransform& t) {
    json obj;
    if (t.offsetX != 0.0f) obj["offset_x"] = (int)t.offsetX;
    if (t.offsetY != 0.0f) obj["offset_y"] = (int)t.offsetY;
    if (t.scaleX != 1.0f) obj["scale_x"] = t.scaleX;
    if (t.scaleY != 1.0f) obj["scale_y"] = t.scaleY;
    if (t.rotation != 0.0f) obj["rotation"] = t.rotation;
    if (t.hueShift != 0.0f) obj["hue_shift"] = t.hueShift;
    if (t.saturation != 0.0f) obj["saturation"] = t.saturation;
    if (t.brightness != 0.0f) obj["brightness"] = t.brightness;
    if (t.contrast != 1.0f) obj["contrast"] = t.contrast;
    if (t.stretchMode != 0) obj["stretch_mode"] = t.stretchMode == 1 ? "crop" : "stretch";
    if (t.color != glm::vec3(1.0f)) obj["color"] = {t.color.r, t.color.g, t.color.b};
    if (t.transparency != 0.0f) obj["opacity"] = 1.0f - t.transparency;
    return obj;
}

static FaceTransform parseTransform(const json& j) {
    FaceTransform t;
    t.offsetX = (float)j.value("offset_x", 0);
    t.offsetY = (float)j.value("offset_y", 0);
    t.scaleX = j.value("scale_x", 1.0f);
    t.scaleY = j.value("scale_y", 1.0f);
    t.rotation = j.value("rotation", 0.0f);
    t.hueShift = j.value("hue_shift", 0.0f);
    t.saturation = j.value("saturation", 0.0f);
    t.brightness = j.value("brightness", 0.0f);
    t.contrast = j.value("contrast", 1.0f);
    std::string sm = j.value("stretch_mode", "stretch");
    t.stretchMode = (sm == "crop") ? 1 : 0;
    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3)
        t.color = glm::vec3(j["color"][0], j["color"][1], j["color"][2]);
    if (j.contains("opacity"))
        t.transparency = 1.0f - j.value("opacity", 1.0f);
    else
        t.transparency = j.value("transparency", 0.0f);
    return t;
}

// Serialize a single face slot (new format: object, or old format: string)
static json serializeFaceSlot(const FaceSettings& fs) {
    json trans = serializeTransform(fs.transform);
    // If the only thing to write is the texture, write as string for compactness
    if (trans.empty())
        return fs.texture;
    trans["texture"] = fs.texture;
    return trans;
}

// Parse a single face slot — supports both old (string) and new (object) format
static std::string parseFaceSlot(const json& j, std::string& outTexture, FaceTransform& outTransform) {
    outTransform = {};
    if (j.is_string()) {
        outTexture = j.get<std::string>();
    } else if (j.is_object()) {
        outTexture = j.value("texture", "");
        outTransform = parseTransform(j);
    }
    return outTexture;
}

static json serializePartColors(const PartColors& c) {
    json j;
    j["head"]     = {c.head.x, c.head.y, c.head.z};
    j["torso"]    = {c.torso.x, c.torso.y, c.torso.z};
    j["leftArm"]  = {c.leftArm.x, c.leftArm.y, c.leftArm.z};
    j["rightArm"] = {c.rightArm.x, c.rightArm.y, c.rightArm.z};
    j["leftLeg"]  = {c.leftLeg.x, c.leftLeg.y, c.leftLeg.z};
    j["rightLeg"] = {c.rightLeg.x, c.rightLeg.y, c.rightLeg.z};
    return j;
}

static PartColors parsePartColors(const json& j) {
    PartColors c;
    auto read = [&](const std::string& key, glm::vec3& out) {
        if (j.contains(key) && j[key].is_array() && j[key].size() >= 3)
            out = {j[key][0], j[key][1], j[key][2]};
    };
    read("head", c.head);
    read("torso", c.torso);
    read("leftArm", c.leftArm);
    read("rightArm", c.rightArm);
    read("leftLeg", c.leftLeg);
    read("rightLeg", c.rightLeg);
    return c;
}

static json vec3Json(const glm::vec3& value) {
    return {value.x, value.y, value.z};
}

static glm::vec3 parseVec3(const json& value, const glm::vec3& fallback) {
    if (!value.is_array() || value.size() < 3)
        return fallback;
    return {
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>()
    };
}

static json serializeCosmeticTexture(const CosmeticTexture& texture) {
    return {
        {"image", texture.image},
        {"offset_x", texture.offsetX},
        {"offset_y", texture.offsetY},
        {"scale_x", texture.scaleX},
        {"scale_y", texture.scaleY},
        {"rotation", texture.rotation},
        {"color", vec3Json(texture.color)},
        {"brightness", texture.brightness},
        {"opacity", texture.opacity}
    };
}

static CosmeticTexture parseCosmeticTexture(const json& value) {
    CosmeticTexture result;
    if (!value.is_object())
        return result;

    result.image = value.value("image", "");
    result.offsetX = value.value("offset_x", 0.0f);
    result.offsetY = value.value("offset_y", 0.0f);
    result.scaleX = value.value("scale_x", 1.0f);
    result.scaleY = value.value("scale_y", 1.0f);
    result.rotation = value.value("rotation", 0.0f);
    result.color = parseVec3(value.value("color", json()), glm::vec3(1.0f));
    result.brightness = value.value("brightness", 1.0f);
    result.opacity = value.value("opacity", 1.0f);
    return result;
}

static json serializeCosmetics(const std::vector<CosmeticSlot>& cosmetics) {
    json arr = json::array();
    for (const auto& c : cosmetics) {
        const std::string id = c.id.empty() ? c.choice : c.id;
        const std::string glb = c.glb.empty() ? c.choice : c.glb;
        const std::string anchor = c.anchorPart.empty() ? c.attachTo : c.anchorPart;
        arr.push_back({
            {"id", id},
            {"type", c.type},
            {"glb", glb},
            {"enabled", c.enabled},
            {"anchor_part", anchor},
            {"offset", vec3Json(c.offset)},
            {"rotation", vec3Json(c.rotation)},
            {"scale", vec3Json(c.scale)},
            {"texture", serializeCosmeticTexture(c.texture)},
            // Keep legacy keys for older readers and old tooling.
            {"slot", c.slot},
            {"choice", c.choice.empty() ? glb : c.choice},
            {"attachTo", c.attachTo.empty() ? anchor : c.attachTo},
            {"color", vec3Json(c.color)}
        });
    }
    return arr;
}

static std::vector<CosmeticSlot> parseCosmetics(const json& arr) {
    std::vector<CosmeticSlot> result;
    if (!arr.is_array()) return result;
    for (const auto& j : arr) {
        if (!j.is_object())
            continue;

        CosmeticSlot cosmetic;
        cosmetic.slot = j.value("slot", "");
        cosmetic.choice = j.value("choice", "");
        cosmetic.attachTo = j.value("attachTo", "");
        cosmetic.offset = parseVec3(j.value("offset", json()), glm::vec3(0.0f));
        cosmetic.rotation = parseVec3(j.value("rotation", json()), glm::vec3(0.0f));
        cosmetic.scale = parseVec3(j.value("scale", json()), glm::vec3(1.0f));
        cosmetic.color = parseVec3(j.value("color", json()), glm::vec3(1.0f));

        cosmetic.id = j.value("id", cosmetic.choice);
        cosmetic.type = j.value("type", "");
        cosmetic.glb = j.value("glb", cosmetic.choice);
        cosmetic.enabled = j.value("enabled", true);
        cosmetic.anchorPart = j.value("anchor_part", cosmetic.attachTo);
        cosmetic.texture = parseCosmeticTexture(j.value("texture", json()));

        if (cosmetic.choice.empty())
            cosmetic.choice = cosmetic.glb;
        if (cosmetic.glb.empty())
            cosmetic.glb = cosmetic.choice;
        if (cosmetic.attachTo.empty())
            cosmetic.attachTo = cosmetic.anchorPart;
        if (cosmetic.anchorPart.empty())
            cosmetic.anchorPart = cosmetic.attachTo.empty() ? "torso" : cosmetic.attachTo;
        if (cosmetic.texture.color == glm::vec3(1.0f) && cosmetic.color != glm::vec3(1.0f))
            cosmetic.texture.color = cosmetic.color;

        result.push_back(std::move(cosmetic));
    }
    return result;
}

// ── Parse face data from "advanced" section ─────────────────────────
static void parsePartFacesFromAdvanced(const json& adv, const std::string& prefix,
                                        FaceVector& out, const std::string& fallbackTexture)
{
    const char* faceNames[] = {"front", "back", "left", "right", "top", "bottom"};
    for (const char* fn : faceNames) {
        std::string key = prefix + "_" + fn;
        FaceSettings& fs = out.byName(fn);
        if (adv.contains(key)) {
            const json& val = adv[key];
            FaceTransform tf;
            parseFaceSlot(val, fs.texture, tf);
            fs.transform = tf;
        } else {
            fs.texture = fallbackTexture;
        }
    }
}

// ── V2 avatar serialization ─────────────────────────────────────────
static void serializePartFacesToAdvanced(json& adv, const std::string& part, const FaceVector& fv) {
    const char* faceNames[] = {"front", "back", "left", "right", "top", "bottom"};
    for (const char* fn : faceNames) {
        const FaceSettings& fs = fv.byName(fn);
        adv[part + "_" + fn] = serializeFaceSlot(fs);
    }
}

void avatarToJson(const AvatarDefinition& avatar, json& j) {
    j = json::object();
    j["format_version"] = avatar.format_version;
    if (!avatar.avatar_id.empty()) j["avatar_id"] = avatar.avatar_id;
    if (!avatar.created_at.empty()) j["created_at"] = avatar.created_at;
    j["updated_at"] = avatar.updated_at.empty() ? std::string("now") : avatar.updated_at;
    j["name"] = avatar.name;

    j["simple"]["face"] = avatar.simple.face;
    j["simple"]["shirt"] = avatar.simple.shirt;
    j["simple"]["pants"] = avatar.simple.pants;
    j["simple"]["skin"] = avatar.simple.skin;
    j["advanced_mode"] = avatar.advancedMode;

    json adv = json::object();
    serializePartFacesToAdvanced(adv, "head", avatar.head);
    serializePartFacesToAdvanced(adv, "torso", avatar.torso);
    serializePartFacesToAdvanced(adv, "leftArm", avatar.leftArm);
    serializePartFacesToAdvanced(adv, "rightArm", avatar.rightArm);
    serializePartFacesToAdvanced(adv, "leftLeg", avatar.leftLeg);
    serializePartFacesToAdvanced(adv, "rightLeg", avatar.rightLeg);
    j["advanced"] = adv;

    j["colors"] = serializePartColors(avatar.colors);
    j["cosmetics"] = serializeCosmetics(avatar.cosmetics);
    if (!avatar.activePreset.empty()) j["active_preset"] = avatar.activePreset;
    if (!avatar.playerModel.empty()) j["player_model"] = avatar.playerModel;
    if (!avatar.bodypartOverrides.is_null())
        j["bodyparts"] = avatar.bodypartOverrides;

    if (avatar.textureMode == "uv_atlas") {
        j["texture_mode"] = "uv_atlas";
        if (!avatar.atlasPath.empty()) j["atlas"] = avatar.atlasPath;
        j["alpha_mode"] = avatar.alphaMode;
        j["alpha_cutoff"] = avatar.alphaCutoff;
        j["unlit"] = avatar.unlit;
    }
}

// ── Load / Save ─────────────────────────────────────────────────────
bool AvatarSystem::loadAvatar(const std::string& avatarName) {
    markAtlasDirty();
    mAvatarName = avatarName;
    mBasePath = avatarPath(avatarName);
    mAvatar.clear();
    mAvatar.name = avatarName;
    mAvatar.basePath = mBasePath;

    // Set up autosave
    mAutosave.setBasePath(mBasePath);

    const std::string jsonPath = mBasePath + "/avatar.json";
    printf("[AVATAR] Loading avatar: %s\n", avatarName.c_str());
    printf("[AVATAR] Path: %s\n", jsonPath.c_str());

    // Try recovery if primary file is missing or corrupt
    std::string recoveryMsg;
    if (!std::filesystem::exists(jsonPath)) {
        // No primary file — try to recover
        if (mAutosave.recover(mAvatar, recoveryMsg)) {
            Terminal::instance().addLog("[AVATAR] " + recoveryMsg + ": " + jsonPath);
            printf("[AVATAR] Recovery: %s\n", recoveryMsg.c_str());
        } else {
            Terminal::instance().addLog("[AVATAR] No avatar.json found at " + jsonPath + "; using defaults");
            mAutosave.setBasePath(mBasePath);
            mHasAvatar = true;
            return true;
        }
    }

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        Terminal::instance().addLog("[AVATAR] Cannot open " + jsonPath);
        mHasAvatar = true;
        return true;
    }

    try {
        json root;
        file >> root;

        // ── Build texture alias table (optional) ──────────────────────
        std::unordered_map<std::string, std::string> textureAliases;
        if (root.contains("textures") && root["textures"].is_object()) {
            printf("[AVATAR] Texture aliases:\n");
            for (auto& [alias, val] : root["textures"].items()) {
                std::string filename = val.get<std::string>();
                std::string resolved = mBasePath + "/" + filename;
                textureAliases[alias] = filename;
                printf("[AVATAR]   %s -> %s (%s)\n", alias.c_str(), filename.c_str(),
                       std::filesystem::exists(resolved) ? "exists" : "MISSING");
                if (!std::filesystem::exists(resolved))
                    Terminal::instance().addLog("[AVATAR] Missing texture: " + resolved);
            }
        }

        auto resolveAlias = [&](const std::string& val) -> std::string {
            auto it = textureAliases.find(val);
            return (it != textureAliases.end()) ? it->second : val;
        };

        // ── Detect format ─────────────────────────────────────────────
        bool hasAdvanced = root.contains("advanced") && root["advanced"].is_object();
        bool hasSimple = root.contains("simple") && root["simple"].is_object();

        if (hasAdvanced) {
            mAvatar.advancedMode = true;
            printf("[AVATAR] Format: advanced\n");

            auto& adv = root["advanced"];
            const char* partKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
            FaceVector AvatarDefinition::*partPtrs[] = {
                &AvatarDefinition::head, &AvatarDefinition::torso,
                &AvatarDefinition::leftArm, &AvatarDefinition::rightArm,
                &AvatarDefinition::leftLeg, &AvatarDefinition::rightLeg
            };

            for (int pi = 0; pi < 6; ++pi) {
                FaceVector& part = mAvatar.*partPtrs[pi];
                // Try to read per-face settings (new object format)
                // Assemble part name for prefix: e.g. "head", "leftArm"
                const char* prefix = partKeys[pi];
                parsePartFacesFromAdvanced(adv, prefix, part, "");

                // Resolve texture aliases
                const char* faceNames[] = {"front", "back", "left", "right", "top", "bottom"};
                for (const char* fn : faceNames)
                    part.byName(fn).texture = resolveAlias(part.byName(fn).texture);
            }
            printf("[AVATAR] Loaded advanced: %s\n", avatarName.c_str());

        } else if (hasSimple) {
            mAvatar.advancedMode = false;
            printf("[AVATAR] Format: simple\n");
            auto& s = root["simple"];
            mAvatar.simple.face  = resolveAlias(s.value("face", ""));
            mAvatar.simple.shirt = resolveAlias(s.value("shirt", ""));
            mAvatar.simple.pants = resolveAlias(s.value("pants", ""));
            mAvatar.simple.skin  = resolveAlias(s.value("skin", ""));
            mAvatar.expandSimple();
            printf("[AVATAR]   face=%s shirt=%s pants=%s skin=%s\n",
                   mAvatar.simple.face.c_str(), mAvatar.simple.shirt.c_str(),
                   mAvatar.simple.pants.c_str(), mAvatar.simple.skin.c_str());
        } else {
            printf("[AVATAR] ERROR: avatar.json has no recognizable format\n");
            Terminal::instance().addLog("[AVATAR] Unrecognized format in " + jsonPath);
            mHasAvatar = true;
            return false;
        }

        // ── V2 fields ──────────────────────────────────────────────────
        mAvatar.format_version = root.value("format_version", 1);
        mAvatar.avatar_id = root.value("avatar_id", "");
        mAvatar.created_at = root.value("created_at", "");
        mAvatar.updated_at = root.value("updated_at", "");

        // Generate avatar_id if missing (v1 → v2 migration)
        if (mAvatar.avatar_id.empty()) {
            std::random_device rd;
            std::mt19937_64 rng(rd());
            char buf[24];
            std::snprintf(buf, sizeof(buf), "local_%016llx", (unsigned long long)rng());
            mAvatar.avatar_id = buf;
            printf("[AVATAR] Generated avatar_id: %s (v1 migration)\n", mAvatar.avatar_id.c_str());
        }

        // ── Shared fields ─────────────────────────────────────────────
        if (root.contains("colors"))
            mAvatar.colors = parsePartColors(root["colors"]);
        if (root.contains("cosmetics"))
            mAvatar.cosmetics = parseCosmetics(root["cosmetics"]);
        mAvatar.activePreset = root.value("active_preset", "");

        // ── Optional player_model / character_model ────────────────────
        // Prefer character_model if both are set
        std::string pm;
        if (root.contains("character_model") && root["character_model"].is_string())
            pm = root["character_model"].get<std::string>();
        else if (root.contains("player_model") && root["player_model"].is_string())
            pm = root["player_model"].get<std::string>();

        if (!pm.empty()) {
            printf("[AVATAR] player_model field: %s\n", pm.c_str());
            if (std::filesystem::exists(pm)) {
                mAvatar.playerModel = pm;
                printf("[AVATAR] Using custom player model from avatar.json\n");
            } else {
                printf("[AVATAR ERROR] player_model path does not exist:\n  %s\n", pm.c_str());
                Terminal::instance().addLog("[AVATAR ERROR] player_model not found: " + pm);
            }
        } else {
            printf("[AVATAR] No player_model field; using default player model\n");
        }

        // ── Per-avatar body part overrides (offset/rotation/scale) ──
        if (root.contains("bodyparts") && root["bodyparts"].is_object()) {
            mAvatar.bodypartOverrides = root["bodyparts"];
            printf("[AVATAR] loaded bodypart overrides for: ");
            for (auto it = mAvatar.bodypartOverrides.begin(); it != mAvatar.bodypartOverrides.end(); ++it)
                printf("%s ", it.key().c_str());
            printf("\n");
        }

        // ── UV atlas mode ──────────────────────────────────────────
        mAvatar.textureMode = root.value("texture_mode", "legacy_faces");
        if (mAvatar.textureMode == "uv_atlas") {
            if (root.contains("atlas") && root["atlas"].is_string()) {
                std::string atPath = root["atlas"].get<std::string>();
                mAvatar.atlasPath = resolvePath(atPath);
                printf("[AVATAR] uv_atlas mode: atlas=%s\n", mAvatar.atlasPath.c_str());
                if (!std::filesystem::exists(mAvatar.atlasPath))
                    Terminal::instance().addLog("[AVATAR] Atlas file not found: " + mAvatar.atlasPath);
            }
            mAvatar.alphaMode = root.value("alpha_mode", "blend");
            mAvatar.alphaCutoff = (float)root.value("alpha_cutoff", 0.5);
            mAvatar.unlit = root.value("unlit", false);
            printf("[AVATAR]   alpha_mode=%s alpha_cutoff=%.2f unlit=%d\n",
                   mAvatar.alphaMode.c_str(), mAvatar.alphaCutoff, (int)mAvatar.unlit);
        }

        mHasAvatar = true;
        mSaveRequested = false;
        mLastSavedAvatar = mAvatar;
        mEditorUndoHistory.clear();
        mEditorChangeCaptured = false;
        if (std::filesystem::exists(jsonPath))
            mLastWriteTime = std::filesystem::last_write_time(jsonPath);
        mLastCheckTime = std::chrono::steady_clock::now();
        Terminal::instance().addLog("[AVATAR] Loaded avatar: " + avatarName);
        return true;
    } catch (const std::exception& e) {
        Terminal::instance().addLog("[AVATAR] Failed to parse avatar.json: " + std::string(e.what()));
        printf("[AVATAR] Parse error: %s\n", e.what());
        mHasAvatar = true;
        return false;
    }
}

bool AvatarSystem::saveSimple(const std::string& avatarName, const SimpleAvatar& simple) {
    const std::string base = avatarPath(avatarName);
    std::filesystem::create_directories(base);

    json root;
    root["name"] = avatarName;
    root["version"] = 1;
    root["advanced_mode"] = false;
    root["simple"]["face"] = simple.face;
    root["simple"]["shirt"] = simple.shirt;
    root["simple"]["pants"] = simple.pants;
    root["simple"]["skin"] = simple.skin;
    root["advanced"] = json::object();

    auto fillFace = [&](const std::string& key, const std::string& val) {
        root["advanced"][key] = val;  // String format for simple avatars
    };
    fillFace("head_front", simple.face.empty() ? simple.skin : simple.face);
    for (const char* f : {"back", "left", "right", "top", "bottom"})
        fillFace(std::string("head_") + f, simple.skin);
    for (const char* part : {"torso", "leftArm", "rightArm"}) {
        for (const char* f : {"front", "back", "left", "right", "top", "bottom"})
            fillFace(std::string(part) + "_" + f, simple.shirt);
    }
    for (const char* part : {"leftLeg", "rightLeg"}) {
        for (const char* f : {"front", "back", "left", "right", "top", "bottom"})
            fillFace(std::string(part) + "_" + f, simple.pants);
    }

    root["colors"] = serializePartColors(mAvatar.colors);
    root["cosmetics"] = serializeCosmetics(mAvatar.cosmetics);
    if (!mAvatar.activePreset.empty())
        root["active_preset"] = mAvatar.activePreset;

    const std::string tmp = base + "/avatar.json.tmp";
    std::ofstream out(tmp);
    if (!out.is_open()) return false;
    out << root.dump(2);
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmp, base + "/avatar.json", ec);
    return !ec;
}

bool AvatarSystem::saveAdvanced(const std::string& avatarName, const AvatarDefinition& def) {
    const std::string base = avatarPath(avatarName);
    std::filesystem::create_directories(base);

    json root;
    root["name"] = avatarName;
    root["version"] = 1;
    root["advanced_mode"] = true;

    root["simple"]["face"] = def.simple.face;
    root["simple"]["shirt"] = def.simple.shirt;
    root["simple"]["pants"] = def.simple.pants;
    root["simple"]["skin"] = def.simple.skin;

    root["advanced"] = json::object();
    auto writePart = [&](const std::string& p, const FaceVector& part) {
        const char* faceNames[] = {"front", "back", "left", "right", "top", "bottom"};
        for (const char* fn : faceNames) {
            const FaceSettings& fs = part.byName(fn);
            root["advanced"][p + "_" + fn] = serializeFaceSlot(fs);
        }
    };
    writePart("head", def.head);
    writePart("torso", def.torso);
    writePart("leftArm", def.leftArm);
    writePart("rightArm", def.rightArm);
    writePart("leftLeg", def.leftLeg);
    writePart("rightLeg", def.rightLeg);

    root["colors"] = serializePartColors(def.colors);
    root["cosmetics"] = serializeCosmetics(def.cosmetics);
    if (!def.activePreset.empty())
        root["active_preset"] = def.activePreset;

    const std::string tmp = base + "/avatar.json.tmp";
    std::ofstream out(tmp);
    if (!out.is_open()) return false;
    out << root.dump(2);
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmp, base + "/avatar.json", ec);
    return !ec;
}

void AvatarSystem::setPartFace(const std::string& part, const std::string& face, const std::string& texturePath) {
    auto getPart = [&]() -> FaceVector* {
        if (part == "head") return &mAvatar.head;
        if (part == "torso") return &mAvatar.torso;
        if (part == "leftArm") return &mAvatar.leftArm;
        if (part == "rightArm") return &mAvatar.rightArm;
        if (part == "leftLeg") return &mAvatar.leftLeg;
        if (part == "rightLeg") return &mAvatar.rightLeg;
        return nullptr;
    };
    FaceVector* p = getPart();
    if (p) p->byName(face).texture = texturePath;
    markAtlasDirty();
}

void AvatarSystem::setPartFaceTransform(const std::string& part, const std::string& face, const FaceTransform& transform) {
    auto getPart = [&]() -> FaceVector* {
        if (part == "head") return &mAvatar.head;
        if (part == "torso") return &mAvatar.torso;
        if (part == "leftArm") return &mAvatar.leftArm;
        if (part == "rightArm") return &mAvatar.rightArm;
        if (part == "leftLeg") return &mAvatar.leftLeg;
        if (part == "rightLeg") return &mAvatar.rightLeg;
        return nullptr;
    };
    FaceVector* p = getPart();
    if (p) p->byName(face).transform = transform;
    markAtlasDirty();
}

void AvatarSystem::setBodypartOverride(Player& player, const std::string& part,
                                       const glm::vec3& offset,
                                       const glm::vec3& rotation,
                                       const glm::vec3& scale) {
    if (mAvatar.bodypartOverrides.is_null() || !mAvatar.bodypartOverrides.is_object())
        mAvatar.bodypartOverrides = json::object();

    mAvatar.bodypartOverrides[part] = {
        {"offset", {offset.x, offset.y, offset.z}},
        {"rotation", {rotation.x, rotation.y, rotation.z}},
        {"scale", {scale.x, scale.y, scale.z}}
    };
    gAvatarBodypartOverrides = mAvatar.bodypartOverrides;

    glm::mat4 local = glm::translate(glm::mat4(1.0f), offset);
    local = glm::rotate(local, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    local = glm::rotate(local, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    local = glm::scale(local, scale);

    for (auto& node : player.perfectPoseSkeleton.nodes) {
        if (node.name == part)
            node.localTransform = local;
    }
    player.updateModelWorldTransforms();
}

void AvatarSystem::setPartColor(const std::string& part, const glm::vec3& color) {
    if (part == "head") mAvatar.colors.head = color;
    else if (part == "torso") mAvatar.colors.torso = color;
    else if (part == "leftArm") mAvatar.colors.leftArm = color;
    else if (part == "rightArm") mAvatar.colors.rightArm = color;
    else if (part == "leftLeg") mAvatar.colors.leftLeg = color;
    else if (part == "rightLeg") mAvatar.colors.rightLeg = color;
    markAtlasDirty();
}

glm::vec3 AvatarSystem::partColor(const std::string& part) const {
    if (part == "head") return mAvatar.colors.head;
    if (part == "torso") return mAvatar.colors.torso;
    if (part == "leftArm") return mAvatar.colors.leftArm;
    if (part == "rightArm") return mAvatar.colors.rightArm;
    if (part == "leftLeg") return mAvatar.colors.leftLeg;
    if (part == "rightLeg") return mAvatar.colors.rightLeg;
    return glm::vec3(1.0f);
}

bool AvatarSystem::savePreset(const std::string& presetName) {
    const std::string base = avatarPath(mAvatarName);
    std::filesystem::create_directories(base + "/presets");

    json root;
    root["name"] = presetName;
    root["colors"] = serializePartColors(mAvatar.colors);

    json adv = json::object();
    auto writePart = [&](const std::string& p, const FaceVector& part) {
        const char* faceNames[] = {"front", "back", "left", "right", "top", "bottom"};
        for (const char* fn : faceNames)
            adv[p + "_" + fn] = serializeFaceSlot(part.byName(fn));
    };
    writePart("head", mAvatar.head);
    writePart("torso", mAvatar.torso);
    writePart("leftArm", mAvatar.leftArm);
    writePart("rightArm", mAvatar.rightArm);
    writePart("leftLeg", mAvatar.leftLeg);
    writePart("rightLeg", mAvatar.rightLeg);
    root["advanced"] = adv;
    root["cosmetics"] = serializeCosmetics(mAvatar.cosmetics);

    const std::string path = base + "/presets/" + presetName + ".json";
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << root.dump(2);
    out.close();
    Terminal::instance().addLog("[AVATAR] Saved preset: " + presetName);
    return true;
}

bool AvatarSystem::loadPreset(const std::string& presetName) {
    const std::string path = avatarPath(mAvatarName) + "/presets/" + presetName + ".json";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json root;
        file >> root;

        if (root.contains("colors"))
            mAvatar.colors = parsePartColors(root["colors"]);

        if (root.contains("advanced")) {
            auto& adv = root["advanced"];
            const char* partKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
            FaceVector AvatarDefinition::*partPtrs[] = {
                &AvatarDefinition::head, &AvatarDefinition::torso,
                &AvatarDefinition::leftArm, &AvatarDefinition::rightArm,
                &AvatarDefinition::leftLeg, &AvatarDefinition::rightLeg
            };
            for (int pi = 0; pi < 6; ++pi)
                parsePartFacesFromAdvanced(adv, partKeys[pi], mAvatar.*partPtrs[pi], "");
        }

        if (root.contains("cosmetics"))
            mAvatar.cosmetics = parseCosmetics(root["cosmetics"]);

        mAvatar.activePreset = presetName;
        Terminal::instance().addLog("[AVATAR] Loaded preset: " + presetName);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> AvatarSystem::listPresets() const {
    std::vector<std::string> result;
    const std::string dir = avatarPath(mAvatarName) + "/presets";
    if (!std::filesystem::exists(dir)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            result.push_back(entry.path().stem().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> AvatarSystem::listAvatars() const {
    std::vector<std::string> result;
    const std::string dir = "assets/avatars";
    if (!std::filesystem::exists(dir))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_directory()) continue;
        std::ifstream file(entry.path() / "avatar.json");
        if (!file.is_open()) continue;
        try {
            json root;
            file >> root;
            const bool hasAvatarData = root.is_object() &&
                ((root.contains("advanced") && root["advanced"].is_object()) ||
                 (root.contains("simple") && root["simple"].is_object()));
            if (hasAvatarData)
                result.push_back(entry.path().filename().string());
        } catch (...) {
            // Invalid avatar folders are intentionally omitted from the editor.
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> AvatarSystem::listPngs(const std::string& avatarName) const {
    std::vector<std::string> result;
    const std::string dir = avatarPath(avatarName);
    if (!std::filesystem::exists(dir))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png")
                result.push_back(entry.path().filename().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

static bool validatePlayerBodyParts(const Player& player) {
    const char* required[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    bool allFound = true;
    std::string missing;
    for (const char* name : required) {
        bool found = false;
        for (const auto& part : player.physicalBody.parts) {
            if (part.name == name) { found = true; break; }
        }
        if (!found) {
            if (!missing.empty()) missing += ", ";
            missing += name;
            allFound = false;
        }
    }
    if (!allFound) {
        printf("[AVATAR ERROR] Avatar player_model failed validation:\n");
        printf("  missing required node%s: %s\n",
               missing.find(',') != std::string::npos ? "s" : "", missing.c_str());
        printf("  actual node names:");
        for (const auto& part : player.physicalBody.parts)
            printf(" %s", part.name.c_str());
        printf("\n");
        Terminal::instance().addLog("[AVATAR] Model validation failed: missing " + missing);
    }
    return allFound;
}

bool AvatarSystem::applyToPlayer(Player& player, bool reloadTextures) {
    if (!mHasAvatar || mAvatarName.empty()) return false;

    // If avatar specifies a custom player model, load it first
    if (!mAvatar.playerModel.empty()) {
        printf("[AVATAR] Loading GLB: %s\n", mAvatar.playerModel.c_str());
        // Pass per-avatar bodypart overrides to the model loader
        if (!mAvatar.bodypartOverrides.is_null())
            gAvatarBodypartOverrides = mAvatar.bodypartOverrides;
        bool modelOk = player.loadModel(mAvatar.playerModel.c_str());
        if (modelOk) {
            validatePlayerBodyParts(player);
        } else {
            printf("[AVATAR ERROR] Failed to load player model: %s\n", mAvatar.playerModel.c_str());
            Terminal::instance().addLog("[AVATAR] Failed to load model: " + mAvatar.playerModel);
        }
    }

    // ── Legacy faces mode (existing working behavior) ─────────
    if (!buildAtlas(player, reloadTextures))
        return false;
    printf("[AVATAR] Applying avatar texture atlas after model load\n");
    bool applied = applyAtlasToPlayer(player);

    // ── Cosmetics (in-game GLB attachments) ─────────────────────
    if (applied && !mAvatar.cosmetics.empty()) {
        CosmeticSystem::instance().loadCosmetics(mAvatar.cosmetics);
        player.setCosmetics(mAvatar.cosmetics);
    }
    return applied;
}

void AvatarSystem::autosaveUpdate(float dt) {
    if (!mHasAvatar || mAvatarName.empty()) return;
    if (!mSaveRequested) return;

    // Save dirty editor data at the same cadence as hot-reload polling.
    mAutosave.update(dt, [this]() -> bool {
        return saveProject();
    });
}

bool AvatarSystem::saveProject() {
    if (!mHasAvatar || mBasePath.empty()) return false;
    // Update timestamp
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &local);
    mAvatar.updated_at = ts;
    if (mAvatar.created_at.empty())
        mAvatar.created_at = ts;

    const bool ok = mAutosave.saveNow(mAvatar);
    if (ok) {
        mSaveRequested = false;
        mLastSavedAvatar = mAvatar;
        mEditorChangeCaptured = false;
        const std::string jsonPath = mBasePath + "/avatar.json";
        std::error_code ec;
        if (std::filesystem::exists(jsonPath, ec))
            mLastWriteTime = std::filesystem::last_write_time(jsonPath, ec);
        mLastCheckTime = std::chrono::steady_clock::now();
    }
    return ok;
}

void AvatarSystem::triggerSave() {
    if (mHasAvatar && !mEditorChangeCaptured) {
        mEditorUndoHistory.push_back(mLastSavedAvatar);
        constexpr size_t kMaxUndoSteps = 30;
        if (mEditorUndoHistory.size() > kMaxUndoSteps)
            mEditorUndoHistory.erase(mEditorUndoHistory.begin());
        mEditorChangeCaptured = true;
    }
    mSaveRequested = true;
}

bool AvatarSystem::undoEditorChange()
{
    if (!mHasAvatar || mEditorUndoHistory.empty())
        return false;
    mAvatar = mEditorUndoHistory.back();
    mEditorUndoHistory.pop_back();
    mEditorChangeCaptured = true;
    markAtlasDirty();
    mSaveRequested = true;
    return true;
}

void AvatarSystem::pollHotReload() {
    if (!mHasAvatar || mAvatarName.empty()) {
        // printf("[AVATAR HR] pollHotReload SKIP: mHasAvatar=%d mAvatarName='%s'\n",
        //        (int)mHasAvatar, mAvatarName.c_str());
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - mLastCheckTime).count() < mPollInterval)
        return;
    mLastCheckTime = now;

    const std::string jsonPath = mBasePath + "/avatar.json";
    // printf("[AVATAR HR] Checking %s...\n", jsonPath.c_str());
    if (!std::filesystem::exists(jsonPath)) {
        printf("[AVATAR HR] File does not exist\n");
        return;
    }

    auto writeTime = std::filesystem::last_write_time(jsonPath);
    // printf("[AVATAR HR] writeTime=%lld lastWriteTime=%lld diff=%lld\n",
    //        (long long)writeTime.time_since_epoch().count(),
    //        (long long)mLastWriteTime.time_since_epoch().count(),
    //        (long long)(writeTime.time_since_epoch().count() - mLastWriteTime.time_since_epoch().count()));
    if (writeTime == mLastWriteTime) {
        // printf("[AVATAR HR] No change detected\n");
        return;
    }
    mLastWriteTime = writeTime;

    printf("[AVATAR HR] CHANGE DETECTED! Reloading...\n");
    Terminal::instance().addLog("[AVATAR] Hot reload detected for: " + mAvatarName);
    loadAvatar(mAvatarName);

    extern Player* gpPlayer;
    if (gpPlayer) {
        printf("[AVATAR HR] Calling applyToPlayer...\n");
        applyToPlayer(*gpPlayer, true);
        printf("[AVATAR HR] applyToPlayer done\n");
    } else {
        printf("[AVATAR HR] gpPlayer is null\n");
    }

    // ── UV atlas PNG hot reload ──────────────────────────────────────
    if (mAvatar.textureMode == "uv_atlas" && !mAvatar.atlasPath.empty()) {
        if (!std::filesystem::exists(mAvatar.atlasPath)) return;
        auto atlasTime = std::filesystem::last_write_time(mAvatar.atlasPath);
        if (atlasTime != mAtlasLastWriteTime) {
            mAtlasLastWriteTime = atlasTime;
            printf("[AVATAR HR] Atlas PNG changed! Reloading texture...\n");
            Terminal::instance().addLog("[AVATAR] Atlas PNG hot reload: " + mAvatar.atlasPath);

            int w = 0, h = 0, n = 0;
            unsigned char* data = stbi_load(mAvatar.atlasPath.c_str(), &w, &h, &n, 4);
            if (data && mUvAtlasTexture != 0) {
                glBindTexture(GL_TEXTURE_2D, mUvAtlasTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                mAtlasWidth = w;
                mAtlasHeight = h;
                stbi_image_free(data);
                printf("[AVATAR HR] Atlas texture updated (%dx%d)\n", w, h);
            } else {
                if (data) stbi_image_free(data);
                printf("[AVATAR HR ERROR] Failed to reload atlas PNG\n");
            }
        }
    }
}

// ── Apply single texture (replaces OutfitAtlas) ─────────────────────
bool AvatarSystem::applySingleTexture(Player& player, const std::string& texturePath, bool reloadTexture)
{
    GLuint texture = gTextures.getPath(texturePath, reloadTexture);
    if (!texture) return false;

    for (size_t partIndex = 0; partIndex < player.physicalBody.partMeshes.size(); ++partIndex) {
        auto& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty()) continue;

        glm::vec3 mn = mesh.verts[0].pos, mx = mesh.verts[0].pos;
        for (auto& v : mesh.verts) { mn = glm::min(mn, v.pos); mx = glm::max(mx, v.pos); }
        for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3) {
            glm::vec3 normal = mesh.verts[i].normal + mesh.verts[i+1].normal + mesh.verts[i+2].normal;
            if (glm::dot(normal, normal) < 0.000001f)
                normal = glm::cross(mesh.verts[i+1].pos - mesh.verts[i].pos, mesh.verts[i+2].pos - mesh.verts[i].pos);
            glm::vec3 a = glm::abs(normal);
            int col = (a.y >= a.x && a.y >= a.z) ? (normal.y >= 0 ? 0 : 1) :
                      (a.z >= a.x) ? (normal.z >= 0 ? 2 : 3) :
                      (normal.x <= 0 ? 4 : 5);
            glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
            for (size_t v = i; v < i + 3; ++v) {
                auto& vert = mesh.verts[v];
                glm::vec2 uv;
                if (col <= 1)
                    uv = {(vert.pos.x - mn.x) / size.x, (vert.pos.y - mn.y) / size.y};
                else if (col <= 3)
                    uv = {(vert.pos.x - mn.x) / size.x, (vert.pos.z - mn.z) / size.z};
                else
                    uv = {(vert.pos.y - mn.y) / size.y, (vert.pos.z - mn.z) / size.z};
                if (col == 1 || col == 3 || col == 5) uv.x = 1.0f - uv.x;
                vert.uv = glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
            }
        }
        for (auto& batch : mesh.batches)
            batch.texture = texture;
    }
    player.bodyPartMeshes = player.physicalBody.partMeshes;
    return true;
}
