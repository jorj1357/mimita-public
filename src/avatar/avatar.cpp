#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "entities/player.h"
#include "world/texture-store.h"
#include "devtools/terminal.h"
#include "config/player-settings.h"

using json = nlohmann::json;

extern TextureStore gTextures;

// AvatarPartFaces
std::string& AvatarPartFaces::byName(const std::string& name) {
    if (name == "front") return front;
    if (name == "back") return back;
    if (name == "left") return left;
    if (name == "right") return right;
    if (name == "top") return top;
    return bottom;
}

const std::string& AvatarPartFaces::byName(const std::string& name) const {
    if (name == "front") return front;
    if (name == "back") return back;
    if (name == "left") return left;
    if (name == "right") return right;
    if (name == "top") return top;
    return bottom;
}

// FaceSettings
FaceSettings& PartFaceSettings::byName(const std::string& name) {
    if (name == "front") return front;
    if (name == "back") return back;
    if (name == "left") return left;
    if (name == "right") return right;
    if (name == "top") return top;
    return bottom;
}

const FaceSettings& PartFaceSettings::byName(const std::string& name) const {
    if (name == "front") return front;
    if (name == "back") return back;
    if (name == "left") return left;
    if (name == "right") return right;
    if (name == "top") return top;
    return bottom;
}

// AvatarDefinition
void AvatarDefinition::expandSimple() {
    auto fill = [](AvatarPartFaces& part, const std::string& all) {
        part.front = all;
        part.back = all;
        part.left = all;
        part.right = all;
        part.top = all;
        part.bottom = all;
    };
    fill(head, simple.skin);
    head.front = simple.face.empty() ? simple.skin : simple.face;
    fill(torso, simple.shirt);
    fill(leftArm, simple.shirt);
    fill(rightArm, simple.shirt);
    fill(leftLeg, simple.pants);
    fill(rightLeg, simple.pants);
}

std::string AvatarDefinition::resolve(const std::string& part, const std::string& face) const {
    auto getPart = [&]() -> const AvatarPartFaces* {
        if (part == "head") return &head;
        if (part == "torso") return &torso;
        if (part == "leftArm") return &leftArm;
        if (part == "rightArm") return &rightArm;
        if (part == "leftLeg") return &leftLeg;
        if (part == "rightLeg") return &rightLeg;
        return nullptr;
    };
    const AvatarPartFaces* p = getPart();
    if (!p) return {};
    return p->byName(face);
}

void AvatarDefinition::clear() {
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
    activePreset.clear();
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
    return {
        {"offsetX", t.offsetX},
        {"offsetY", t.offsetY},
        {"scale", t.scale},
        {"rotation", t.rotation},
        {"stretchMode", t.stretchMode},
        {"hue", t.hue},
        {"saturation", t.saturation},
        {"brightness", t.brightness},
        {"contrast", t.contrast}
    };
}

