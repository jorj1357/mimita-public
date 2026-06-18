#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "entities/player.h"
#include "world/texture-store.h"
#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "map/map_common.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

using json = nlohmann::json;

extern TextureStore gTextures;

namespace {

constexpr int ATLAS_SIZE = 2000;
constexpr int CELL_SIZE = 313;
constexpr int GAP = 8;
constexpr int PADDING = 40;
constexpr int INSET = 2;
constexpr int USABLE = CELL_SIZE - INSET * 2;

int partRow(const std::string& name) {
    if (name == "head") return 0;
    if (name == "torso") return 1;
    if (name == "leftArm") return 2;
    if (name == "rightArm") return 3;
    if (name == "leftLeg") return 4;
    if (name == "rightLeg") return 5;
    return -1;
}

int faceColumn(glm::vec3 normal) {
    glm::vec3 a = glm::abs(normal);
    if (a.z >= a.x && a.z >= a.y) return normal.z >= 0.0f ? 0 : 1;
    if (a.y >= a.x) return normal.y <= 0.0f ? 2 : 3;
    return normal.x <= 0.0f ? 4 : 5;
}

const char* faceName(int column) {
    static const char* names[] = {"top", "bottom", "front", "back", "left", "right"};
    return names[std::clamp(column, 0, 5)];
}

glm::vec2 projectedUV(const Vertex& vertex, int face, glm::vec3 mn, glm::vec3 mx) {
    glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
    glm::vec2 uv;
    if (face <= 1)
        uv = {(vertex.pos.x - mn.x) / size.x, (vertex.pos.y - mn.y) / size.y};
    else if (face <= 3)
        uv = {(vertex.pos.x - mn.x) / size.x, (vertex.pos.z - mn.z) / size.z};
    else
        uv = {(vertex.pos.y - mn.y) / size.y, (vertex.pos.z - mn.z) / size.z};
    if (face == 1 || face == 3 || face == 5)
        uv.x = 1.0f - uv.x;
    return glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
}

glm::vec2 atlasUV(int row, int column, glm::vec2 local) {
    float x = (float)(PADDING + column * (CELL_SIZE + GAP) + INSET);
    float y = (float)(PADDING + row * (CELL_SIZE + GAP) + INSET);
    float usable = (float)USABLE;
    return {(x + local.x * usable) / (float)ATLAS_SIZE,
            (y + local.y * usable) / (float)ATLAS_SIZE};
}

int faceIndexForName(const std::string& name) {
    if (name == "top") return 0;
    if (name == "bottom") return 1;
    if (name == "front") return 2;
    if (name == "back") return 3;
    if (name == "left") return 4;
    if (name == "right") return 5;
    return -1;
}

}

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

bool AvatarSystem::buildAtlas(Player& player, bool reloadTextures) {
    // Expand simple mode to per-face assignments
    if (!mAvatar.advancedMode)
        const_cast<AvatarDefinition&>(mAvatar).expandSimple();

    // Create 2000x2000 RGBA buffer, clear to neutral gray
    std::vector<unsigned char> atlasPixels(ATLAS_SIZE * ATLAS_SIZE * 4, 128);
    std::vector<unsigned char> scaled(USABLE * USABLE * 4);

    auto blitToCell = [&](int row, int col, const std::string& path) {
        if (path.empty()) return;
        std::string fullPath = resolvePath(path);
        if (!std::filesystem::exists(fullPath)) {
            Terminal::instance().addLog("[AVATAR] Missing texture: " + fullPath);
            return;
        }

        int w, h, n;
        unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 4);
        if (!data) {
            Terminal::instance().addLog("[AVATAR] Failed to load: " + fullPath);
            return;
        }

        // Scale to USABLE x USABLE
        if (w != USABLE || h != USABLE) {
            stbir_resize_uint8_linear(data, w, h, 0, scaled.data(), USABLE, USABLE, 0, STBIR_RGBA);
        } else {
            std::memcpy(scaled.data(), data, USABLE * USABLE * 4);
        }
        stbi_image_free(data);

        // Blit into atlas
        int cellX = PADDING + col * (CELL_SIZE + GAP) + INSET;
        int cellY = PADDING + row * (CELL_SIZE + GAP) + INSET;
        for (int y = 0; y < USABLE; ++y) {
            std::memcpy(
                &atlasPixels[((cellY + y) * ATLAS_SIZE + cellX) * 4],
                &scaled[y * USABLE * 4],
                USABLE * 4
            );
        }
    };

    // Fill each body part x face cell
    const std::string parts[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    const std::string faces[] = {"top", "bottom", "front", "back", "left", "right"};
    AvatarPartFaces AvatarDefinition::*partPtrs[] = {
        &AvatarDefinition::head, &AvatarDefinition::torso,
        &AvatarDefinition::leftArm, &AvatarDefinition::rightArm,
        &AvatarDefinition::leftLeg, &AvatarDefinition::rightLeg
    };

    for (int pi = 0; pi < 6; ++pi) {
        const AvatarPartFaces& part = mAvatar.*partPtrs[pi];
        for (int fi = 0; fi < 6; ++fi) {
            blitToCell(pi, fi, part.byName(faces[fi]));
        }
    }

    // Upload atlas to GPU
    if (mAtlasTexture) {
        glDeleteTextures(1, &mAtlasTexture);
        mAtlasTexture = 0;
    }

    glGenTextures(1, &mAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, mAtlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    Terminal::instance().addLog("[AVATAR] Built atlas texture for: " + mAvatarName);
    return true;
}

bool AvatarSystem::applyAtlasToPlayer(Player& player) {
    if (!mAtlasTexture) return false;

    // Apply UV remapping to each body part mesh (same logic as OutfitAtlas)
    for (size_t partIndex = 0; partIndex < player.physicalBody.parts.size(); ++partIndex) {
        if (partIndex >= player.physicalBody.partMeshes.size())
            break;
        const std::string& name = player.physicalBody.parts[partIndex].name;
        const int row = partRow(name);
        if (row < 0) continue;

        Mesh& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty()) continue;

        glm::vec3 mn = mesh.verts[0].pos;
        glm::vec3 mx = mesh.verts[0].pos;
        for (const Vertex& v : mesh.verts) {
            mn = glm::min(mn, v.pos);
            mx = glm::max(mx, v.pos);
        }

        for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3) {
            glm::vec3 normal = mesh.verts[i].normal + mesh.verts[i + 1].normal + mesh.verts[i + 2].normal;
            if (glm::dot(normal, normal) < 0.000001f)
                normal = glm::cross(mesh.verts[i + 1].pos - mesh.verts[i].pos,
                                    mesh.verts[i + 2].pos - mesh.verts[i].pos);
            const int face = faceColumn(normal);
            for (size_t v = i; v < i + 3; ++v)
                mesh.verts[v].uv = atlasUV(row, face, projectedUV(mesh.verts[v], face, mn, mx));
        }

        for (Mesh::Batch& batch : mesh.batches)
            batch.texture = mAtlasTexture;
    }

    player.bodyPartMeshes = player.physicalBody.partMeshes;
    Terminal::instance().addLog("[AVATAR] Applied atlas to player");
    return true;
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
