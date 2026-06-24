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

bool AvatarSystem::loadAvatar(const std::string& avatarName) {
    mAvatarName = avatarName;
    mBasePath = avatarPath(avatarName);
    mAvatar.clear();
    mAvatar.name = avatarName;
    mAvatar.basePath = mBasePath;

    const std::string jsonPath = mBasePath + "/avatar.json";
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        Terminal::instance().addLog("[AVATAR] No avatar.json found at " + jsonPath + "; using defaults");
        mHasAvatar = true;
        return true;
    }

    try {
        json root;
        file >> root;

        json s = root.value("simple", json::object());
        mAvatar.simple.face = s.value("face", "");
        mAvatar.simple.shirt = s.value("shirt", "");
        mAvatar.simple.pants = s.value("pants", "");
        mAvatar.simple.skin = s.value("skin", "");

        mAvatar.advancedMode = root.value("advanced_mode", false);

        auto readPart = [&](const std::string& partName, AvatarPartFaces& part) {
            std::string p = partName;
            part.front = root["advanced"].value(p + "_front", "");
            part.back = root["advanced"].value(p + "_back", "");
            part.left = root["advanced"].value(p + "_left", "");
            part.right = root["advanced"].value(p + "_right", "");
            part.top = root["advanced"].value(p + "_top", "");
            part.bottom = root["advanced"].value(p + "_bottom", "");
        };

        if (root.contains("advanced")) {
            readPart("head", mAvatar.head);
            readPart("torso", mAvatar.torso);
            readPart("leftArm", mAvatar.leftArm);
            readPart("rightArm", mAvatar.rightArm);
            readPart("leftLeg", mAvatar.leftLeg);
            readPart("rightLeg", mAvatar.rightLeg);
        }

        if (!mAvatar.advancedMode)
            mAvatar.expandSimple();

        mHasAvatar = true;
        // Initialize hot-reload tracker to avoid false reload on first frame
        const std::string jsonPathResolved = mBasePath + "/avatar.json";
        if (std::filesystem::exists(jsonPathResolved))
            mLastWriteTime = std::filesystem::last_write_time(jsonPathResolved);
        mLastCheckTime = std::chrono::steady_clock::now();
        Terminal::instance().addLog("[AVATAR] Loaded avatar: " + avatarName);
        return true;
    } catch (const std::exception& e) {
        Terminal::instance().addLog("[AVATAR] Failed to parse avatar.json: " + std::string(e.what()));
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

    // Expand simple to advanced
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

    const std::string tmp = base + "/avatar.json.tmp";
    std::ofstream out(tmp);
    if (!out.is_open()) return false;
    out << root.dump(2);
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmp, base + "/avatar.json", ec);
    return !ec;
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