static FaceTransform parseTransform(const json& j) {
    FaceTransform t;
    t.offsetX = j.value("offsetX", 0.0f);
    t.offsetY = j.value("offsetY", 0.0f);
    t.scale = j.value("scale", 1.0f);
    t.rotation = j.value("rotation", 0.0f);
    t.stretchMode = j.value("stretchMode", 0);
    t.hue = j.value("hue", 0.0f);
    t.saturation = j.value("saturation", 1.0f);
    t.brightness = j.value("brightness", 1.0f);
    t.contrast = j.value("contrast", 1.0f);
    return t;
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

static json serializeCosmetics(const std::vector<CosmeticSlot>& cosmetics) {
    json arr = json::array();
    for (auto& c : cosmetics)
        arr.push_back({{"slot", c.slot}, {"choice", c.choice}});
    return arr;
}

static std::vector<CosmeticSlot> parseCosmetics(const json& arr) {
    std::vector<CosmeticSlot> result;
    if (!arr.is_array()) return result;
    for (auto& j : arr)
        result.push_back({j.value("slot", ""), j.value("choice", "")});
    return result;
}

// ── Load / Save ─────────────────────────────────────────────────────
bool AvatarSystem::loadAvatar(const std::string& avatarName) {
    mAvatarName = avatarName;
    mBasePath = avatarPath(avatarName);
    mAvatar.clear();
    mAvatar.name = avatarName;
    mAvatar.basePath = mBasePath;

    const std::string jsonPath = mBasePath + "/avatar.json";
    printf("[AVATAR] Loading avatar: %s\n", avatarName.c_str());
    printf("[AVATAR] Path: %s\n", jsonPath.c_str());

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        Terminal::instance().addLog("[AVATAR] No avatar.json found at " + jsonPath + "; using defaults");
        mHasAvatar = true;
        return true;
    }

    try {
        json root;
        file >> root;

        // Detect format: new (textures + faces) vs old (simple or advanced)
        bool hasNewFormat = root.contains("textures") && root.contains("faces") && root["textures"].is_object() && root["faces"].is_object();
        bool hasOldSimple = root.contains("simple") && root["simple"].is_object();
        bool hasOldAdvanced = root.contains("advanced") && root["advanced"].is_object();

        if (hasNewFormat) {
            printf("[AVATAR] Format: mimita-avatar v1 (new alias-based)\n");

            // Build texture alias map: alias -> resolved file path
            std::unordered_map<std::string, std::string> aliasMap;
            for (auto& [alias, val] : root["textures"].items()) {
                std::string filename = val.get<std::string>();
                std::string resolved = mBasePath + "/" + filename;
                aliasMap[alias] = filename; // store relative filename for AvatarPartFaces
                printf("[AVATAR]   Alias %s -> %s (%s)\n", alias.c_str(), filename.c_str(),
                       std::filesystem::exists(resolved) ? "exists" : "MISSING");
                if (!std::filesystem::exists(resolved))
                    Terminal::instance().addLog("[AVATAR] Missing texture: " + resolved);
            }

            // Resolve face assignments: alias -> filename
            const char* partKeys[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
            const char* faceKeys[] = {"front", "back", "left", "right", "top", "bottom"};
            AvatarPartFaces AvatarDefinition::*partPtrs[] = {
                &AvatarDefinition::head, &AvatarDefinition::torso,
                &AvatarDefinition::leftArm, &AvatarDefinition::rightArm,
                &AvatarDefinition::leftLeg, &AvatarDefinition::rightLeg
            };

            for (int pi = 0; pi < 6; ++pi) {
                std::string partKey = partKeys[pi];
                if (!root["faces"].contains(partKey) || !root["faces"][partKey].is_object()) {
                    printf("[AVATAR]   %s: no faces defined\n", partKey.c_str());
                    continue;
                }
                for (int fi = 0; fi < 6; ++fi) {
                    std::string faceKey = faceKeys[fi];
                    std::string alias;
                    auto& faceObj = root["faces"][partKey];
                    if (faceObj.is_object() && faceObj.contains(faceKey)) {
                        auto& val = faceObj[faceKey];
                        if (val.is_string())
                            alias = val.get<std::string>();
                        else if (val.is_object() && val.contains("texture"))
                            alias = val["texture"].get<std::string>();
                    }
                    if (alias.empty()) continue;

                    auto it = aliasMap.find(alias);
                    std::string filename = (it != aliasMap.end()) ? it->second : alias;
                    printf("[AVATAR]     %s.%s -> %s -> %s\n", partKey.c_str(), faceKey.c_str(),
                           alias.c_str(), filename.c_str());

                    // Check if file actually exists
                    std::string fullPath = mBasePath + "/" + filename;
                    if (!std::filesystem::exists(fullPath)) {
                        printf("[AVATAR]     WARNING: file not found: %s\n", fullPath.c_str());
                    }

                    // Store resolved filename in AvatarPartFaces
                    (mAvatar.*partPtrs[pi]).byName(faceKey) = filename;
                }
            }

            printf("[AVATAR] Loaded new-format avatar: %s\n", avatarName.c_str());
        } else if (hasOldSimple || hasOldAdvanced) {
            printf("[AVATAR] Format: legacy (simple/advanced)\n");

            if (hasOldSimple) {
                auto& s = root["simple"];
                mAvatar.simple.face = s.value("face", "");
                mAvatar.simple.shirt = s.value("shirt", "");
                mAvatar.simple.pants = s.value("pants", "");
                mAvatar.simple.skin = s.value("skin", "");
                printf("[AVATAR]   simple: face=%s shirt=%s pants=%s skin=%s\n",
                       mAvatar.simple.face.c_str(), mAvatar.simple.shirt.c_str(),
                       mAvatar.simple.pants.c_str(), mAvatar.simple.skin.c_str());
            }

            mAvatar.advancedMode = root.value("advanced_mode", false);

            if (hasOldAdvanced) {
                auto readPart = [&](const std::string& p, AvatarPartFaces& part) {
                    part.front = root["advanced"].value(p + "_front", "");
                    part.back = root["advanced"].value(p + "_back", "");
                    part.left = root["advanced"].value(p + "_left", "");
                    part.right = root["advanced"].value(p + "_right", "");
                    part.top = root["advanced"].value(p + "_top", "");
                    part.bottom = root["advanced"].value(p + "_bottom", "");
                };
                readPart("head", mAvatar.head);
                readPart("torso", mAvatar.torso);
                readPart("leftArm", mAvatar.leftArm);
                readPart("rightArm", mAvatar.rightArm);
                readPart("leftLeg", mAvatar.leftLeg);
                readPart("rightLeg", mAvatar.rightLeg);
            }

            if (!mAvatar.advancedMode)
                mAvatar.expandSimple();
        } else {
            printf("[AVATAR] ERROR: avatar.json has no recognized format\n");
            Terminal::instance().addLog("[AVATAR] Unrecognized format in " + jsonPath);
            mHasAvatar = true;
            return false;
        }

        // Shared fields for both formats
        if (root.contains("colors"))
            mAvatar.colors = parsePartColors(root["colors"]);
        if (root.contains("cosmetics"))
            mAvatar.cosmetics = parseCosmetics(root["cosmetics"]);
        mAvatar.activePreset = root.value("active_preset", "");

        mHasAvatar = true;
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
        root["advanced"][key] = val;
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
    auto writePart = [&](const std::string& p, const AvatarPartFaces& part) {
        root["advanced"][p + "_front"] = part.front;
        root["advanced"][p + "_back"] = part.back;
        root["advanced"][p + "_left"] = part.left;
        root["advanced"][p + "_right"] = part.right;
        root["advanced"][p + "_top"] = part.top;
        root["advanced"][p + "_bottom"] = part.bottom;
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

void AvatarSystem::setPartColor(const std::string& part, const glm::vec3& color) {
    if (part == "head") mAvatar.colors.head = color;
    else if (part == "torso") mAvatar.colors.torso = color;
    else if (part == "leftArm") mAvatar.colors.leftArm = color;
    else if (part == "rightArm") mAvatar.colors.rightArm = color;
    else if (part == "leftLeg") mAvatar.colors.leftLeg = color;
    else if (part == "rightLeg") mAvatar.colors.rightLeg = color;
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

    // Save all part face textures and transforms
    json adv = json::object();
    auto writePart = [&](const std::string& p, const AvatarPartFaces& part) {
        adv[p + "_front"] = part.front;
        adv[p + "_back"] = part.back;
        adv[p + "_left"] = part.left;
        adv[p + "_right"] = part.right;
        adv[p + "_top"] = part.top;
        adv[p + "_bottom"] = part.bottom;
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
            auto readPart = [&](const std::string& p, AvatarPartFaces& part) {
                part.front = root["advanced"].value(p + "_front", "");
                part.back = root["advanced"].value(p + "_back", "");
                part.left = root["advanced"].value(p + "_left", "");
                part.right = root["advanced"].value(p + "_right", "");
                part.top = root["advanced"].value(p + "_top", "");
                part.bottom = root["advanced"].value(p + "_bottom", "");
            };
            readPart("head", mAvatar.head);
            readPart("torso", mAvatar.torso);
            readPart("leftArm", mAvatar.leftArm);
            readPart("rightArm", mAvatar.rightArm);
            readPart("leftLeg", mAvatar.leftLeg);
            readPart("rightLeg", mAvatar.rightLeg);
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
        if (entry.is_directory())
            result.push_back(entry.path().filename().string());
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

void AvatarSystem::setPartFace(const std::string& part, const std::string& face, const std::string& texturePath) {
    auto getPart = [&]() -> AvatarPartFaces* {
        if (part == "head") return &mAvatar.head;
        if (part == "torso") return &mAvatar.torso;
        if (part == "leftArm") return &mAvatar.leftArm;
        if (part == "rightArm") return &mAvatar.rightArm;
        if (part == "leftLeg") return &mAvatar.leftLeg;
        if (part == "rightLeg") return &mAvatar.rightLeg;
        return nullptr;
    };
    AvatarPartFaces* p = getPart();
    if (p) p->byName(face) = texturePath;
}

bool AvatarSystem::applyToPlayer(Player& player, bool reloadTextures) {
    if (!mHasAvatar || mAvatarName.empty()) return false;

    if (!buildAtlas(player, reloadTextures))
        return false;

    return applyAtlasToPlayer(player);
}

void AvatarSystem::pollHotReload() {
    if (!mHasAvatar || mAvatarName.empty()) return;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - mLastCheckTime).count() < mPollInterval)
        return;
    mLastCheckTime = now;

    const std::string jsonPath = mBasePath + "/avatar.json";
    if (!std::filesystem::exists(jsonPath))
        return;

    auto writeTime = std::filesystem::last_write_time(jsonPath);
    if (writeTime == mLastWriteTime)
        return;
    mLastWriteTime = writeTime;

    Terminal::instance().addLog("[AVATAR] Hot reload detected for: " + mAvatarName);
    loadAvatar(mAvatarName);

    extern Player* gpPlayer;
    if (gpPlayer)
        applyToPlayer(*gpPlayer, true);
}

// ── Apply single texture (replaces OutfitAtlas) ─────────────────────
bool AvatarSystem::applySingleTexture(Player& player, const std::string& texturePath, bool reloadTexture)
{
    GLuint texture = gTextures.getPath(texturePath, reloadTexture);
    if (!texture) return false;

    for (size_t partIndex = 0; partIndex < player.physicalBody.partMeshes.size(); ++partIndex) {
        auto& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty()) continue;

        // Compute per-triangle UVs (same as atlas but without atlas lookup)
        glm::vec3 mn = mesh.verts[0].pos, mx = mesh.verts[0].pos;
        for (auto& v : mesh.verts) { mn = glm::min(mn, v.pos); mx = glm::max(mx, v.pos); }
        for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3) {
            glm::vec3 normal = mesh.verts[i].normal + mesh.verts[i+1].normal + mesh.verts[i+2].normal;
            if (glm::dot(normal, normal) < 0.000001f)
                normal = glm::cross(mesh.verts[i+1].pos - mesh.verts[i].pos, mesh.verts[i+2].pos - mesh.verts[i].pos);
            // Use faceColumnForNormal from avatar-atlas.cpp via a local copy
            glm::vec3 a = glm::abs(normal);
            int col = (a.z >= a.x && a.z >= a.y) ? (normal.z >= 0 ? 4 : 5) :
                      (a.y >= a.x) ? (normal.y <= 0 ? 0 : 1) : (normal.x <= 0 ? 2 : 3);
            glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
            for (size_t v = i; v < i + 3; ++v) {
                auto& vert = mesh.verts[v];
                glm::vec2 uv;
                if (col == 4 || col == 5)
                    uv = {(vert.pos.x - mn.x) / size.x, (vert.pos.y - mn.y) / size.y};
                else if (col <= 1)
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
